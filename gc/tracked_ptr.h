//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../sgcl/detail/cell_block.h"
#include "../sgcl/detail/heap.h"
#include "../sgcl/detail/thread.h"
#include "../sgcl/detail/unique_ptr.h"
#include "../sgcl/make_tracked.h"
#include "../sgcl/tracked_ptr.h"
#include "../sgcl/unique_ptr.h"

#include <cstdint>
#include <memory>

namespace sgcl::detail {
    // The thread's source of cells for the gc::tracked_ptrs in unmanaged
    // memory (cell_block.h): a block of a cache line of slots, handed out
    // in order, each zeroed as it goes. The block is a root by its unique
    // state while any slot of it may still be handed out; the last slot
    // handed out, or the thread ending, sets the block UniqueReleased (types.h),
    // with release after the last zeroing, and the collector frees it in
    // the cycle that finds every slot free again. The pointer's
    // destructor frees its slot on whatever thread it runs, so a slot
    // handed out here may be freed by another thread, and a block is held
    // by nothing but its slots. take() is called by a thread that is
    // registered (gc::tracked_ptr checks its stack first, which registers
    // it), on the tagged path only.
    struct CellAllocator {
        CellBlock* block = nullptr;
        unsigned index = 0;

        ~CellAllocator() noexcept {
            release();
        }

        Pointer* take() noexcept {
            if (!block) [[unlikely]] {
                block = make_tracked<CellBlock>().release();
                index = 0;
            }
            auto slot = (Pointer*)&block->slots[index];   // the word, a tracked word from here on
            slot->store(nullptr);
            if (++index == CellBlock::Slots) [[unlikely]] {
                release();
            }
            return slot;
        }

        // The block let go of: its state, after the slots handed out so far
        void release() noexcept {
            if (block) {
                Page::set_state<State::UniqueReleased>(block);
                block = nullptr;
            }
        }
    };

    inline thread_local CellAllocator cell_allocator;
}

namespace gc {
    // The sgcl::tracked_ptr that may live anywhere: one word. Inside a
    // managed object or on a stack it is an sgcl::tracked_ptr<T>, and
    // costs what one costs; in any other memory (new/malloc, a std
    // container, a global, a thread_local, a lambda copied to the heap,
    // the frame of a plain coroutine) it is the address of a cell, a
    // word of a managed block of a cache line of them (detail/cell_block.h)
    // that is a root by state, with the sign bit set. The cell is taken by the
    // constructor from the thread's allocator (detail::CellAllocator
    // below) and given back by the destructor, and belongs to this pointer
    // for the whole time between: no store ever allocates, so two threads storing
    // into the same pointer race on one atomic word, as they do on an
    // sgcl::tracked_ptr, and never on the making of a cell; no move ever
    // takes a cell from another pointer, so a thread reading through the
    // cell of a pointer another thread moves from reads a cell that lives
    // as long as its pointer. A move is a copy, as it is for
    // sgcl::tracked_ptr. The mode is fixed by the address of the pointer
    // when it is constructed (the heap's range, then the thread's stack)
    // and read back from the sign of the word, so no memory the collector
    // reads ever holds the tagged form: the pointer maps and the stack
    // scan see plain words. A read is a load and a test of the sign; a
    // construction in unmanaged memory is a slot from the thread's
    // allocator, one managed allocation per block, a null included.
    template<class T>
    class tracked_ptr {
    public:
        using element_type = T;
        // What a container stores in its managed memory in place of this
        // type (detail/managed.h): the word itself
        using tracked_type = sgcl::tracked_ptr<T>;

        tracked_ptr() noexcept {
            if (_tracked_here()) {
                new (&_ptr) sgcl::tracked_ptr<T>();
            } else {
                _make_cell();
            }
        }

        tracked_ptr(std::nullptr_t) noexcept
        : tracked_ptr() {
        }

        // From a raw pointer: under the rules of the raw constructor of
        // tracked_ptr (a managed object or a part of it, never an element
        // of a container's buffer, never an object a unique_ptr owns).
        template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p) noexcept {
            assert((!p || sgcl::detail::Page::is_object(p)) && "a tracked_ptr may address a managed object or a part of it, not an element of a container's buffer");
            assert(!p || !sgcl::detail::Page::is_unique(p));
            _construct(static_cast<element_type*>(p));
        }

        // An atomic load (sgcl/atomic.h, atomic_ref.h), on a registered
        // thread with the target under its hazard pointer: the word as
        // loaded, which is never an element of a buffer and never a unique
        // object (the released state is set before the word is stored).
        tracked_ptr(element_type* p, sgcl::detail::OnRegisteredThread) noexcept {
            assert(sgcl::detail::thread_registered);
            assert(!p || sgcl::detail::Page::is_object(p));
            if (_tracked_here()) {
                new (&_ptr) sgcl::tracked_ptr<T>(p, sgcl::detail::OnRegisteredThread{});
            } else {
                _make_cell();
                if (p) {
                    _store_cell(p);
                }
            }
        }

