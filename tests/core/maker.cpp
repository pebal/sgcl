//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <array>
#include <thread>
#include <tuple>

TEST(Maker_Tests, DefaultConstructor) {
    struct S {
        char value = 2;
    };
    auto ptr = make_tracked<S>();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 2);
    auto tr = make_tracked<tracked_ptr<int>>();
    EXPECT_NE(tr, nullptr);
    EXPECT_EQ(*tr, nullptr);
}

TEST(Maker_Tests, ParameterConstructor) {
    auto ptr = make_tracked<int>(3);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 3);
}

// Managed arrays are internal to the containers; their construction paths
// are exercised through sgcl::vector (tests/vector.cpp).
TEST(Maker_Tests, InternalArrayThroughVector) {
    struct S {
        char value = 9;
    };
    sgcl::vector<S> s(3);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s[2].value, 9);
    sgcl::vector<tracked_ptr<int>> tr(3);
    EXPECT_EQ(tr[0], nullptr);
    EXPECT_EQ(tr[2], nullptr);
    sgcl::vector<int> big(7000, 5);
    EXPECT_EQ(big.size(), 7000u);
    for (size_t i = 0; i < big.size(); ++i) {
        if (big[i] != 5) {
            FAIL() << "element " << i;
        }
    }
    sgcl::vector<Foo> foo(3, 7);
    EXPECT_EQ(foo[2].get_value(), 7);
}

// The size classes of the buffers (detail/maker.h): up to 752 bytes each
// about 1.5 times the last, above it the classes that fill a page of 64 KB
// in 64, 48, 32, 24, 16, 12, 8, 6, 4, 3, 2 and 1 slots, the header (16
// bytes) inside the slot, and every class the whole of its slot (the
// class and the header a multiple of 16). A buffer of n bytes gets the
// smallest of them that holds n, and past the last a range of pages of
// exactly n. The classes grown by 1.5 all the way up (1120 ... 43152,
// 64728) wasted up to 22 KB of every page (43152: one slot of 43168
// bytes to a page), and the classes 8, 24, 40, 216, 328, 744 left 8 bytes
// of their slot out of the capacity (8 and 16 shared a slot of 32).
static_assert(sizeof(detail::Array<2704>) == 2720 && sizeof(detail::Array<10896>) == 10912 && sizeof(detail::Array<752>) == 768);

namespace {
    // The capacity a buffer of n bytes gets, and the class the test
    // expects: the smallest that holds n, n itself past the last
    size_t class_capacity(size_t n) {
        auto p = detail::Maker<std::byte[]>::make_tracked_data(n);
        return ((detail::ArrayBase*)p.get() - 1)->capacity;
    }

    size_t expected_class(size_t n) {
        static const size_t classes[] = {16, 32, 48, 64, 96, 144, 224, 336, 496, 752,
            1008, 1344, 2032, 2704, 4080, 5440, 8176, 10896, 16368, 21824, 32752, 65520};
        for (auto c : classes) {
            if (c >= n) {
                return c;
            }
        }
        return n;
    }
}

TEST(Maker_Tests, ABufferGetsTheSmallestClassThatHoldsIt) {
#if defined(SGCL_ASAN)
    GTEST_SKIP() << "under the address sanitizer a buffer is the capacity asked for, in a class with a redzone (maker.h)";
#endif
    size_t checked = 0;
    for (size_t n = 1; n <= 70000; n += 7) {
        auto capacity = class_capacity(n);
        ASSERT_GE(capacity, n) << "bytes " << n;
        ASSERT_EQ(capacity, expected_class(n)) << "bytes " << n;
        if (capacity <= config::page_size - sizeof(detail::ArrayBase)) {
            // the whole slot: the class and the header a multiple of the
            // header's alignment, sizeof(Array<class>) exactly
            ASSERT_EQ((capacity + sizeof(detail::ArrayBase)) % alignof(detail::ArrayBase), 0u) << "class " << capacity;
            if (capacity > 752) {
                // a page of such slots wastes at most 256 bytes
                ASSERT_LE(config::page_size % (capacity + sizeof(detail::ArrayBase)), 256u) << "class " << capacity;
            }
        }
        ++checked;
    }
    EXPECT_EQ(checked, 10000u);
    for (size_t n = 0; n <= 1100; ++n) {   // every size of the small classes and the first of a page
        ASSERT_EQ(class_capacity(n), expected_class(n == 0 ? 1 : n)) << "bytes " << n;
    }
}


