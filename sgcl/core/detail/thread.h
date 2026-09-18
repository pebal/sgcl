//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "object_allocator.h"
#include "object_pool_allocator.h"
#include "os.h"

#include <cstdio>
#include <exception>
#include <thread>

#if SGCL_LOG_PRINT_LEVEL >= 3
#include <iostream>
#endif

namespace sgcl::detail {
    class Thread;
    // This thread's Thread, set by its constructor, null before: the one
    // thread-local of the hot paths (current_thread, the registration test
    // of every tracked_ptr constructor). A pointer with a constant
    // initializer and no destructor is read without the guard a
    // thread_local with a constructor carries; the Thread itself is a
    // function-local thread_local (current_thread), destroyed at thread
    // exit like before. Stays set after that: no re-registration from
    // thread_local destructors.
    inline bool thread_registered() noexcept {
        return current_thread_ptr != nullptr;
    }
    // The stack of the thread, [begin, begin + size), set at registration
    // and zero before it: one thread-local for an address test without the
    // guard of current_thread() (ptr.h).
    struct ThreadStack {
        uintptr_t begin = 0;
        uintptr_t size = 0;
        bool holds(const void* p) const noexcept {
            return (uintptr_t)p - begin < size;
        }
    };
    inline thread_local ThreadStack thread_stack;
    // True on a thread while it sweeps garbage: the destructors it runs are
    // those of objects that die together with everything reachable only
    // from them, in no order (containers.h: a container inside such an
    // object leaves its nodes to the sweep).
    inline thread_local bool sweeping = false;

    // A registered thread: its allocators, one per type it has allocated,
    // its page cache, and the Data the collector reads (the stack range,
    // the hazard pointer, the pages it published), which outlives the
    // thread until the collector has picked it up. Made on the thread's
    // first contact with the library (register_thread), destroyed when
    // the thread exits.
    class Thread : public ThreadHead {
    public:
        // A line of its own: the hazard pointer is written at every atomic
        // operation of the thread, and the Data of two threads would
        // otherwise share a line.
        struct alignas(config::CacheLineSize) Data {
            Data(PageAllocator* p) noexcept
            : page_allocator(p) {
            }
            std::unique_ptr<PageAllocator> page_allocator;
            // The whole stack of the thread, [stack_begin, stack_end); the
            // collector scans its used pages for roots.
            uintptr_t stack_begin = 0;
            uintptr_t stack_end = 0;
            std::thread::id id;   // the thread, for the diagnostics that name a stack (collector.h: referrers)
            // Handshake at thread exit: the collector raises stack_scan for
            // the time it reads the stack and skips a thread that is exiting;
            // the thread raises exiting and waits for stack_scan to drop
            // before its stack goes away. Both sides use seq_cst so that at
            // least one of them sees the other's flag.
            std::atomic<bool> exiting = {false};
            std::atomic<bool> stack_scan = {false};
            std::atomic<bool> is_deleted = {false};
            Data* next = {nullptr};
            Data* next_registered = {nullptr};
            std::atomic<Page*> pages = {nullptr};   // mutator pushes, collector exchanges
            RawPointer hazard_pointer = {nullptr};
            // Raised by this thread inside a barrier's slow path (types.h:
            // BarrierRegion); the collector waits for it to drop after the
            // flip (collector.h: _wait_barriers)
            std::atomic<uint32_t> in_barrier = {0};
        };

        Thread()
        : _page_allocator(new PageAllocator)
        , _data(new Data{_page_allocator}) {
#if SGCL_LOG_PRINT_LEVEL >= 3
            std::cout << "[sgcl] start thread id: " << std::this_thread::get_id() << std::endl;
#endif
            if (!os::thread_stack(_data->stack_begin, _data->stack_end)) {
                std::fprintf(stderr, "[sgcl] cannot determine the stack range of a thread\n");
                std::terminate();
            }
            thread_stack = {_data->stack_begin, _data->stack_end - _data->stack_begin};
            _data->id = std::this_thread::get_id();
            barrier_word = &_data->in_barrier;
            current_thread_ptr = this;   // stays set: no re-registration from thread_local destructors
            _data->next = threads_data.load(std::memory_order_acquire);
            while(!threads_data.compare_exchange_weak(_data->next, _data, std::memory_order_release, std::memory_order_relaxed));
        }

        ~Thread() noexcept {
            // No more stack roots from here on: wait out a scan in progress,
            // the stack is about to disappear.
            _data->exiting.store(true, std::memory_order_seq_cst);
            while (_data->stack_scan.load(std::memory_order_seq_cst)) {
                std::this_thread::yield();
            }
            barrier_word = &dead_barrier_word;   // a barrier run late (a thread_local's destructor) raises a word the collector does not read: Data may be gone
            // The allocators return their pool slots and cached headers first;
            // is_deleted is this thread's last store, and from then on the
            // collector may take the remaining pages and delete Data.
            for (auto& a : _allocators) {
                a.reset();
            }
            _data->is_deleted.store(true, std::memory_order_release);
#if SGCL_LOG_PRINT_LEVEL >= 3
            std::cout << "[sgcl] stop thread id: " << std::this_thread::get_id() << std::endl;
#endif
        }

        // The thread's allocator for T (the pool's or the large objects',
        // by the size of T), made on first use
        template<class T>
        auto& alocator() noexcept {
            return _allocator<typename TypeInfo<T>::Allocator>();
        }

        // Whether p is on this thread's stack: the location check of the
        // debug assertions
        bool on_stack(const void* p) const noexcept {
            return (uintptr_t)p - _data->stack_begin < _data->stack_end - _data->stack_begin;
        }