        // Copies take the source's word as it is, unchecked: it is a
        // tracked word already, and may address a container's buffer,
        // which the raw constructor would not accept. Always inline, the
        // copies and the moves: the compiler's own threshold leaves the
        // constructor out of line by the size of the caller (measured: a
        // copy onto the stack 2.0 or 3.6 ns, by the code beside the loop).
        SGCL_ALWAYS_INLINE tracked_ptr(const tracked_ptr& p) noexcept {
            _construct(p.get());
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        SGCL_ALWAYS_INLINE tracked_ptr(const tracked_ptr<U>& p) noexcept {
            _construct(static_cast<element_type*>(p.get()));
        }

        // A move is a copy, as for sgcl::tracked_ptr: the source keeps its
        // value and, in unmanaged memory, its cell (see above).
        SGCL_ALWAYS_INLINE tracked_ptr(tracked_ptr&& p) noexcept {
            _construct(p.get());
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        SGCL_ALWAYS_INLINE tracked_ptr(tracked_ptr<U>&& p) noexcept {
            _construct(static_cast<element_type*>(p.get()));
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        SGCL_ALWAYS_INLINE tracked_ptr(const sgcl::tracked_ptr<U>& p) noexcept {
            _construct(static_cast<element_type*>(p.get()));
        }

        // The object released from its owner: the barrier of the store
        // takes it out of the unique state, as in tracked_ptr.
        template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(sgcl::unique_ptr<U>&& u) noexcept {
            if (_tracked_here()) {
                new (&_ptr) sgcl::tracked_ptr<T>(std::move(u));
            } else {
                _make_cell();
                _cell_of()->store_released(u.release());
            }
        }

        ~tracked_ptr() noexcept {
            if (!_is_cell()) [[likely]] {
                std::destroy_at(&_ptr);
            } else {
                _release_cell();
            }
        }

        tracked_ptr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        tracked_ptr& operator=(const tracked_ptr& p) noexcept {
            _store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _store(static_cast<element_type*>(p.get()));
            return *this;
        }

        // A move is a copy (see above)
        tracked_ptr& operator=(tracked_ptr&& p) noexcept {
            _store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept {
            _store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(const sgcl::tracked_ptr<U>& p) noexcept {
            _store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename sgcl::unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(sgcl::unique_ptr<U>&& u) noexcept {
            if (!_is_cell()) [[likely]] {
                _ptr = std::move(u);
            } else {
                _cell_of()->store_released(u.release());
            }
            return *this;
        }

        explicit operator bool() const noexcept {
            return get() != nullptr;
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        element_type* get() const noexcept {
            auto w = _ptr.get();   // the word, whichever member holds it
            if (!_is_cell(w)) [[likely]] {
                return w;
            }
            return (element_type*)_cell_of()->load();
        }

        // For destructors, as tracked_ptr::if_alive: a copy of the pointer,
        // or null when its target dies in the same sweep.
        tracked_ptr if_alive() const noexcept {
            auto p = get();
            if (p && sgcl::detail::sweeping && sgcl::detail::Page::dying(p)) {
                return tracked_ptr();
            }
            return *this;
        }

        void reset() noexcept {
            if (!_is_cell()) [[likely]] {
                _ptr.reset();
            } else {
                _cell_of()->store(nullptr);
            }
        }

        // The pointer replaced by a raw one, under the rules of the raw
        // constructor.
        void reset(element_type* p) noexcept {
            assert((!p || sgcl::detail::Page::is_object(p)) && "a tracked_ptr may address a managed object or a part of it, not an element of a container's buffer");
            assert(!p || !sgcl::detail::Page::is_unique(p));
            _store(p);
        }

        void swap(tracked_ptr& p) noexcept {
            auto a = get();
            auto b = p.get();
            _store(b);
            p._store(a);
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        tracked_ptr<U> as() const noexcept {
            return tracked_ptr<U>(sgcl::tracked_ptr<element_type>(get()).template as<U>());
        }

        const std::type_info& type() const noexcept {
            return sgcl::detail::Pointer::type_info<element_type>(get());
        }

    private:
        static constexpr uintptr_t SignBit = uintptr_t(1) << (sizeof(uintptr_t) * 8 - 1);

        // Where this tracked_ptr lives, decided once, in the constructor: the
        // heap's range first (no thread-local access for a member of a
        // managed object), then the stack of the thread, which registers
        // the thread on first contact.
        bool _tracked_here() const noexcept {
            if (sgcl::detail::Heap::contains(this) || sgcl::detail::thread_stack.holds(this)) [[likely]] {
                return true;
            }
            sgcl::detail::ensure_thread_registered();   // an unregistered thread has no stack yet
            return sgcl::detail::thread_stack.holds(this);
        }

        static bool _is_cell(const void* w) noexcept {
            return (intptr_t)w < 0;
        }

        bool _is_cell() const noexcept {
            return (intptr_t)_cell < 0;
        }

        // The cell of a pointer in unmanaged memory, taken by _make_cell and
        // there for the life of the pointer: a word of a block (detail/cell_block.h)
        sgcl::detail::Pointer* _cell_of() const noexcept {
            assert(_is_cell());
            return (sgcl::detail::Pointer*)(_cell & ~SignBit);
        }

        // Out of line, both: inlined, the allocator and the release in
        // the cell branches of the constructors and the destructor cost
        // the other branch, the one of a tracked word (measured: a copy
        // onto the stack 1.8 -> 3.6 ns), and next to them the call is
        // nothing.
        SGCL_NOINLINE void _make_cell() noexcept {
            _cell = (uintptr_t)sgcl::detail::cell_allocator.take() | SignBit;
        }

        SGCL_NOINLINE void _release_cell() noexcept {
            auto cell = _cell_of();
            assert(!sgcl::detail::CellBlock::is_free(cell));
            cell->store_no_update(cell);   // free: its own address (detail/cell_block.h)
        }

        // The word stored, unchecked: the value is a tracked word already,
        // a container's buffer included (the public raw constructor and
        // reset(T*) check theirs first). In the tracked mode the
        // constructor of tracked_ptr without the check; in a cell the
        // barrier's store on the cell's word (detail::Pointer::store).
        // Always inline, both: called from every constructor and
        // assignment, so the compiler's own threshold leaves them out of
        // line, which costs a copy 1.7 ns.
        SGCL_ALWAYS_INLINE void _construct(element_type* p) noexcept {
            if (_tracked_here()) {
                new (&_ptr) sgcl::tracked_ptr<T>(p, sgcl::detail::Unchecked{});
            } else {
                _make_cell();
                if (p) {
                    _store_cell(p);
                }
            }
        }

        // The store into the cell's word, with its barrier (detail::Pointer:
        // store): the target becomes reachable, the cell's page is carded.
        // Never a check of the target: the callers copy words that are
        // tracked already (the public raw constructor and reset(T*) check
        // theirs first).
        void _store_cell(element_type* p) noexcept {
            _cell_of()->store(p);
        }

        SGCL_ALWAYS_INLINE void _store(element_type* p) noexcept {
            if (!_is_cell()) [[likely]] {
                _ptr._ptr()->store(p);
            } else {
                _store_cell(p);
            }
        }

        // The pointer read without an atomic load, as tracked_ptr::get_plain:
        // for the containers, on the pointer to their own buffer.
        element_type* get_plain() const noexcept {
            auto w = _ptr.get_plain();
            if (!_is_cell(w)) [[likely]] {
                return w;
            }
            return (element_type*)_cell_of()->load_plain();
        }

        // The word the atomics operate on (atomic.h, atomic_ref.h): the
        // tracked_ptr itself, or the cell's. A tracked_ptr<T> and a
        // tracked_ptr<void> are one word (tracked_ptr.h).
        sgcl::tracked_ptr<T>& _word() noexcept {
            if (!_is_cell()) {
                return _ptr;
            }
            return *(sgcl::tracked_ptr<T>*)_cell_of();
        }

        template<class> friend class tracked_ptr;
        template<class> friend class sgcl::atomic;
        template<class> friend class sgcl::atomic_ref;
        template<class, template<class> class> friend class sgcl::vector;
        template<class, size_t, template<class> class> friend struct sgcl::array;

        union {
            sgcl::tracked_ptr<T> _ptr;   // inside a managed object or on a stack
            uintptr_t _cell;       // elsewhere: a word of a managed detail::CellBlock, tagged
        };
    };

    template<class T>
    tracked_ptr(T*) -> tracked_ptr<T>;

    template<class T>
    tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;

    template<class T>
    tracked_ptr(sgcl::tracked_ptr<T>) -> tracked_ptr<T>;

    template<class T>
    tracked_ptr(sgcl::unique_ptr<T>&&) -> tracked_ptr<T>;

    template<class T, class U>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        using Y = typename std::common_type<decltype(l.get()), decltype(r.get())>::type;
        return static_cast<Y>(l.get()) <=> static_cast<Y>(r.get());
    }

    template<class T, class U>
    inline bool operator==(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        return static_cast<const void*>(l.get()) == static_cast<const void*>(r.get());
    }

    template<class T, class U>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, const sgcl::tracked_ptr<U>& r) noexcept {
        using Y = typename std::common_type<decltype(l.get()), decltype(r.get())>::type;
        return static_cast<Y>(l.get()) <=> static_cast<Y>(r.get());
    }

    template<class T, class U>
    inline bool operator==(const tracked_ptr<T>& l, const sgcl::tracked_ptr<U>& r) noexcept {
        return static_cast<const void*>(l.get()) == static_cast<const void*>(r.get());
    }

    template<class T>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return l.get() <=> static_cast<decltype(l.get())>(nullptr);
    }

    template<class T>
    inline bool operator==(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return l.get() == nullptr;
    }

    template<class T, class U>
    inline tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(static_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(const_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(dynamic_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p) {
        s << p.get();
        return s;
    }
}

namespace std {
    template<class T>
    struct hash<gc::tracked_ptr<T>> {
        std::size_t operator()(const gc::tracked_ptr<T>& p) const noexcept {
            return std::hash<T*>{}(p.get());
        }
    };
}
