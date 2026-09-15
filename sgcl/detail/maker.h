//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array.h"
#include "thread.h"
#include "type_info.h"
#include "unique_ptr.h"
#include <type_traits>

namespace sgcl::detail {
    // The construction of managed objects (make_tracked.h): a slot from
    // the thread's allocator for the type, the object constructed in it,
    // the slot handed to a UniquePtr. Value-initialized without
    // arguments (`new T`, not `new T()`: a trivial type stays as the slot
    // is, which is zero or a destroyed element's null words).
    class MakerBase {
    protected:
        template<class T, class ...A>
        static void _construct(void* p, A&&... a) {
            if constexpr(sizeof...(A)) {
                new(p) T(std::forward<A>(a)...);
            } else {
                new(p) T;
            }
        }

        // The allocator handed the slot out in state UniqueLock already.
        template<class T, class ...A>
        static void _construct_and_register(void* p, A&&... a) {
            try {
                _construct<T>(p, std::forward<A>(a)...);
            }
            catch (...) {
                Page::set_state<State::BadAlloc>(p);
                throw;
            }
        }
    };

    template<class T>
    class Maker : MakerBase {
    public:       
        // In place, in memory the container obtained zeroed (or holding a
        // destroyed element, whose pointer words are null again).
        template<class ...A>
        static void construct(void* p, A&&... a) {
            _construct<typename TypeInfo<T>::Type>(p, std::forward<A>(a)...);
        }

        // The element destroyed in place, by the container that owns it
        inline static void destroy(T* p) noexcept {
            if constexpr(!std::is_trivially_destructible_v<T> && std::is_destructible_v<T>) {
                std::destroy_at(p);
            }
        }

        // A new object of T, constructed from the arguments; a root through
        // the UniquePtr until it is handed to a tracked_ptr
        template<class ...A>
        static UniquePtr<T> make_tracked(A&&... a) {
            return _make(std::forward<A>(a)...);
        }

        // Constructed inside the allocator's init, before the slot is
        // published: the collector never sees the object half-written, nor
        // the words of the slot's last occupant. For the cells of weak
        // pointers (weak_cell.h) only: a slot published after its
        // construction is no root while the constructor runs, so a
        // constructor that stored a tracked_ptr into the object would leave
        // the target unrooted; a cell's word is not traced at all.
        template<class ...A>
        static UniquePtr<T> make_tracked_before_publish(A&&... a) {
            static_assert(!Info::MayContainTracked, "a type constructed before publication may not hold tracked pointers");
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(0, [&](void* p) {
                _construct<Type>(p, std::forward<A>(a)...);
            });
            return UniquePtr<T>((Type*)mem);
        }

        // A slot for a T without a construction: raw storage the caller
        // fills (a trivial type; the slot is zero or holds null words)
        template<class ...A>
        static UniquePtr<T> make_tracked_data() {
            return _make_data();
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;

        // The collector may read the words of the object once the slot is
        // published (state UniqueLock) and while it is being constructed:
        // a word at a pointer offset of the type must be null or its final
        // value, never a leftover of the slot's last object. A pool slot
        // (object_pool_allocator.h) needs nothing for that: its page was
        // zero when it was issued to the type (fresh from the heap, or
        // zeroed by the collector when it went back: object_pool_allocator_base.h,
        // _free), and every object of the type that died in the slot since
        // left its pointer words null, the destructor of tracked_ptr
        // storing a null (tracked_ptr.h); what is left at the other
        // offsets is data of the same type, which the map's elimination
        // classifies as it would the final value. A large object
        // (object_allocator.h) is zeroed in the allocator's `init`, before
        // the publication, which the release store of the state orders
        // before the collector's reads: its page range comes from the heap
        // as it was freed.
        static void _init(void* p) noexcept {
            if constexpr(Info::MayContainTracked && !Info::Allocator::IsPoolAllocator::value) {
                std::memset(p, 0, sizeof(T));
            }
        }

        // The slot from the thread's allocator, zeroed if it is a page range
        // (_init), the object constructed in it after the slot came out
        // (in state UniqueLock: the constructor's stores are barrier stores)
        template<class ...A>
        static UniquePtr<T> _make(A&&... a) {
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(0, _init);
            _construct_and_register<Type>(mem, std::forward<A>(a)...);
            return UniquePtr<T>((Type*)mem);
        }

        // A slot without a construction (make_tracked_data: raw storage)
        static UniquePtr<T> _make_data() {
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(0, _init);
            return UniquePtr<T>((Type*)mem);
        }
    };