        uintptr_t stack_begin() const noexcept {
            return _data->stack_begin;
        }

        // seq_cst on purpose: the hazard protocol publishes the pointer and
        // then reads the atomic again, and the read must not be performed
        // before the publication is visible, so both sides are seq_cst
        // (weak_ptr::lock, atomic_word.h: _load): on x86 a plain store may
        // be overtaken by the load, and seq_cst makes it an xchg; on arm64
        // the store is stlr either way, and the load must be ldar, since
        // an acquire load is ldapr since ARMv8.3, which may pass the stlr.
        void set_hazard_pointer(void* p) {
            _data->hazard_pointer.store(p, std::memory_order_seq_cst);
        }

        // The object is held by its tracked_ptr now (or was not taken)
        void clear_hazard_pointer() {
            _data->hazard_pointer.store(nullptr, std::memory_order_release);
        }

        inline static std::atomic<Data*> threads_data = {nullptr};
        inline static std::thread::id main_thread_id = {};
        inline static std::atomic<uint32_t> dead_barrier_word = {0};

    private:
        PageAllocator* const _page_allocator;
        std::array<std::unique_ptr<ObjectAllocatorBase>, config::MaxTypesNumber> _allocators;
        Data* const _data;

        // The allocators are an array indexed by the type's number, one
        // number per type for the process (_type_index), at most
        // config::MaxTypesNumber of them
        template<class Allocator>
        Allocator& _allocator() {
            auto& alocator = _allocators[_type_index<typename Allocator::ValueType>()];
            if (!alocator) {
                if constexpr(Allocator::IsPoolAllocator::value) {
                    alocator.reset(new Allocator(*_page_allocator, _data->pages));
                } else {
                    alocator.reset(new Allocator(_data->pages));
                }
            }
            return static_cast<Allocator&>(*alocator);
        }
        // The type's number: taken from the counter on the first use of the
        // type on any thread
        template<class T>
        inline static unsigned _type_index() {
            static const unsigned index = _next_type_index();
            return index;
        }

        static unsigned _next_type_index() {
            auto index = _type_counter++;
            if (index >= config::MaxTypesNumber) {
                // Was an assert: in release the next line would index past
                // _allocators. Not a recoverable condition for the caller.
                std::fprintf(stderr, "[sgcl] more than %zu managed types; raise config::MaxTypesNumber\n", config::MaxTypesNumber);
                std::terminate();
            }
            return index;
        }
        inline static std::atomic<unsigned> _type_counter = {0};
    };

    // The main thread's id, taken at static initialization: the Thread of
    // the main thread terminates the collector when it exits (~Thread).
    struct MainThreadDetector {
        MainThreadDetector() noexcept {
            Thread::main_thread_id = std::this_thread::get_id();
        }
    };

    inline static MainThreadDetector main_thread_detector;

    // This thread's Thread, made on the first call and destroyed when the
    // thread exits (the destructor unregisters it); its constructor sets
    // current_thread_ptr. The slow path of current_thread() and
    // ensure_thread_registered(): a function-local thread_local carries a
    // guard, read at every access, which the pointer spares the hot paths.
    // The main thread's exit is the process's: its thread_local destructors
    // run before the static ones, and a static destructor may use the
    // library (a global unique_ptr's object, whose destructor copies
    // pointers and makes objects: unique_ptr.md), so the main thread's
    // Thread is never destroyed. The collector is stopped here, its last
    // cycles scanning this stack as ever, and the Thread stays whole: its
    // allocators with their pages, its Data, which the collector would
    // delete once unregistered, and current_thread_ptr naming it; nothing
    // is reclaimed, the process ends. (Destroyed as any thread's, the
    // main thread's Thread left a pointer to a dead object with its Data
    // freed, and the static destructors ran on freed memory.)
    inline Thread& register_thread() noexcept {
        struct Holder {
            Thread* thread = new Thread;
            ~Holder() {
                if (std::this_thread::get_id() == Thread::main_thread_id) {
                    terminate_collector();
                } else {
                    delete thread;
                }
            }
        };
        static thread_local Holder holder;
        return *holder.thread;
    }

    inline Thread& current_thread() noexcept {
        auto thread = current_thread_ptr;
        if (thread) [[likely]] {
                return *static_cast<Thread*>(thread);
        }
        return register_thread();
    }

    // Lowest address to zero when clearing the stack below the caller:
    // `bytes` below here, never within config::StackGuardMargin of the end
    // of this thread's stack, and never below the pages the stack has
    // already touched (zeroing untouched pages would only add them to the
    // scan).
    SGCL_ALWAYS_INLINE uintptr_t stack_clear_limit(size_t bytes) noexcept {
        uintptr_t here = (uintptr_t)&here;
        auto floor = current_thread().stack_begin() + config::StackGuardMargin;
        auto limit = bytes < here ? here - bytes : 0;
        limit = limit < floor ? floor : limit;
        auto touched = os::lowest_touched(limit, here);
        return touched > limit ? touched : limit;
    }

    // A thread's stack is scanned for roots only once the thread is
    // registered, and a thread can hold a root without ever allocating (a
    // copy of a pointer read from a shared object), so constructing a
    // tracked_ptr on the stack registers the thread. One thread-local flag:
    // the check is a load and a predictable branch.
    inline void ensure_thread_registered() noexcept {
        if (!current_thread_ptr) [[unlikely]] {
            register_thread();
        }
    }
}