// Every size class of the buffers at its boundary, for elements of 1, 4, 8
// and 16 bytes (and of 16 with a tracked pointer, zeroed): the largest
// count a class holds stays in it, one more element goes to the next class
// (past the last, a range of pages). Each class is one pool for every
// element size and type: its slot, its count per page and its Metadata
// the same whoever asks. Four threads make the first buffers of every
// class at once (a class's pool is made on its first use).
namespace {
    struct Elem16 {
        long a, b;
    };

    struct TrackedElem16 {
        tracked_ptr<int> p;
        long b;
    };

    constexpr size_t class_sizes[] = {16, 32, 48, 64, 96, 144, 224, 336, 496, 752,
        1008, 1344, 2032, 2704, 4080, 5440, 8176, 10896, 16368, 21824, 32752, 65520};

    // The class a buffer came from, by its page: the slot and its count
    // per page, the pool's Metadata
    struct BufferPlace {
        size_t capacity;
        const detail::Metadata* metadata;
        size_t page_count;
    };

    template<class T>
    BufferPlace place_of(size_t n, bool whole_pages = false) {
        auto p = whole_pages ? detail::Maker<T[]>::make_tracked_data_in_whole_pages(n) : detail::Maker<T[]>::make_tracked_data(n);
        auto base = (detail::ArrayBase*)p.get() - 1;
        EXPECT_EQ(base->metadata, &detail::TypeInfo<T>::array_metadata());
        if (detail::TypeInfo<T>::MayContainTracked) {
            for (size_t i = 0; i < base->capacity * sizeof(T); ++i) {
                if (((unsigned char*)p.get())[i]) {
                    ADD_FAILURE() << "byte " << i << " of a buffer of " << n << " elements of " << sizeof(T) << " bytes is not zero";
                    break;
                }
            }
        }
        auto page = detail::Page::page_of(p.get());
        return {base->capacity, page->metadata, page->page_count};
    }

    template<class T>
    void check_class_boundaries(const detail::Metadata* (&pools)[std::size(class_sizes)]) {
        constexpr size_t E = sizeof(T);
        for (size_t c = 0; c < std::size(class_sizes); ++c) {
            const size_t bytes = class_sizes[c];
            if (bytes < E) {
                continue;
            }
            const size_t n = bytes / E;   // the most the class holds
            for (bool whole : {false, true}) {
                auto in = place_of<T>(n, whole);
                EXPECT_EQ(in.capacity, n) << "class " << bytes << ", element " << E;
                EXPECT_EQ(in.metadata->object_size, bytes + sizeof(detail::ArrayBase)) << "class " << bytes << ", element " << E;
                EXPECT_EQ(in.metadata->object_count, config::page_size / (bytes + sizeof(detail::ArrayBase))) << "class " << bytes;
                EXPECT_TRUE(in.metadata->is_array && in.metadata->pool_allocated);
                EXPECT_FALSE(in.metadata->is_string);
                EXPECT_EQ(in.metadata->destroy, nullptr);
                EXPECT_EQ(in.metadata->free, &detail::ObjectPoolAllocatorBase::free_pool_pages);
                EXPECT_NE(std::string(in.metadata->type_info.name()).find("Array"), std::string::npos);
                EXPECT_EQ(in.metadata->child_pointers.map.size(), 0u);
                if (!pools[c]) {
                    pools[c] = in.metadata;
                }
                EXPECT_EQ(in.metadata, pools[c]) << "class " << bytes << ", element " << E << ": one pool per class";

                // one element more: the next class, or past the last a range
                auto next = place_of<T>(n + 1, whole);
                if (c + 1 < std::size(class_sizes)) {
                    EXPECT_EQ(next.capacity, class_sizes[c + 1] / E) << "class " << bytes << " + 1, element " << E;
                    EXPECT_EQ(next.metadata->object_size, class_sizes[c + 1] + sizeof(detail::ArrayBase));
                    if (pools[c + 1]) {
                        EXPECT_EQ(next.metadata, pools[c + 1]);
                    }
                } else {
                    const size_t range = (n + 1) * E + sizeof(detail::ArrayBase);
                    const size_t pages = (range + config::page_size - 1) / config::page_size;
                    EXPECT_EQ(next.capacity, whole ? (pages * config::page_size - sizeof(detail::ArrayBase)) / E : n + 1) << "element " << E;
                    EXPECT_FALSE(next.metadata->pool_allocated);
                    EXPECT_EQ(next.metadata->object_count, 1u);
                    EXPECT_EQ(next.page_count, pages);
                    EXPECT_EQ(next.metadata, &detail::TypeInfo<detail::Array<>>::private_metadata());
                }
            }
        }
    }
}

