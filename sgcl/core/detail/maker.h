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
    // bytes the slot holds: what its last user left there, an object of
    // the type or, on a page the type took from the heap, of another
    // (the heap does not zero a page given back: object_pool_allocator_base.h,
    // _free); zeros only on a page fresh from the system. For a type
    // that may hold tracked pointers the words at its pointer offsets
    // are null (a page such a type takes from the heap is zeroed then:
    // object_pool_allocator_base.h, _next_page; a destroyed tracked_ptr
    // stores a null; a large object and a buffer of such a type are
    // zeroed when they are issued). The containers value-initialize
    // their elements themselves (vector.h, dynamic_array.h: _make_at).
    class MakerBase {
    public:
        // Whether constructing a T from A cannot throw: the constructor's
        // own noexcept, looked at from here, so that a type with private
        // constructors that befriends MakerBase is seen as it is
        // (std::is_nothrow_constructible sees no private constructor and
        // says false, and a noexcept caller got a terminate stub for it)
        template<class T, class... A>
        static constexpr bool nothrow_constructible = noexcept(::new (static_cast<void*>(nullptr)) T(std::declval<A>()...));

        // Every managed allocation of the library comes through here (the
        // makers below, the strings: string_data.h). It never fails: running
        // out of managed memory is handled where it can happen, the page
        // allocator's refill and the large object's range (page_allocator.h,
        // object_allocator.h): a full collection, one more try, then the end
        // of the program (heap.h: out_of_managed_memory, DESIGN 356). Nothing
        // here checks, so the fast path is the allocator's.
        template<class Allocator, class Init>
        SGCL_ALWAYS_INLINE static auto allocate(Allocator& allocator, size_t size, Init&& init) noexcept {
            return allocator.alloc(size, init);
        }

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
        static UniquePtr<T> make_tracked(A&&... a) noexcept(nothrow_constructible<Type, A...>) {
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
        static UniquePtr<T> make_tracked_before_publish(A&&... a) noexcept {
            static_assert(!Info::MayContainTracked, "a type constructed before publication may not hold tracked pointers");
            static_assert(nothrow_constructible<Type, A...>, "a type constructed before publication is constructed inside the allocator, which is noexcept");
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocate(allocator, 0, [&](void* p) noexcept {
                _construct<Type>(p, std::forward<A>(a)...);
            });
            return UniquePtr<T>((Type*)mem);
        }

        // A slot for a T without a construction: raw storage the caller
        // fills (a trivial type; the slot holds null words at the pointer
        // offsets and, elsewhere, what its last user left, of this type or
        // of another: MakerBase above)
        template<class ...A>
        static UniquePtr<T> make_tracked_data() noexcept {
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
        // zeroed when the type took it from the heap, on the allocating
        // thread before the page was published (object_pool_allocator_base.h,
        // _next_page: for a type whose pointer map still has an offset; a
        // map that reads empty is never read, and never fills again), and
        // every object of the type that died in the slot since
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
        static UniquePtr<T> _make(A&&... a) noexcept(nothrow_constructible<Type, A...>) {
            auto& thread = current_thread();
            SGCL_TSAN_RELEASE(thread.barrier_word);   // as a barrier's end does (types.h: BarrierRegion)
            auto& allocator = thread.alocator<Type>();
            auto mem = allocate(allocator, 0, _init);
            _construct_and_register<Type>(mem, std::forward<A>(a)...);
            return UniquePtr<T>((Type*)mem);
        }

        // A slot without a construction (make_tracked_data: raw storage)
        static UniquePtr<T> _make_data() noexcept {
            auto& thread = current_thread();
            SGCL_TSAN_RELEASE(thread.barrier_word);   // as a barrier's end does (types.h: BarrierRegion)
            auto& allocator = thread.alocator<Type>();
            auto mem = allocate(allocator, 0, _init);
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

    // The size classes as data: every class its own pool (its own
    // Metadata, pages and slab, as a type of its own), described by a row
    // of constants, its buffers made by one function for all classes and
    // element types (ArrayMaker::_alloc_class; a function per class, element
    // size and zeroing was code for each, and the classes of a page all of
    // it for every element size used). The type of a class's objects stays
    // Array<class> for the statistics.
    template<size_t Size>
    constexpr TypeConstants buffer_class_constants_of() noexcept {
        using Info = TypeInfo<Array<Size>>;
        static_assert(Info::Allocator::IsPoolAllocator::value && !Info::MayContainTracked && !Info::get_destroy_function());
        return Metadata::constants_of<Array<Size>>();
    }

    template<size_t... I>
    constexpr std::array<TypeConstants, sizeof...(I)> buffer_class_constants_of(std::index_sequence<I...>) noexcept {
        return {buffer_class_constants_of<buffer_classes[I]>()...};
    }

    inline constexpr auto buffer_class_constants = buffer_class_constants_of(std::make_index_sequence<buffer_classes.size()>());

    // The most elements of `object_size` bytes a buffer may be asked for:
    // their bytes with the header, rounded up to whole pages, still fit in
    // size_t (bytes + sizeof(ArrayBase) <= 2^N - page_size), so no
    // sum of _make_array or the range's allocator wraps. A capacity past it
    // is a buffer no memory holds: the program ends as when the heap
    // refuses one (heap.h: out_of_managed_memory).
    constexpr size_t buffer_max_capacity(size_t object_size) noexcept {
        return (size_t(-1) - sizeof(ArrayBase) - (config::page_size - 1)) / object_size;
    }

    // The pools of the classes: per class its number among the allocators
    // of every thread (thread.h: pool_allocator; 0 until its first use) and
    // its Metadata, made on the first use of the class on any thread
    // (Metadata::of_pool). Both constinit: no guard and no order of
    // initialization. Each on cache lines of its own: the slots are read
    // by every buffer's allocation, and placed after the collector's
    // counters, which it writes every cycle (memory_counters.h), they
    // shared a line with them (the classes of a page measured 4-9% slower).
    struct BufferPools {
        alignas(config::cache_line_size) inline static constinit std::atomic<unsigned> slots[buffer_classes.size()] = {};
        alignas(config::cache_line_size) inline static constinit std::atomic<Metadata*> metadata[buffer_classes.size()] = {};
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

        // A buffer past a page (Array<>, the allocator of a range) with
        // `data_size` bytes past it: the address of its first element, the
        // range kept by its state (UniqueLock, set by the allocator) until
        // the caller hands it to a UniquePtr, which nothing between the two
        // can prevent
        template<class T>
        static void* _alloc(size_t data_size, const Header& header) noexcept {
            using Info = TypeInfo<T>;
            using Type = typename Info::Type;
            auto& thread = current_thread();
            auto& allocator = thread.alocator<Type>();
            auto mem = allocate(allocator, data_size, header);
            return ((Type*)mem)->data;
        }

        // A buffer of the size class c (buffer_classes) for `capacity`
        // elements of `object_size` bytes, what the class holds, from the
        // thread's allocator for the class; kept as _alloc's. One body for
        // every class and element type, the header's fields in registers
        // (a Header passed by reference went through the stack). The small
        // classes' chain calls it with constants and the compiler may
        // inline it there (as it did the function of each class: for the
        // smallest buffers a call costs a nanosecond more); the classes of
        // a page call it out of line (_alloc_page_class).
        static void* _alloc_class(unsigned c, ArrayMetadata* metadata, size_t capacity, size_t object_size, bool zero) noexcept {
            auto& allocator = current_thread().pool_allocator(BufferPools::slots[c], [c]() -> Metadata& {
                return Metadata::of_pool(BufferPools::metadata[c], buffer_class_constants[c]);
            });
            auto mem = allocate(allocator, 0, Header{metadata, capacity, object_size, zero});
            return ((Array<>*)mem)->data;
        }

        // The class c of a page (found by buffer_large_class_index), one
        // function for all of them
        SGCL_NOINLINE static void* _alloc_page_class(unsigned c, ArrayMetadata* metadata, size_t capacity, size_t object_size, bool zero) noexcept {
            return _alloc_class(c, metadata, capacity, object_size, zero);
        }

        // Zeroed when the element type may hold tracked pointers and its
        // map still has a pointer offset: a map emptied by elimination
        // (child_pointers.h; it only loses offsets) proves the elements
        // hold none, and the collector, which reads the elements only
        // while the map has an offset (collector.h: _mark_array_childs),
        // never reads this buffer's leftovers. One relaxed load per buffer.
        template<bool Zero>
        static bool _zero(const ArrayMetadata* metadata) noexcept {
            if constexpr(Zero) {
                return metadata->child_pointers.any.load(std::memory_order_relaxed);
            } else {
                return false;
            }
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
        // smallest buffers, the most frequent, a call through a table of
        // the classes cost a nanosecond more); past them the class of a
        // page found by buffer_large_class_index (a direct call), then the
        // range. A capacity past buffer_max_capacity ends the program, one
        // comparison with a constant before the chain.
        template<size_t ObjectSize, bool Zero, size_t I = 0>
        static UniquePtr<void> _make_array(size_t capacity, ArrayMetadata* metadata, bool whole_pages) noexcept {
            if constexpr(I == 0) {
                if (capacity > buffer_max_capacity(ObjectSize)) [[unlikely]] {
                    out_of_managed_memory();
                }
            }
            const size_t bytes = ObjectSize * capacity;
            if constexpr(I < buffer_small_classes) {
                if (bytes <= buffer_classes[I]) {
                    return UniquePtr<void>(_alloc_class(I, metadata, buffer_classes[I] / ObjectSize, ObjectSize, _zero<Zero>(metadata)));
                }
                return _make_array<ObjectSize, Zero, I + 1>(capacity, metadata, whole_pages);
            } else {
                if (bytes <= buffer_classes.back()) {
                    auto c = buffer_large_class_index(bytes);
                    return UniquePtr<void>(_alloc_page_class(unsigned(c), metadata, buffer_classes[c] / ObjectSize, ObjectSize, _zero<Zero>(metadata)));
                }
                if (whole_pages) {
                    auto pages = (bytes + sizeof(ArrayBase) + config::page_size - 1) / config::page_size;
                    capacity = (pages * config::page_size - sizeof(ArrayBase)) / ObjectSize;
                }
                return UniquePtr<void>(_alloc<Array<>>(ObjectSize * capacity + sizeof(ArrayBase) - sizeof(Array<>), Header{metadata, capacity, ObjectSize, _zero<Zero>(metadata)}));
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
        static UniquePtr<T> make_tracked_data(size_t capacity) noexcept {
            auto p = _make_array<sizeof(T), Info::MayContainTracked>(capacity, &Info::array_metadata(), false);
            return UniquePtr<T>((T*)p.release());
        }

        // A buffer for at least `capacity` elements of T, past a page as
        // many as its pages hold: for a container that reads the capacity
        // back from the header and grows from it (vector)
        static UniquePtr<T> make_tracked_data_in_whole_pages(size_t capacity) noexcept {
            auto p = _make_array<sizeof(T), Info::MayContainTracked>(capacity, &Info::array_metadata(), true);
            return UniquePtr<T>((T*)p.release());
        }

    private:
        using Info = TypeInfo<T>;
        using Type = typename Info::Type;
    };
}
