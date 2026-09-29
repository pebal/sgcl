//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array.h"
#include "thread.h"
#include "type_info.h"
#include "unique_ptr.h"

#include <array>
#include <bit>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The construction of managed objects (make_tracked.h): a slot from
    // the thread's allocator for the type, the object constructed in it,
    // the slot handed to a UniquePtr. Default-initialized without
    // arguments (`new T`, not `new T()`): a trivial type, and a trivial
    // member of a class without a constructor of its own, keeps the
    // bytes the slot holds. Those are zero only on a page fresh from the
    // heap or given back to it (object_pool_allocator_base.h: _free); a
    // slot reused in its pool, or a range of pages, holds what its last
    // user left there, only the words at pointer offsets null (a
    // destroyed tracked_ptr stores a null, and a buffer of a type that
    // may hold tracked pointers is zeroed when it is issued). The
    // containers value-initialize their elements themselves (vector.h,
    // dynamic_array.h: _make_at).
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
        // In place, in memory the container obtained: zeroed for a type
        // that may hold tracked pointers (or holding a destroyed element,
        // whose pointer words are null again), as its last user left it
        // otherwise; without arguments `new T` (MakerBase above).
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
        // fills (a trivial type; the slot holds null words at the pointer
        // offsets and, elsewhere, zeros or what its last user left)
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

    // The size classes of the buffers up to a page, in bytes of elements
    // (past the 16-byte header, ArrayBase), each class the whole of its
    // slot: the header and the class are a multiple of 16, the header's
    // alignment, so that no padding of the slot is left out of the
    // capacity. Up to 752 about 1.5 times the last (buffer_small_classes,
    // a chain of comparisons, allocate_small below); above, the classes
    // that fill a page: 64, 48, 32, 24, 16, 12, 8, 6, 4, 3, 2 and 1 slots
    // of it, each the largest whose slot fits that many times (an octave
    // table). A buffer past the last class is a range of pages.
    inline constexpr std::array<size_t, 22> buffer_classes = {
        16, 32, 48, 64, 96, 144, 224, 336, 496, 752,
        1008, 1344, 2032, 2704, 4080, 5440, 8176, 10896, 16368, 21824, 32752, 65520};

    // How many of the classes are the small ones, and the largest of them
    inline constexpr size_t buffer_small_classes = 10;
    inline constexpr size_t buffer_small_class_limit = buffer_classes[buffer_small_classes - 1];

    // The index of the smallest class that holds `bytes`, for a size past
    // the small classes and up to the last: the octave of the slot size,
    // bit_width(bytes + 15), gives the first class that can hold a size of
    // the octave, and one comparison the class after it, which holds the
    // rest (checked with the classes, below)
    inline constexpr size_t buffer_octaves = 17;

    consteval std::array<unsigned char, buffer_octaves> buffer_octave_first() {
        std::array<unsigned char, buffer_octaves> first = {};
        for (size_t k = 0; k < buffer_octaves; ++k) {
            // the least bytes of the octave: bytes + 15 >= 2^(k - 1)
            size_t least = k ? (size_t(1) << (k - 1)) : 0;
            least = least > sizeof(ArrayBase) - 1 ? least - (sizeof(ArrayBase) - 1) : 0;
            size_t i = buffer_small_classes - 1;
            while (i + 1 < buffer_classes.size() && buffer_classes[i] < least) {
                ++i;
            }
            first[k] = (unsigned char)i;
        }
        return first;
    }

    inline constexpr auto buffer_octave_table = buffer_octave_first();

    constexpr size_t buffer_large_class_index(size_t bytes) noexcept {
        size_t i = buffer_octave_table[std::bit_width(bytes + sizeof(ArrayBase) - 1)];
        return i + (buffer_classes[i] < bytes);
    }

    // The classes as the allocators see them: sorted, each with the header
    // a multiple of 16 (the slot, sizeof(Array<class>), is then the header
    // and the class exactly), the last a whole page, and past the small
    // classes a page of slots of the class wastes at most 256 bytes; the
    // octave table finds the smallest class for every size from the small
    // classes' end up to the last
    template<size_t... I>
    consteval bool buffer_classes_valid(std::index_sequence<I...>) {
        constexpr size_t slots[] = {sizeof(Array<buffer_classes[I]>)...};
        for (size_t i = 0; i < buffer_classes.size(); ++i) {
            auto c = buffer_classes[i];
            if ((c + sizeof(ArrayBase)) % alignof(ArrayBase) || slots[i] != c + sizeof(ArrayBase) || (i && c <= buffer_classes[i - 1]) || slots[i] > PageDataSize) {
                return false;
            }
            if (i >= buffer_small_classes && PageDataSize % slots[i] > 256) {
                return false;
            }
        }
        if (slots[buffer_classes.size() - 1] != PageDataSize) {
            return false;
        }
        // the selection steps only where a class ends or an octave begins:
        // both sides of each such size checked
        auto finds_smallest = [](size_t bytes) {
            if (bytes <= buffer_small_class_limit || bytes > buffer_classes.back()) {
                return true;
            }
            size_t smallest = 0;
            while (buffer_classes[smallest] < bytes) {
                ++smallest;
            }
            return buffer_large_class_index(bytes) == smallest;
        };
        for (auto c : buffer_classes) {
            if (!finds_smallest(c) || !finds_smallest(c + 1)) {
                return false;
            }
        }
        for (size_t k = 1; k < buffer_octaves; ++k) {
            auto edge = size_t(1) << (k - 1);   // bytes + 15 == 2^(k - 1): the octave's first size
            if (edge >= sizeof(ArrayBase) && (!finds_smallest(edge - sizeof(ArrayBase)) || !finds_smallest(edge - sizeof(ArrayBase) + 1))) {
                return false;
            }
        }
        return true;
    }

    static_assert(buffer_classes_valid(std::make_index_sequence<buffer_classes.size()>()), "the buffer size classes do not fill their slots and pages");

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
        // bytes past it, from the thread's allocator for that class: the
        // address of its first element, the slot kept by its state
        // (UniqueLock, set by the allocator) until the caller hands it to a
        // UniquePtr, which nothing between the two can prevent
        template<class T>
        static void* _alloc(size_t data_size, const Header& header) {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocator.alloc(data_size, header);
            return ((Type*)mem)->data;
        }

        // A buffer of the size class Size for elements of ObjectSize bytes,
        // its capacity what the class holds: the capacity and the zeroing
        // are constants in it
        template<size_t Size, size_t ObjectSize, bool Zero>
        static void* _alloc_class(ArrayMetadata* metadata) {
            return _alloc<Array<Size>>(0, Header{metadata, Size / ObjectSize, ObjectSize, Zero});
        }

        // The classes of a page: a table of their functions, indexed by
        // buffer_large_class_index
        using AllocClass = void* (*)(ArrayMetadata*);

        template<size_t ObjectSize, bool Zero, size_t... I>
        static constexpr std::array<AllocClass, sizeof...(I)> _class_entries(std::index_sequence<I...>) {
            return {&_alloc_class<buffer_classes[I], ObjectSize, Zero>...};
        }

        // A buffer for at least `capacity` elements: from a pool of a size
        // class (the capacity rounded up to what the class holds) or, past
        // a page, from a range of pages; with `whole_pages`, the capacity
        // of a range rounded up to what its pages hold next to the header.
        // The range is whole pages anyway, and a container that grows by
        // its capacity (vector) then doubles into whole pages again: made
        // at a power of two of bytes past a page, which the header puts
        // onto a page more, it took 3, 5, 9 pages, and takes 2, 4, 8. The
        // others ask for what they index (a count of buckets, of blocks)
        // and would only have more slots to zero and to trace.
        // The small classes are a chain of comparisons, one class after
        // another from the smallest, inlined whole into the caller (for the
        // smallest buffers, the most frequent, a call through the table
        // costs a nanosecond more); past them the table of the classes of a
        // page, then the range.
        template<size_t ObjectSize, bool Zero, size_t I = 0>
        static UniquePtr<void> _make_array(size_t capacity, ArrayMetadata* metadata, bool whole_pages) {
            const size_t bytes = ObjectSize * capacity;
            if constexpr(I < buffer_small_classes) {
                if (bytes <= buffer_classes[I]) {
                    return UniquePtr<void>(_alloc_class<buffer_classes[I], ObjectSize, Zero>(metadata));
                }
                return _make_array<ObjectSize, Zero, I + 1>(capacity, metadata, whole_pages);
            } else {
                if (bytes <= buffer_classes.back()) {
                    static constexpr auto table = _class_entries<ObjectSize, Zero>(std::make_index_sequence<buffer_classes.size()>());
                    return UniquePtr<void>(table[buffer_large_class_index(bytes)](metadata));
                }
                if (whole_pages) {
                    auto pages = (bytes + sizeof(ArrayBase) + config::page_size - 1) / config::page_size;
                    capacity = (pages * config::page_size - sizeof(ArrayBase)) / ObjectSize;
                }
                return UniquePtr<void>(_alloc<Array<>>(ObjectSize * capacity + sizeof(ArrayBase) - sizeof(Array<>), Header{metadata, capacity, ObjectSize, Zero}));
            }
        }
    };

    // Managed buffers exist only for the containers (vector, dynamic_array,
    // the maps of deque and the buckets of the hash tables): raw storage
    // for `capacity` elements, none constructed, zeroed when the element
    // type may hold tracked pointers (a zeroed tracked_ptr is null, and the
    // collector traces every slot), as the slot or the pages were left
    // otherwise: a container that wants zeros writes them (vector.h,
    // dynamic_array.h: value-initialization). The pointer returned addresses the
    // first element; the deleter finds the buffer through the page like for
    // any interior pointer.
    template<class T>
    class Maker<T[]> : ArrayMaker {
        static_assert(alignof(T) <= alignof(ArrayBase), "array elements aligned beyond 16 bytes are not supported");
    public:
        // A buffer for `capacity` elements of T
        static UniquePtr<T> make_tracked_data(size_t capacity) {
            auto p = _make_array<sizeof(T), Info::MayContainTracked>(capacity, &Info::array_metadata(), false);
            return UniquePtr<T>((T*)p.release());
        }

        // A buffer for at least `capacity` elements of T, past a page as
        // many as its pages hold: for a container that reads the capacity
        // back from the header and grows from it (vector)
        static UniquePtr<T> make_tracked_data_in_whole_pages(size_t capacity) {
            auto p = _make_array<sizeof(T), Info::MayContainTracked>(capacity, &Info::array_metadata(), true);
            return UniquePtr<T>((T*)p.release());
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;
    };
}