    class ArrayMaker : MakerBase {
    protected:
        // The header (the element type's metadata, the capacity) is written
        // into the slot before the allocator publishes it, and the elements
        // are zeroed then when their type may hold tracked pointers: what
        // the collector reads of a buffer is complete by the time it can
        // see the buffer (array_base.h). Plain stores: the slot is free and
        // unseen here, the release store of its state orders them.
        struct Header {
            ArrayMetadata* metadata;
            size_t capacity;
            size_t object_size;
            bool zero;

            // the allocator's init: the header and the zeroing, before publication
            void operator()(void* p) const noexcept {
                auto array = (Array<>*)p;
                array->metadata = metadata;
                array->capacity = capacity;
                if (zero) {
                    std::memset(array->data, 0, object_size * capacity);
                }
            }
        };

        // A buffer of the size class T (Array<N>, below) with `data_size`
        // bytes past it, from the thread's allocator for that class
        template<class T>
        static UniquePtr<void> _make(size_t data_size, const Header& header) {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(data_size, header);
            return UniquePtr<void>(((Type*)mem)->data);
        }

        // A buffer for at least `capacity` elements: from a pool of a size
        // class (the capacity rounded up to what the class holds) or, past
        // a page, from a range of pages.
        template<size_t N = sizeof(uintptr_t)>
        static UniquePtr<void> _make_array(size_t capacity, size_t object_size, ArrayMetadata* metadata, bool zero) {
            if (object_size * capacity <= N && sizeof(Array<N>) <= PageDataSize) {
                return _make<Array<N>>(0, Header{metadata, N / object_size, object_size, zero});
            } else {
                if constexpr(sizeof(Array<N>) < PageDataSize) {
                    static constexpr auto Size = (N * 3 / 2 + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
                    return _make_array<Size>(capacity, object_size, metadata, zero);
                } else {
                    if (object_size * capacity <= PageDataSize - sizeof(ArrayBase)) {
                        return _make<Array<PageDataSize - sizeof(ArrayBase)>>(0, Header{metadata, (PageDataSize - sizeof(ArrayBase)) / object_size, object_size, zero});
                    } else {
                        return _make<Array<>>(object_size * capacity + sizeof(ArrayBase) - sizeof(Array<>), Header{metadata, capacity, object_size, zero});
                    }
                }
            }
        }
    };

    // Managed buffers exist only for the containers (sgcl::vector, array,
    // the maps of deque and the buckets of the hash tables): raw storage
    // for `capacity` elements, none constructed, zeroed when the element
    // type may hold tracked pointers (a zeroed tracked_ptr is null, and the
    // collector traces every slot). The pointer returned addresses the
    // first element; the deleter finds the buffer through the page like for
    // any interior pointer.
    template<class T>
    class Maker<T[]> : ArrayMaker {
        static_assert(alignof(T) <= alignof(ArrayBase), "array elements aligned beyond 16 bytes are not supported");
    public:
        // A buffer for `capacity` elements of T
        static UniquePtr<T> make_tracked_data(size_t capacity) {
            auto p = _make_array<>(capacity, sizeof(T), &Info::array_metadata(), Info::MayContainTracked);
            return UniquePtr<T>((T*)p.release());
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;
    };
}
