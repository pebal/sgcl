//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "object_allocator.h"
#include "object_pool_allocator.h"
#include "stack_pointer_allocator.h"

#include <thread>

#if SGCL_LOG_PRINT_LEVEL >= 3
#include <iostream>
#endif

namespace sgcl::detail {
    class Thread {
    public:
        struct Data {
            Data(BlockAllocator* b, StackPointerAllocator* s) noexcept
            : block_allocator(b)
            , stack_roots_allocator(s) {
            }
            std::unique_ptr<BlockAllocator> block_allocator;
            std::unique_ptr<StackPointerAllocator> stack_roots_allocator;
            std::atomic<bool> is_deleted = {false};
            bool is_used = {true};
            bool is_last_registered = {false};
            Data* next = {nullptr};
            Data* next_registered = {nullptr};
            std::atomic<Page*> pages = {nullptr};
            Page* last_page_registered = {nullptr};
        };

        struct ChildPointers {
            uintptr_t base;
            detail::ChildPointers::Map* map;
        };

        struct RangeGuard {
            RangeGuard(const RangeGuard&) = delete;
            void operator=(const RangeGuard&) = delete;

            constexpr RangeGuard(Thread& t, const ChildPointers& cp) noexcept
            : thread(t)
            , old_pointers(t.child_pointers) {
                thread.child_pointers = cp;
            }

            ~RangeGuard() noexcept {
                thread.child_pointers = old_pointers;
            }

            Thread& thread;
            const ChildPointers old_pointers;
        };

        Thread()
        : stack_allocator(new StackPointerAllocator)
        , _block_allocator(new BlockAllocator)
        , _data(new Data{_block_allocator, stack_allocator}) {
#if SGCL_LOG_PRINT_LEVEL >= 3
            std::cout << "[sgcl] start thread id: " << std::this_thread::get_id() << std::endl;
#endif
            _data->next = threads_data.load(std::memory_order_acquire);
            while(!threads_data.compare_exchange_weak(_data->next, _data, std::memory_order_release, std::memory_order_relaxed));
        }

        ~Thread() noexcept {
            _data->is_deleted.store(true, std::memory_order_release);
            if (std::this_thread::get_id() == main_thread_id) {
                for (auto& a : _allocators) {
                    a.reset();
                }
                terminate_collector();
            }
#if SGCL_LOG_PRINT_LEVEL >= 3
            std::cout << "[sgcl] stop thread id: " << std::this_thread::get_id() << std::endl;
#endif
        }

        template<class T>
        auto& alocator() noexcept {
            return _allocator<typename TypeInfo<T>::ObjectAllocator>();
        }

        RangeGuard use_child_pointers(const ChildPointers& cp) noexcept {
            return {*this, cp};
        }

        ChildPointers child_pointers = {0, nullptr};
        StackPointerAllocator* const stack_allocator;
        inline static std::atomic<Data*> threads_data = {nullptr};
        inline static std::thread::id main_thread_id = {};

    private:
        BlockAllocator* const _block_allocator;
        std::array<std::unique_ptr<ObjectAllocatorBase>, config::MaxTypesNumber> _allocators;
        Data* const _data;

        template<class Allocator>
        Allocator& _allocator() {
            auto& alocator = _allocators[_type_index<typename Allocator::ValueType>()];
            if (!alocator) {
                if constexpr(Allocator::IsPoolAllocator::value) {
                    alocator.reset(new Allocator(*_block_allocator, _data->pages));
                } else {
                    alocator.reset(new Allocator(_data->pages));
                }
            }
            return static_cast<Allocator&>(*alocator);
        }
        template<class T>
        inline static unsigned _type_index() {
            static const unsigned index = _type_counter++;
            assert(_type_counter < config::MaxTypesNumber);
            return index;
        }
        inline static std::atomic<unsigned> _type_counter = {0};
    };

    struct MainThreadDetector {
        MainThreadDetector() noexcept {
            Thread::main_thread_id = std::this_thread::get_id();
        }
    };

    inline static MainThreadDetector main_thread_detector;

    inline Thread& current_thread() noexcept {
        static thread_local Thread instance;
        return instance;
    }
}