TEST(Maker_Tests, EveryClassBoundaryForEveryElementSize) {
#if defined(SGCL_ASAN)
    GTEST_SKIP() << "under the address sanitizer a buffer is the capacity asked for, in a class with a redzone (maker.h)";
#endif
    auto first = make_tracked<int>(1);   // the heap made on this thread first
    std::atomic<int> ready = {0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            ready.fetch_add(1);
            while (ready.load() < 4) {
            }
            for (auto bytes : class_sizes) {
                auto p = detail::Maker<std::byte[]>::make_tracked_data(bytes);
                EXPECT_EQ(((detail::ArrayBase*)p.get() - 1)->capacity, bytes);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    const detail::Metadata* pools[std::size(class_sizes)] = {};
    check_class_boundaries<std::byte>(pools);
    check_class_boundaries<int>(pools);
    check_class_boundaries<long>(pools);
    check_class_boundaries<Elem16>(pools);
    check_class_boundaries<TrackedElem16>(pools);
    for (auto m : pools) {
        EXPECT_NE(m, nullptr);
    }
    EXPECT_EQ(pools[0]->type_info, typeid(detail::Array<16>));
    EXPECT_EQ(pools[9]->type_info, typeid(detail::Array<752>));
    EXPECT_EQ(pools[21]->type_info, typeid(detail::Array<65520>));
}

// Types of the race below and of nothing else: their first allocation in
// the process is the one the test races. Of different sizes, so that an
// object made in another type's pool shows in its size as well as in its
// type.
namespace {
    constexpr int RacedTypes = 16;
    constexpr int RacingThreads = 8;
    std::atomic<int> raced_alive = {0};

    template<int N>
    struct Raced {
        Raced(int t, int i) : thread(t), index(i) {
            raced_alive.fetch_add(1, std::memory_order_relaxed);
        }
        ~Raced() {
            raced_alive.fetch_sub(1, std::memory_order_relaxed);
        }
        int thread;
        int index;
        char pad[8 * (N + 1)] = {};
        tracked_ptr<Raced> next;
    };

    template<class Seq>
    struct RacedHeadsOf;

    template<int... N>
    struct RacedHeadsOf<std::integer_sequence<int, N...>> {
        using type = std::tuple<tracked_ptr<Raced<N>>...>;
    };

    using RacedHeads = RacedHeadsOf<std::make_integer_sequence<int, RacedTypes>>::type;

    // What one thread saw of one type: its Metadata, its pointer map, and
    // the Metadata the page of its last object names
    struct RacedSeen {
        const detail::Metadata* metadata = nullptr;
        const detail::ChildPointers* pointers = nullptr;
        const detail::Metadata* page_metadata = nullptr;
    };

    // Objects `from` .. `to` - 1 of type N, pushed on the thread's chain
    // of the type; what the thread sees of the type is taken after the
    // first of them (the race) and checked after the last
    template<int N>
    SGCL_NOINLINE bool make_raced(int t, int from, int to, RacedHeads& heads, RacedSeen& seen) {
        using Info = detail::TypeInfo<Raced<N>>;
        auto& head = std::get<N>(heads);
        for (int i = from; i < to; ++i) {
            tracked_ptr<Raced<N>> p = make_tracked<Raced<N>>(t, i);
            p->next = head;
            head = p;
        }
        if (from == 0) {
            seen.metadata = &Info::private_metadata();
            seen.pointers = &Info::child_pointers();
            seen.page_metadata = detail::Page::page_of(head.get())->metadata;
            return true;
        }
        return detail::Page::page_of(head.get())->metadata == seen.metadata && &Info::child_pointers() == seen.pointers;
    }

    template<int N>
    SGCL_NOINLINE bool check_raced(int t, int count, RacedHeads& heads) {
        int i = count;
        for (auto p = std::get<N>(heads); p; p = p->next) {
            --i;
            if (p->thread != t || p->index != i || p.type() != typeid(Raced<N>)
                || detail::Pointer::object_size(p.get()) != sizeof(Raced<N>)) {
                return false;
            }
        }
        return i == 0;
    }

    using MakeRaced = bool (*)(int, int, int, RacedHeads&, RacedSeen&);
    using CheckRaced = bool (*)(int, int, RacedHeads&);

    template<int... N>
    constexpr std::array<MakeRaced, sizeof...(N)> make_raced_table(std::integer_sequence<int, N...>) {
        return {&make_raced<N>...};
    }

    template<int... N>
    constexpr std::array<CheckRaced, sizeof...(N)> check_raced_table(std::integer_sequence<int, N...>) {
        return {&check_raced<N>...};
    }
}

// Many threads released at once onto many types never used before, each
// thread making objects of all of them in an order of its own: the first
// object of a type makes the type's Metadata and pointer map
// (page_info.h, metadata.h) and numbers it (thread.h), all three raced.
// The orders are n = k * step mod 16 with a step odd and different per
// thread: every thread starts on type 0, at k = 8 all are on type 8, at
// k = 4 and 12 four at a time on 4 and 12, at even k two at a time;
// each thread makes one object of every type first, in its order, and
// the rest of them after, so that the first uses meet. Every thread must
// see one Metadata and one pointer map per type, the pages of its
// objects must name that Metadata, the objects keep their values through
// collections, and they are freed once dropped.
TEST(Maker_Tests, ManyThreadsMakeManyNewTypesAtOnce) {
    constexpr int Count = 300;
    (void)make_tracked<int>(1);   // the heap and the collector made on this thread first
    constexpr auto makers = make_raced_table(std::make_integer_sequence<int, RacedTypes>());
    constexpr auto checkers = check_raced_table(std::make_integer_sequence<int, RacedTypes>());
    RacedSeen seen[RacingThreads][RacedTypes] = {};
    std::atomic<int> failed = {0};
    std::atomic<int> ready = {0};
    std::vector<std::thread> threads;
    for (int t = 0; t < RacingThreads; ++t) {
        threads.emplace_back([&, t] {
            RacedHeads heads;
            const int step = 2 * t + 1;   // odd: a permutation of the 16 types, a different one per thread
            ready.fetch_add(1);
            while (ready.load() < RacingThreads) {   // spinning, not a latch: released within nanoseconds, not by wakes one at a time
            }
            for (int k = 0; k < RacedTypes; ++k) {
                auto n = k * step % RacedTypes;
                makers[n](t, 0, 1, heads, seen[t][n]);
            }
            for (int k = 0; k < RacedTypes; ++k) {
                auto n = k * step % RacedTypes;
                if (!makers[n](t, 1, Count, heads, seen[t][n])) {
                    failed.fetch_add(1);
                }
            }
            for (int round = 0; round < 2; ++round) {
                collector::force_collect(true);
                for (int n = 0; n < RacedTypes; ++n) {
                    if (!checkers[n](t, Count, heads)) {
                        failed.fetch_add(1);
                    }
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(failed.load(), 0);
    for (int n = 0; n < RacedTypes; ++n) {
        auto& first = seen[0][n];
        EXPECT_NE(first.metadata, nullptr) << n;
        EXPECT_EQ(&first.metadata->child_pointers, first.pointers) << n;
        for (int t = 0; t < RacingThreads; ++t) {
            EXPECT_EQ(seen[t][n].metadata, first.metadata) << "type " << n << " thread " << t;
            EXPECT_EQ(seen[t][n].pointers, first.pointers) << "type " << n << " thread " << t;
            EXPECT_EQ(seen[t][n].page_metadata, first.metadata) << "type " << n << " thread " << t;
        }
#if defined(SGCL_ASAN)
        EXPECT_EQ(first.metadata->user_size, 8 + 8 * (n + 1) + sizeof(tracked_ptr<int>)) << n;   // object_size is the slot, with its redzone (page_info.h: SlotSize)
#else
        EXPECT_EQ(first.metadata->object_size, 8 + 8 * (n + 1) + sizeof(tracked_ptr<int>)) << n;
#endif
        EXPECT_EQ(first.pointers->map.size(), 1u) << n;
    }
    collector::clear_stack();
    for (int i = 0; i < 3 && raced_alive.load() != 0; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(raced_alive.load(), 0);
}

// A buffer whose bytes, capacity times the element's size, pass size_t:
// 2^61 elements of 8 bytes wrapped to 0 bytes and got a buffer of the
// smallest class, whose header said it held 2 elements. Refused now, as
// any buffer no memory holds: the program ends with the line of a refused
// managed allocation (maker.h: buffer_max_capacity). No container reaches
// it (vector and dynamic_array stop at max_size(), the hash tables at
// MaxBuckets, a deque grows its map a block at a time).
TEST(Maker_Tests, ABufferWhoseBytesPassSizeTEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    auto wrapped = [] {
        auto p = detail::Maker<uint64_t[]>::make_tracked_data(size_t(1) << 61);
        (void)p;
    };
    auto wrapped_in_pages = [] {
        auto p = detail::Maker<uint64_t[]>::make_tracked_data_in_whole_pages((size_t(1) << 61) + 1);
        (void)p;
    };
    EXPECT_DEATH(wrapped(), "sgcl: out of managed memory");
    EXPECT_DEATH(wrapped_in_pages(), "sgcl: out of managed memory");
}

// The bound of that check: at buffer_max_capacity the bytes with the
// header, rounded up to whole pages and then by the range's allocator
// once more (its page_size - 1), still fit in size_t; one element more
// and the bytes with the header alone do not leave that room. Computed in
// 128 bits for elements of 1 byte to past a page; nothing allocated.
TEST(Maker_Tests, TheLargestBufferCapacityFitsSizeT) {
    using Wide = unsigned __int128;
    constexpr Wide limit = size_t(-1);
    constexpr Wide header = sizeof(detail::ArrayBase);
    constexpr Wide page = config::page_size;
    for (size_t size : {size_t(1), size_t(2), size_t(3), size_t(8), size_t(24), size_t(4096), config::page_size + 8}) {
        Wide max = detail::buffer_max_capacity(size);
        Wide pages = (max * size + header + page - 1) / page;
        EXPECT_LE(pages * page + page - 1, limit) << size;
        EXPECT_GT((max + 1) * size + header + page - 1, limit) << size;
    }
}
