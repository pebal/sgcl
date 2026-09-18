//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "sgcl/containers/deque.h"
#include "tests/types.h"

#include <algorithm>
#include <deque>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static_assert(std::random_access_iterator<sgcl::deque<int>::iterator>);
static_assert(std::random_access_iterator<sgcl::deque<int>::const_iterator>);
static_assert(std::sized_sentinel_for<sgcl::deque<int>::iterator, sgcl::deque<int>::iterator>);
static_assert(std::sortable<sgcl::deque<int>::iterator>);
static_assert(std::ranges::random_access_range<sgcl::deque<int>>);
static_assert(std::ranges::random_access_range<const sgcl::deque<int>>);
static_assert(sizeof(sgcl::deque<int>::iterator) == 3 * sizeof(void*));   // raw: the map, the index, the slot

namespace {
    struct MoveOnly {
        int value;

        MoveOnly() noexcept
        : value(0) {
        }

        explicit MoveOnly(int v) noexcept
        : value(v) {
        }

        MoveOnly(MoveOnly&& other) noexcept
        : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    // Throws from the construction that brings `remaining` to zero; moves
    // never throw. `live` counts the instances alive.
    struct Thrower {
        inline static int remaining = -1;
        inline static int live = 0;
        int value;

        explicit Thrower(int v)
        : value(v) {
            check();
            ++live;
        }

        Thrower(const Thrower& other)
        : value(other.value) {
            check();
            ++live;
        }

        Thrower(Thrower&& other) noexcept
        : value(other.value) {
            ++live;
        }

        Thrower& operator=(const Thrower&) = default;
        Thrower& operator=(Thrower&&) = default;

        ~Thrower() {
            --live;
        }

        bool operator==(const Thrower& other) const {
            return value == other.value;
        }

        static void check() {
            if (remaining == 0) {
                remaining = -1;
                throw std::runtime_error("Thrower");
            }
            if (remaining > 0) {
                --remaining;
            }
        }
    };

    template<class D>
    std::vector<int> to_vector(const D& d) {
        return std::vector<int>(d.begin(), d.end());
    }

    template<class D>
    std::vector<int> to_reversed_vector(const D& d) {
        return std::vector<int>(d.rbegin(), d.rend());
    }
}

TEST(Deque_Test, DefaultConstructorEmpty) {
    sgcl::deque<Int> dq;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(dq.size(), 0u);
    EXPECT_EQ(dq.begin(), dq.end());
    EXPECT_EQ(dq.cbegin(), dq.cend());
    EXPECT_EQ(dq.rbegin(), dq.rend());
    EXPECT_EQ(dq.crbegin(), dq.crend());
    EXPECT_GT(dq.max_size(), 0u);
}

TEST(Deque_Test, ConstructorNDefault) {
    sgcl::deque<Int> dq(3);
    EXPECT_EQ(collector::get_live_object_count(), 2u);   // the map and one block
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(dq.empty());
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {0, 0, 0};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, ConstructorNValues) {
    sgcl::deque<Int> dq(4, 3);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(dq.size(), 4u);
    std::vector<int> expected = {3, 3, 3, 3};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, InitializerList) {
    sgcl::deque<Int> dq({4, 5, 6});
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, ConstructorRange) {
    std::vector<int> expected = {1, 2, 3};
    sgcl::deque<Int> dq(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(dq.size(), 3u);
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, ConstructorInputIterators) {
    std::istringstream in("7 8 9");
    auto first = std::istream_iterator<int>(in);
    auto last = std::istream_iterator<int>();
    sgcl::deque<Int> dq(first, last);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    std::vector<int> expected = {7, 8, 9};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, CopyConstructor) {
    sgcl::deque<Int> other({1, 2, 3});
    sgcl::deque<Int> dq(other);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(to_vector(dq), expected);
    EXPECT_EQ(to_vector(other), expected);
}

TEST(Deque_Test, MoveConstructor) {
    sgcl::deque<Int> other({4, 5, 6});
    sgcl::deque<Int> dq(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.begin(), other.end());
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
    other.push_back(1);   // a moved-from deque is usable
    EXPECT_EQ(other.size(), 1u);
}

TEST(Deque_Test, CopyAssignment) {
    sgcl::deque<Int> other({1, 2, 3});
    sgcl::deque<Int> dq;
    dq = other;
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(to_vector(dq), expected);
    expected = {3, 2, 1};
    EXPECT_EQ(to_reversed_vector(dq), expected);

    other = sgcl::deque<Int>({2, 3});
    dq = other;
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(dq.size(), 2u);
    expected = {2, 3};
    EXPECT_EQ(to_vector(dq), expected);

    other = sgcl::deque<Int>();
    dq = other;
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // dq keeps its map
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(dq.size(), 0u);
    EXPECT_EQ(dq.begin(), dq.end());

    dq = dq;
    EXPECT_EQ(dq.size(), 0u);
}

TEST(Deque_Test, MoveAssignment) {
    sgcl::deque<Int> other({4, 5, 6});
    sgcl::deque<Int> dq{1};
    dq = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(dq.size(), 3u);
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, ListAssignment) {
    sgcl::deque<Int> dq;
    dq = {7, 8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    std::vector<int> expected = {7, 8, 9};
    EXPECT_EQ(to_vector(dq), expected);
    expected = {9, 8, 7};
    EXPECT_EQ(to_reversed_vector(dq), expected);

    dq = {8, 9};
    EXPECT_EQ(Int::counter, 2u);
    expected = {8, 9};
    EXPECT_EQ(to_vector(dq), expected);

    dq = std::initializer_list<Int>();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(dq.size(), 0u);
    EXPECT_EQ(dq.begin(), dq.end());
}

TEST(Deque_Test, AssignNValues) {
    sgcl::deque<Int> dq({1, 2});
    dq.assign(4, 5);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 4u);
    std::vector<int> expected = {5, 5, 5, 5};
    EXPECT_EQ(to_vector(dq), expected);

    dq.assign(2, 3);
    EXPECT_EQ(Int::counter, 2u);
    expected = {3, 3};
    EXPECT_EQ(to_vector(dq), expected);

    dq.assign(0, 0);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(dq.size(), 0u);
    EXPECT_EQ(dq.rbegin(), dq.rend());
}

TEST(Deque_Test, AssignRange) {
    sgcl::deque<Int> dq;
    std::vector<int> expected = {3, 4, 5};
    dq.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(to_vector(dq), expected);

    expected = {2, 3, 4, 5};
    dq.assign(expected.begin(), expected.end());
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(to_vector(dq), expected);

    std::istringstream in("1 2");
    dq.assign(std::istream_iterator<int>(in), std::istream_iterator<int>());
    EXPECT_EQ(Int::counter, 2u);
    expected = {1, 2};
    EXPECT_EQ(to_vector(dq), expected);

    dq.assign(expected.begin(), expected.begin());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(dq.size(), 0u);
}

TEST(Deque_Test, AssignList) {
    sgcl::deque<Int> dq({1, 2});
    dq.assign({3, 4, 5});
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 3u);
    std::vector<int> expected = {3, 4, 5};
    EXPECT_EQ(std::vector<int>(dq.cbegin(), dq.cend()), expected);
    expected = {5, 4, 3};
    EXPECT_EQ(std::vector<int>(dq.crbegin(), dq.crend()), expected);

    dq.assign(std::initializer_list<Int>());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(dq.cbegin(), dq.cend());
}

TEST(Deque_Test, ElementAccess) {
    sgcl::deque<Int> dq({0, 1, 2, 3, 4});
    const auto& cdq = dq;
    for (size_t i = 0; i < dq.size(); ++i) {
        EXPECT_EQ(dq[i], int(i));
        EXPECT_EQ(cdq[i], int(i));
        EXPECT_EQ(dq.at(i), int(i));
        EXPECT_EQ(cdq.at(i), int(i));
    }
    EXPECT_THROW(dq.at(5), std::out_of_range);
    EXPECT_THROW(cdq.at(10), std::out_of_range);
    EXPECT_EQ(dq.front(), 0);
    EXPECT_EQ(cdq.front(), 0);
    EXPECT_EQ(dq.back(), 4);
    EXPECT_EQ(cdq.back(), 4);
    dq[2] = 7;
    dq.front() = 5;
    dq.back() = 6;
    std::vector<int> expected = {5, 1, 7, 3, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, PushAndEmplaceBack) {
    sgcl::deque<Int> dq({1, 2, 3});
    dq.push_back(4);
    Int five(5);
    dq.push_back(five);
    auto& v = dq.emplace_back(6);
    EXPECT_EQ(v, 6);
    EXPECT_EQ(&v, &dq.back());
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 7u);
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, PushAndEmplaceFront) {
    sgcl::deque<Int> dq({4, 5, 6});
    dq.push_front(3);
    Int two(2);
    dq.push_front(two);
    auto& v = dq.emplace_front(1);
    EXPECT_EQ(v, 1);
    EXPECT_EQ(&v, &dq.front());
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the map, one block per direction
    EXPECT_EQ(Int::counter, 7u);
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
    expected = {6, 5, 4, 3, 2, 1};
    EXPECT_EQ(to_reversed_vector(dq), expected);
}

TEST(Deque_Test, PopBackAndFront) {
    sgcl::deque<Int> dq({1, 2, 3, 4});
    dq.pop_back();
    EXPECT_EQ(Int::counter, 3u);
    dq.pop_front();
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    std::vector<int> expected = {2, 3};
    EXPECT_EQ(to_vector(dq), expected);
    dq.pop_front();
    dq.pop_back();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the empty block is dropped, the map stays
    dq.push_back(9);
    EXPECT_EQ(dq.front(), 9);
    EXPECT_EQ(dq.back(), 9);
}

// 100k elements each way: many blocks in both directions, several map
// reallocations, and the blocks released again as the ends are popped.
TEST(Deque_Test, GrowthInBothDirections) {
    const int n = 100000;
    sgcl::deque<int> dq;
    off_frame([&] {
        for (int i = 1; i <= n; ++i) {
            dq.push_front(-i);
        }
        for (int i = 0; i < n; ++i) {
            dq.push_back(i);
        }
    });
    ASSERT_EQ(dq.size(), size_t(2 * n));
    // the checks leave references to elements behind: in a frame of their own
    off_frame([&] {
        EXPECT_EQ(dq.front(), -n);
        EXPECT_EQ(dq.back(), n - 1);
        for (int i = 0; i < 2 * n; ++i) {
            ASSERT_EQ(dq[i], i - n);
        }
        int expected = -n;
        for (auto v : dq) {
            ASSERT_EQ(v, expected++);
        }
        expected = n - 1;
        for (auto i = dq.rbegin(); i != dq.rend(); ++i) {
            ASSERT_EQ(*i, expected--);
        }
    });
    // 1024 ints per block: 196 or 197 blocks depending on the alignment of
    // the first element within its block, plus the map
    auto count = collector::get_live_object_count();
    EXPECT_GE(count, 197u);
    EXPECT_LE(count, 198u);
    off_frame([&] {
        for (int i = 0; i < n; ++i) {
            dq.pop_front();
            dq.pop_back();
        }
    });
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {   // the same map serves a moving window
            dq.push_back(i);
            dq.push_back(i);
            dq.pop_front();
        }
        EXPECT_EQ(dq.front(), 500);
    });
    EXPECT_EQ(dq.size(), 1000u);
    dq.clear();
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(Deque_Test, RandomAccessIteratorAndSort) {
    std::mt19937 rng(1);
    sgcl::deque<int> dq;
    for (int i = 0; i < 20000; ++i) {
        dq.push_back(int(rng() % 1000));
    }
    std::vector<int> expected(dq.begin(), dq.end());
    std::sort(expected.begin(), expected.end());
    std::ranges::sort(dq);
    EXPECT_EQ(to_vector(dq), expected);
    EXPECT_TRUE(std::ranges::binary_search(dq, expected[100]));
    std::sort(dq.begin(), dq.end(), std::greater<>());
    std::sort(expected.begin(), expected.end(), std::greater<>());
    EXPECT_EQ(to_vector(dq), expected);

    auto it = dq.begin();
    EXPECT_EQ(it[3], dq[3]);
    EXPECT_EQ(*(it + 5), dq[5]);
    EXPECT_EQ(*(5 + it), dq[5]);
    EXPECT_EQ(*(dq.end() - 1), dq.back());
    EXPECT_EQ(dq.end() - dq.begin(), ptrdiff_t(dq.size()));
    EXPECT_LT(it, it + 1);
    EXPECT_GE(it + 1, it);
    it += 10;
    EXPECT_EQ(*it, dq[10]);
    it -= 4;
    EXPECT_EQ(*it, dq[6]);
    EXPECT_EQ(*it++, dq[6]);
    EXPECT_EQ(*it, dq[7]);
    EXPECT_EQ(*--it, dq[6]);
    EXPECT_EQ(*it--, dq[6]);
    EXPECT_EQ(*it, dq[5]);
    sgcl::deque<int>::const_iterator cit = it;   // iterator converts to const_iterator
    EXPECT_EQ(cit, it);
    EXPECT_EQ(*cit, dq[5]);
    const auto& cdq = dq;
    EXPECT_EQ(std::distance(cdq.begin(), cdq.end()), ptrdiff_t(dq.size()));
    EXPECT_EQ(*cdq.rbegin(), cdq.back());
    EXPECT_EQ(*cdq.crbegin(), cdq.back());
    EXPECT_EQ(*(cdq.rend() - 1), cdq.front());
}

// Iterators hold raw pointers, so they may live anywhere, in a std::vector
// too: the deque roots the map and the blocks, not the iterators.
TEST(Deque_Test, IteratorsInStdVector) {
    sgcl::deque<Int> dq;
    for (int i = 0; i < 3000; ++i) {
        dq.push_back(i);
    }
    std::vector<sgcl::deque<Int>::iterator> its;
    for (auto it = dq.begin(); it != dq.end(); ++it) {
        its.push_back(it);
    }
    std::vector<sgcl::deque<Int>::const_iterator> cits(its.begin(), its.end());
    ASSERT_EQ(its.size(), 3000u);
    std::mt19937 rng(3);
    std::shuffle(its.begin(), its.end(), rng);
    std::sort(its.begin(), its.end());
    collector::force_collect(true);
    for (int i = 0; i < 3000; ++i) {
        ASSERT_EQ(*its[i], i);
        ASSERT_EQ(*cits[i], i);
        ASSERT_EQ(its[i] - dq.begin(), i);
        ASSERT_EQ(its[i], cits[i]);
    }
    EXPECT_EQ(cits[1000][500], 1500);
    EXPECT_EQ(*(its[2999] - 2999), 0);
    std::vector<sgcl::deque<Int>::iterator> copy = its;
    its.clear();
    EXPECT_EQ(*copy.back(), 2999);
    EXPECT_EQ(Int::counter, 3000u);
    EXPECT_EQ(collector::get_live_object_count(), 4u);   // 1024 per block: three blocks and the map
}

// An iterator kept across a collection stays valid as long as the deque is
// unchanged: the deque roots the map and the blocks, and the elements'
// pointers keep their targets.
TEST(Deque_Test, IteratorAcrossCollection) {
    sgcl::deque<Foo> dq;
    for (int i = 0; i < 1000; ++i) {
        dq.emplace_back(i);
    }
    sgcl::deque<Foo>::iterator it;
    sgcl::deque<Foo>::const_iterator cit;
    sgcl::deque<Foo>::reverse_iterator rit;
    std::vector<sgcl::deque<Foo>::iterator> its;
    off_frame([&] {
        it = dq.begin() + 600;
        cit = dq.cbegin() + 999;
        rit = dq.rbegin();
        its = {dq.begin(), dq.begin() + 63, dq.begin() + 64, dq.end() - 1};
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 1u + 16u + 1000u);   // the map, 64 per block, the Baz
    // the checks leave references to elements behind: in a frame of their own
    off_frame([&] {
        EXPECT_EQ(it->ptr->value, 600);
        EXPECT_EQ((*it).value, 600);
        EXPECT_EQ(cit->ptr->value, 999);
        EXPECT_EQ(rit->ptr->value, 999);
        EXPECT_EQ(it, dq.begin() + 600);
        EXPECT_EQ(it[100].ptr->value, 700);
        EXPECT_EQ(its[0]->ptr->value, 0);
        EXPECT_EQ(its[1]->ptr->value, 63);
        EXPECT_EQ(its[2]->ptr->value, 64);
        EXPECT_EQ(its[3]->ptr->value, 999);
        long sum = 0;
        for (auto i = it; i != dq.end(); ++i) {
            sum += i->ptr->value;
        }
        EXPECT_EQ(sum, (600 + 999) * 400 / 2);
        sum = 0;
        for (auto i = dq.rbegin(); i != dq.rend(); ++i) {   // a step back across every block boundary
            sum += i->value;
        }
        EXPECT_EQ(sum, 999 * 1000 / 2);
    });
    it = sgcl::deque<Foo>::iterator();
    cit = sgcl::deque<Foo>::const_iterator();
    rit = sgcl::deque<Foo>::reverse_iterator();
    its.clear();
    dq.clear();
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

// A deque emptied by pops starts at a block boundary: its iterators
// address no block, and the ends grow again from there.
TEST(Deque_Test, EmptiedInTheMiddleOfABlock) {
    sgcl::deque<Int> dq({1, 2, 3, 4, 5});
    dq.pop_front();
    dq.pop_back();
    dq.pop_back();
    dq.pop_front();
    dq.pop_back();
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the map
    EXPECT_EQ(dq.begin(), dq.end());
    EXPECT_EQ(dq.begin() + 0, dq.end());
    EXPECT_EQ(dq.rbegin(), dq.rend());
    int visits = 0;
    for (auto& v : dq) {
        visits += v;
    }
    EXPECT_EQ(visits, 0);
    dq.push_front(2);
    dq.push_back(3);
    dq.push_front(1);
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(to_vector(dq), expected);
    sgcl::deque<Int> fresh;
    EXPECT_EQ(fresh.begin(), fresh.end());
    EXPECT_EQ(fresh.rbegin(), fresh.rend());
    EXPECT_EQ(fresh.end() - fresh.begin(), 0);
}

// A block left empty by pops stays as the spare of its end and serves the
// next push there or, moved across, at the other end: a window of elements
// travelling through the deque allocates no blocks. An emptied deque drops
// every block.
TEST(Deque_Test, SpareBlocks) {
    sgcl::deque<long> dq;   // 512 per block
    off_frame([&] {
        for (long i = 0; i < 1024; ++i) {
            dq.push_back(i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the map and two blocks
    off_frame([&] {
        for (long i = 0; i < 512; ++i) {
            dq.pop_front();
        }
        dq.push_front(-1);   // into the spare at the front
        dq.pop_front();
    });
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    off_frame([&] {
        for (long i = 0; i < 512; ++i) {   // fills the back block, then the spare moves over
            dq.push_back(1000 + i);
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(dq.size(), 1024u);
    off_frame([&] {
        EXPECT_EQ(dq.front(), 512);
        EXPECT_EQ(dq.back(), 1511);
        for (long i = 0; i < 512; ++i) {
            dq.pop_front();
        }
        for (long i = 0; i < 100000; ++i) {   // a window of 512 over some 200 blocks
            dq.push_back(i);
            dq.pop_front();
        }
        EXPECT_EQ(dq.front(), 100000 - 512);
        EXPECT_EQ(dq.back(), 99999);
    });
    EXPECT_EQ(dq.size(), 512u);
    auto count = collector::get_live_object_count();   // the map, one or two blocks, at most one spare
    EXPECT_GE(count, 2u);
    EXPECT_LE(count, 4u);
    off_frame([&] {
        for (long i = 0; i < 512; ++i) {
            dq.pop_back();
        }
        EXPECT_TRUE(dq.empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the map alone
    dq.push_back(7);
    dq.push_front(6);
    std::vector<long> expected = {6, 7};
    EXPECT_EQ(std::vector<long>(dq.begin(), dq.end()), expected);
    dq.clear();
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

// The insertions run in a frame of their own: the counts must not see
// references the pushes left behind.
TEST(Deque_Test, Emplace) {
    sgcl::deque<Int> dq({2, 3, 5});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.emplace(dq.begin(), 1);
        EXPECT_EQ(*it, 1);
    });
    EXPECT_EQ(Int::counter, 4u);
    off_frame([&] {
        it = dq.emplace(dq.begin() + 3, 4);
        EXPECT_EQ(*it, 4);
    });
    EXPECT_EQ(Int::counter, 5u);
    off_frame([&] {
        it = dq.emplace(dq.end(), 6);
        EXPECT_EQ(*it, 6);
    });
    EXPECT_EQ(Int::counter, 6u);
    off_frame([&] {
        it = dq.emplace(dq.begin() + 1, 9);   // front half
        EXPECT_EQ(*it, 9);
    });
    EXPECT_EQ(Int::counter, 7u);
    std::vector<int> expected = {1, 9, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
    off_frame([&] {
        it = dq.emplace(dq.begin() + 2, dq[5]);   // the argument refers into the deque
        EXPECT_EQ(*it, 5);
    });
    expected = {1, 9, 5, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
    EXPECT_EQ(Int::counter, 8u);
    it = sgcl::deque<Int>::iterator();
    EXPECT_EQ(collector::get_live_object_count(), 3u);
}

TEST(Deque_Test, InsertValue) {
    sgcl::deque<Int> dq({2, 3, 5});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.insert(dq.begin(), 1);
        EXPECT_EQ(*it, 1);
        Int four(4);
        it = dq.insert(dq.begin() + 3, four);
        EXPECT_EQ(*it, 4);
        it = dq.insert(dq.end(), Int(6));
        EXPECT_EQ(*it, 6);
        it = dq.insert(dq.begin() + 4, dq[1]);
        EXPECT_EQ(*it, 2);
    });
    EXPECT_EQ(Int::counter, 7u);
    EXPECT_EQ(dq.size(), 7u);
    std::vector<int> expected = {1, 2, 3, 4, 2, 5, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, InsertNValues) {
    sgcl::deque<Int> dq({2, 3, 5});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.insert(dq.begin(), 0, 1);
        EXPECT_EQ(it, dq.begin());
        EXPECT_EQ(dq.size(), 3u);
        it = dq.insert(dq.begin(), 2, 1);
        EXPECT_EQ(*it, 1);
        EXPECT_EQ(*++it, 1);
        it = dq.insert(dq.begin() + 4, 2, 4);
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(*++it, 4);
        it = dq.insert(dq.end(), 2, 6);
        EXPECT_EQ(*it, 6);
        EXPECT_EQ(*++it, 6);
        it = dq.insert(dq.begin() + 1, 2, dq[3]);   // front half, value from the deque
        EXPECT_EQ(*it, 3);
    });
    EXPECT_EQ(Int::counter, 11u);
    std::vector<int> expected = {1, 3, 3, 1, 2, 3, 4, 4, 5, 6, 6};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, InsertRange) {
    sgcl::deque<Int> dq({2, 3, 5});
    std::vector<int> other = {4, 5};
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.insert(dq.end(), other.begin(), other.begin());
        EXPECT_EQ(it, dq.end());
        it = dq.insert(dq.begin(), other.begin(), other.end());
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(*++it, 5);
        other = {7, 8};
        it = dq.insert(dq.begin() + 4, other.begin(), other.end());
        EXPECT_EQ(*it, 7);
        EXPECT_EQ(*++it, 8);
        other = {2, 3};
        it = dq.insert(dq.end(), other.begin(), other.end());
        EXPECT_EQ(*it, 2);
        EXPECT_EQ(*++it, 3);
        other = {0, 1};
        it = dq.insert(dq.begin() + 1, other.begin(), other.end());   // front half
        EXPECT_EQ(*it, 0);
        std::istringstream in("11 12");
        it = dq.insert(dq.begin() + 3, std::istream_iterator<int>(in), std::istream_iterator<int>());
        EXPECT_EQ(*it, 11);
        std::forward_list<int> forward = {21, 22};   // forward only: pushed at the back and rotated
        it = dq.insert(dq.begin() + 2, forward.begin(), forward.end());
        EXPECT_EQ(*it, 21);
    });
    EXPECT_EQ(Int::counter, 15u);
    std::vector<int> expected = {4, 0, 21, 22, 1, 11, 12, 5, 2, 3, 7, 8, 5, 2, 3};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, InsertList) {
    sgcl::deque<Int> dq({2, 3, 5});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.insert(dq.end(), std::initializer_list<Int>());
        EXPECT_EQ(it, dq.end());
        it = dq.insert(dq.begin(), {5, 4});
        EXPECT_EQ(*it, 5);
        it = dq.insert(dq.begin() + 4, {8, 7});
        EXPECT_EQ(*it, 8);
        it = dq.insert(dq.end(), {3, 2});
        EXPECT_EQ(*it, 3);
    });
    EXPECT_EQ(Int::counter, 9u);
    std::vector<int> expected = {5, 4, 2, 3, 8, 7, 5, 3, 2};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, Erase) {
    sgcl::deque<Int> dq({1, 2, 3, 4, 5});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.erase(dq.begin());
        EXPECT_EQ(*it, 2);
    });
    EXPECT_EQ(Int::counter, 4u);
    off_frame([&] {
        it = dq.erase(++dq.begin());
        EXPECT_EQ(*it, 4);
    });
    EXPECT_EQ(Int::counter, 3u);
    off_frame([&] {
        it = dq.erase(--dq.end());
        EXPECT_EQ(it, dq.end());
    });
    EXPECT_EQ(Int::counter, 2u);
    off_frame([&] {
        it = dq.erase(dq.end());
        EXPECT_EQ(it, dq.end());
    });
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(to_vector(dq), expected);
}

TEST(Deque_Test, EraseRange) {
    sgcl::deque<Int> dq({1, 2, 3, 4, 5, 6, 7, 8});
    sgcl::deque<Int>::iterator it;
    off_frame([&] {
        it = dq.erase(dq.begin(), dq.begin() + 2);
        EXPECT_EQ(*it, 3);
    });
    EXPECT_EQ(Int::counter, 6u);
    off_frame([&] {
        it = dq.erase(dq.begin() + 1, dq.begin() + 3);   // front half
        EXPECT_EQ(*it, 6);
    });
    EXPECT_EQ(Int::counter, 4u);
    off_frame([&] {
        it = dq.erase(dq.end() - 2, dq.end());
        EXPECT_EQ(it, dq.end());
    });
    EXPECT_EQ(Int::counter, 2u);
    off_frame([&] {
        it = dq.erase(dq.end(), dq.end());
        EXPECT_EQ(it, dq.end());
        it = dq.erase(dq.begin(), dq.begin());
        EXPECT_EQ(it, dq.begin());
    });
    std::vector<int> expected = {3, 6};
    EXPECT_EQ(to_vector(dq), expected);
    off_frame([&] {
        it = dq.erase(dq.begin(), dq.end());
        EXPECT_EQ(it, dq.end());
    });
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(dq.empty());
}

TEST(Deque_Test, Resize) {
    sgcl::deque<Int> dq({1, 2, 3});
    dq.resize(5);
    EXPECT_EQ(Int::counter, 5u);
    std::vector<int> expected = {1, 2, 3, 0, 0};
    EXPECT_EQ(to_vector(dq), expected);
    dq.resize(2);
    EXPECT_EQ(Int::counter, 2u);
    expected = {1, 2};
    EXPECT_EQ(to_vector(dq), expected);
    dq.resize(4, 8);
    EXPECT_EQ(Int::counter, 4u);
    expected = {1, 2, 8, 8};
    EXPECT_EQ(to_vector(dq), expected);
    dq.resize(0);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(dq.empty());
    dq.resize(2000);
    EXPECT_EQ(Int::counter, 2000u);
    EXPECT_EQ(dq.size(), 2000u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // two blocks of 1024 and the map
}

TEST(Deque_Test, Swap) {
    sgcl::deque<Int> dq1({1, 2});
    sgcl::deque<Int> dq2({4, 5, 6, 7});
    dq1.swap(dq2);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> expected = {4, 5, 6, 7};
    EXPECT_EQ(to_vector(dq1), expected);
    expected = {1, 2};
    EXPECT_EQ(to_vector(dq2), expected);
    swap(dq1, dq2);
    EXPECT_EQ(to_vector(dq1), expected);
    sgcl::deque<Int> empty;
    dq1.swap(empty);
    EXPECT_TRUE(dq1.empty());
    EXPECT_EQ(to_vector(empty), expected);
}

TEST(Deque_Test, Clear) {
    sgcl::deque<Int> dq({5, 6, 7});
    dq.clear();
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(dq.empty());
    EXPECT_EQ(dq.begin(), dq.end());
    EXPECT_EQ(dq.rbegin(), dq.rend());
    dq.push_back(1);
    EXPECT_EQ(dq.size(), 1u);
    EXPECT_EQ(dq.front(), 1);
}

TEST(Deque_Test, ShrinkToFit) {
    sgcl::deque<Int> dq;
    off_frame([&] {
        for (int i = 0; i < 5000; ++i) {
            dq.push_front(i);
        }
        dq.erase(dq.begin() + 3, dq.end());
    });
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the blocks went as they emptied, but for the last one, the spare
    dq.shrink_to_fit();
    EXPECT_EQ(collector::get_live_object_count(), 2u);   // the spare went too
    std::vector<int> expected = {4999, 4998, 4997};
    EXPECT_EQ(to_vector(dq), expected);
    dq.push_back(1);   // the shrunk map grows again
    dq.push_front(2);
    expected = {2, 4999, 4998, 4997, 1};
    EXPECT_EQ(to_vector(dq), expected);
    dq.clear();
    dq.shrink_to_fit();
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(Deque_Test, Comparisons) {
    sgcl::deque<int> a({1, 2, 3});
    sgcl::deque<int> b({1, 2, 3});
    sgcl::deque<int> c({1, 2, 4});
    sgcl::deque<int> d({1, 2});
    sgcl::deque<int> e;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(d < a);
    EXPECT_TRUE(e < d);
    EXPECT_TRUE(e == sgcl::deque<int>());
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
    EXPECT_EQ(a <=> c, std::strong_ordering::less);
    EXPECT_EQ(a <=> d, std::strong_ordering::greater);
    sgcl::deque<std::string> s1({"a", "b"});
    sgcl::deque<std::string> s2({"a", "c"});
    EXPECT_TRUE(s1 < s2);
    EXPECT_TRUE(s1 != s2);
    sgcl::deque<double> f1({1.0, 2.0});
    sgcl::deque<double> f2({1.0, 2.5});
    EXPECT_EQ(f1 <=> f2, std::partial_ordering::less);
}

TEST(Deque_Test, MoveOnlyElements) {
    sgcl::deque<MoveOnly> dq;
    dq.push_back(MoveOnly(3));
    dq.emplace_back(4);
    dq.push_front(MoveOnly(1));
    dq.emplace_front(0);
    dq.insert(dq.begin() + 2, MoveOnly(2));
    dq.emplace(dq.end() - 1, 7);
    EXPECT_EQ(dq.size(), 6u);
    std::vector<int> values;
    for (auto& m : dq) {
        values.push_back(m.value);
    }
    std::vector<int> expected = {0, 1, 2, 3, 7, 4};
    EXPECT_EQ(values, expected);
    dq.erase(dq.begin() + 4);
    dq.erase(dq.begin(), dq.begin() + 2);
    dq.resize(5);   // resize(n) needs no copy
    dq.pop_front();
    values.clear();
    for (auto& m : dq) {
        values.push_back(m.value);
    }
    expected = {3, 4, 0, 0};
    EXPECT_EQ(values, expected);
    sgcl::deque<MoveOnly> other(std::move(dq));
    EXPECT_EQ(other.size(), 4u);
    EXPECT_TRUE(dq.empty());
    dq = std::move(other);
    EXPECT_EQ(dq.size(), 4u);
    std::sort(dq.begin(), dq.end(), [](const MoveOnly& a, const MoveOnly& b) { return a.value < b.value; });
    EXPECT_EQ(dq.front().value, 0);
    EXPECT_EQ(dq.back().value, 4);
}

TEST(Deque_Test, StringElements) {
    // every operation runs in a frame of its own: in optimized builds the
    // inlined push paths leave block pointers in the frame they run in
    sgcl::deque<std::string> dq;
    sgcl::deque<std::string> copy;
    off_frame([&] {
        dq = {"pear", "apple"};
        dq.push_back("fig");
        dq.emplace_front(3, 'z');
        dq.insert(dq.begin() + 2, "kiwi");
        std::string s = "plum";
        dq.insert(dq.end(), s);
        std::vector<std::string> expected = {"zzz", "pear", "kiwi", "apple", "fig", "plum"};
        EXPECT_EQ(std::vector<std::string>(dq.begin(), dq.end()), expected);
        std::ranges::sort(dq);
        std::sort(expected.begin(), expected.end());
        EXPECT_EQ(std::vector<std::string>(dq.begin(), dq.end()), expected);
        dq.erase(dq.begin() + 1, dq.begin() + 3);
        EXPECT_EQ(dq.size(), 4u);
        EXPECT_EQ(dq[1], "pear");
        copy = dq;
        EXPECT_EQ(copy, dq);
        dq.clear();
    });
    EXPECT_EQ(copy.size(), 4u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

// Elements holding tracked pointers: what they point to stays alive with
// the element, and dies with it.
TEST(Deque_Test, TrackedElements) {
    sgcl::deque<Foo> dq;
    dq.emplace_back(1);
    dq.push_back(Foo(2));
    dq.emplace_front(0);
    dq.insert(dq.begin() + 1, Foo(7));
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 7u);   // the map, two blocks, four Baz
    std::vector<int> values;
    off_frame([&] {
        for (auto& f : dq) {
            values.push_back(f.ptr->value);
        }
    });
    std::vector<int> expected = {0, 7, 1, 2};
    EXPECT_EQ(values, expected);
    dq.erase(dq.begin() + 1);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    dq.pop_front();
    EXPECT_EQ(collector::get_live_object_count(), 5u);   // the Baz went with the element, the emptied front block stays as the spare

    sgcl::deque<tracked_ptr<Baz>> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(make_tracked<Baz>(i));
    }
    collector::force_collect(true);
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            ASSERT_EQ(ptrs[i]->value, i);
        }
    });
    ptrs.erase(ptrs.begin() + 100, ptrs.begin() + 600);
    EXPECT_EQ(ptrs.size(), 500u);
    // 512 pointers per block: the 500 left, shifted by the erasure, span
    // two blocks; with the map, the 500 Baz and the five objects of dq
    EXPECT_EQ(collector::get_live_object_count(), 5u + 3u + 500u);
    off_frame([&] {
        EXPECT_EQ(ptrs[100]->value, 600);
    });
    ptrs.clear();
    dq.clear();
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

// A deque inside a managed object: when the object becomes garbage the
// collector destroys the elements through the blocks.
TEST(Deque_Test, DequeInsideManagedObject) {
    struct Holder {
        sgcl::deque<Int> values;
        sgcl::deque<tracked_ptr<Holder>> links;
    };
    off_frame([&] {
        tracked_ptr<Holder> a = make_tracked<Holder>();
        tracked_ptr<Holder> b = make_tracked<Holder>();
        a->values = {1, 2, 3};
        b->values.assign(2000, 4);
        a->links.push_back(b);
        b->links.push_back(a);   // a cycle
        collector::force_collect(true);
        EXPECT_EQ(a->values.size(), 3u);
        EXPECT_EQ(b->values[1999], 4);
        EXPECT_EQ(a->links.front()->values.size(), 2000u);
        EXPECT_EQ(Int::counter, 2003u);
        // two holders; a: map and block for the values, the same for the
        // links; b: map and two blocks for the values, map and block for the links
        EXPECT_EQ(collector::get_live_object_count(), 2u + 2u + 2u + 3u + 2u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Deque_Test, ExceptionSafety) {
    sgcl::deque<Thrower> dq;
    for (int i = 0; i < 5; ++i) {
        dq.emplace_back(i);
    }
    EXPECT_EQ(Thrower::live, 5);
    std::vector<int> expected = {0, 1, 2, 3, 4};
    auto values = [&] {
        std::vector<int> v;
        for (auto& t : dq) {
            v.push_back(t.value);
        }
        return v;
    };

    Thrower::remaining = 0;
    EXPECT_THROW(dq.emplace_back(9), std::runtime_error);
    EXPECT_EQ(dq.size(), 5u);
    EXPECT_EQ(Thrower::live, 5);
    Thrower::remaining = 0;
    EXPECT_THROW(dq.emplace_front(9), std::runtime_error);
    EXPECT_EQ(dq.size(), 5u);
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(collector::get_live_object_count(), 2u);   // the block allocated for the failed front element was dropped

    Thrower value(7);
    Thrower::remaining = 1;   // not an element, so no private copy: the first pushed copy succeeds, the second throws
    EXPECT_THROW(dq.insert(dq.begin() + 3, 3, value), std::runtime_error);
    EXPECT_EQ(dq.size(), 5u);
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(Thrower::live, 6);
    Thrower::remaining = 0;
    EXPECT_THROW(dq.insert(dq.begin() + 1, 3, value), std::runtime_error);   // front half: the first push throws, the block it took is dropped
    EXPECT_EQ(values(), expected);
    std::vector<Thrower> range = {Thrower(8), Thrower(9), Thrower(10)};
    Thrower::remaining = 1;
    EXPECT_THROW(dq.insert(dq.begin() + 4, range.begin(), range.end()), std::runtime_error);
    EXPECT_EQ(values(), expected);
    Thrower::remaining = 0;
    EXPECT_THROW(dq.emplace(dq.begin() + 2, 9), std::runtime_error);
    EXPECT_EQ(values(), expected);
    Thrower::remaining = 0;
    EXPECT_THROW(dq.resize(8, value), std::runtime_error);
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(Thrower::live, 9);
    EXPECT_EQ(collector::get_live_object_count(), 2u);

    dq.insert(dq.begin() + 2, range.begin(), range.end());
    expected = {0, 1, 8, 9, 10, 2, 3, 4};
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(Thrower::live, 12);

    Thrower::remaining = 3;
    EXPECT_THROW(sgcl::deque<Thrower>(5, value), std::runtime_error);
    EXPECT_EQ(Thrower::live, 12);
    Thrower::remaining = 2;
    EXPECT_THROW(sgcl::deque<Thrower>(range.begin(), range.end()), std::runtime_error);
    EXPECT_EQ(Thrower::live, 12);
    Thrower::remaining = 2;
    EXPECT_THROW({ sgcl::deque<Thrower> copy(dq); }, std::runtime_error);
    EXPECT_EQ(Thrower::live, 12);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the insertion in the front half took a second block
    Thrower::remaining = -1;
    dq.clear();
    range.clear();
    EXPECT_EQ(Thrower::live, 1);
}

TEST(Deque_Test, EraseFunctions) {
    sgcl::deque<Int> dq({1, 2, 3, 2, 4, 2, 5});
    EXPECT_EQ(erase(dq, 2), 3u);
    EXPECT_EQ(Int::counter, 4u);
    std::vector<int> expected = {1, 3, 4, 5};
    EXPECT_EQ(to_vector(dq), expected);
    EXPECT_EQ(erase_if(dq, [](const Int& v) { return v > 3; }), 2u);
    EXPECT_EQ(Int::counter, 2u);
    expected = {1, 3};
    EXPECT_EQ(to_vector(dq), expected);
    EXPECT_EQ(erase(dq, 9), 0u);
    EXPECT_EQ(dq.size(), 2u);
}

// Random operations against std::deque, with the collector kicked now and
// then; eager destruction is checked through the instance counter.
TEST(Deque_Test, StressAgainstStdDeque) {
    std::mt19937 rng(7);
    sgcl::deque<Int> dq;
    std::deque<int> oracle;
    auto same = [&] {
        if (dq.size() != oracle.size() || Int::counter != oracle.size()) {
            return false;
        }
        return std::equal(dq.begin(), dq.end(), oracle.begin(), [](const Int& a, int b) { return a == b; });
    };
    off_frame([&] {
        for (int step = 0; step < 30000; ++step) {
            int op = rng() % 100;
            int value = int(rng() % 1000);
            size_t size = oracle.size();
            size_t pos = size ? rng() % size : 0;
            if (op < 15) {
                dq.push_back(value);
                oracle.push_back(value);
            } else if (op < 30) {
                dq.push_front(value);
                oracle.push_front(value);
            } else if (op < 40) {
                if (size) {
                    dq.pop_back();
                    oracle.pop_back();
                }
            } else if (op < 50) {
                if (size) {
                    dq.pop_front();
                    oracle.pop_front();
                }
            } else if (op < 60) {
                auto it = dq.insert(dq.begin() + pos, value);
                oracle.insert(oracle.begin() + pos, value);
                ASSERT_EQ(*it, value);
            } else if (op < 65) {
                size_t count = rng() % 5;
                dq.insert(dq.begin() + pos, count, value);
                oracle.insert(oracle.begin() + pos, count, value);
            } else if (op < 75) {
                if (size) {
                    auto it = dq.erase(dq.begin() + pos);
                    auto oit = oracle.erase(oracle.begin() + pos);
                    ASSERT_EQ(it - dq.begin(), oit - oracle.begin());
                }
            } else if (op < 80) {
                size_t last = pos + rng() % 4;
                last = std::min(last, size);
                dq.erase(dq.begin() + pos, dq.begin() + last);
                oracle.erase(oracle.begin() + pos, oracle.begin() + last);
            } else if (op < 90) {
                if (size) {
                    ASSERT_EQ(dq[pos], oracle[pos]);
                    ASSERT_EQ(dq.at(pos), oracle.at(pos));
                }
            } else if (op < 93) {
                collector::force_collect();
            } else if (op < 96) {
                if (size) {
                    ASSERT_EQ(dq.front(), oracle.front());
                    ASSERT_EQ(dq.back(), oracle.back());
                }
            } else {
                ASSERT_EQ(dq.size(), oracle.size());
                ASSERT_EQ(dq.empty(), oracle.empty());
            }
            if (step % 500 == 0) {
                ASSERT_TRUE(same()) << "step " << step;
            }
        }
    });
    if (::testing::Test::HasFatalFailure()) {
        return;
    }
    ASSERT_TRUE(same());
    off_frame([&] {   // the blocks the copy and the clears touch must not linger in this frame
        sgcl::deque<Int> copy(dq);
        ASSERT_TRUE(std::equal(copy.begin(), copy.end(), dq.begin(), dq.end(), [](const Int& a, const Int& b) { return int(a) == int(b); }));
        dq.clear();
        EXPECT_EQ(Int::counter, oracle.size());
        copy.clear();
    });
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

// The review's fixes: the operations that form an iterator on a deque
// without a map, erase(end()), a fill insertion of an element of the deque
namespace {
    struct Page {
        char bytes[4096];   // one element per block
    };
}

TEST(Deque_Test, OperationsOnAnEmptyDeque) {
    sgcl::deque<int> d;
    EXPECT_EQ(d.erase(d.begin(), d.end()), d.end());
    EXPECT_EQ(d.erase(d.end()), d.end());
    EXPECT_EQ(d.erase(d.begin()), d.end());   // begin() is end()
    std::vector<int> none;
    EXPECT_EQ(d.insert(d.begin(), none.begin(), none.end()), d.begin());
    EXPECT_EQ(d.insert(d.end(), 0, 7), d.begin());
    EXPECT_EQ(d.insert(d.begin(), {}), d.begin());
    EXPECT_EQ(std::erase_if(d, [](int) { return true; }), 0u);
    EXPECT_EQ(std::erase(d, 1), 0u);
    EXPECT_TRUE(d.empty());
    d.assign({1, 2, 3});
    d.clear();   // the map let go: the same again
    EXPECT_EQ(d.erase(d.begin(), d.end()), d.end());
    EXPECT_EQ(d.insert(d.begin(), 0, 7), d.begin());
    EXPECT_EQ(std::erase_if(d, [](int) { return true; }), 0u);
    d = {1, 2};
    EXPECT_EQ(d.erase(d.end()), d.end());   // a no-op on a deque with elements
    EXPECT_EQ(d.size(), 2u);
    d.pop_front();
    d.pop_front();   // emptied by pops: the map stays
    EXPECT_EQ(d.erase(d.end()), d.end());
    EXPECT_EQ(d.insert(d.begin(), none.begin(), none.end()), d.end());
    EXPECT_TRUE(d.empty());
}

TEST(Deque_Test, EraseEndWithOneElementPerBlock) {
    sgcl::deque<Page> big;
    EXPECT_EQ(big.erase(big.end()), big.end());
    for (int i = 0; i < 3; ++i) {
        big.emplace_back().bytes[0] = char('a' + i);   // the end reaches the end of the map
    }
    EXPECT_EQ(big.erase(big.end()), big.end());   // forms no iterator past the end, which would read past the map
    EXPECT_EQ(big.size(), 3u);
    EXPECT_EQ(big.erase(big.begin())->bytes[0], 'b');
    EXPECT_EQ(big.erase(big.begin(), big.end()), big.end());
    EXPECT_TRUE(big.empty());
    EXPECT_EQ(big.erase(big.end()), big.end());
}

TEST(Deque_Test, FillInsertOfAnElementOfTheDeque) {
    sgcl::deque<std::string> s = {"a", "b", "c"};
    s.insert(s.begin(), 2, s.back());   // the front half: the value copied first
    std::vector<std::string> expected = {"c", "c", "a", "b", "c"};
    EXPECT_EQ(std::vector<std::string>(s.begin(), s.end()), expected);
    s.insert(s.end(), 2, s.front());
    expected = {"c", "c", "a", "b", "c", "c", "c"};
    EXPECT_EQ(std::vector<std::string>(s.begin(), s.end()), expected);
    s.insert(s.begin() + 3, 2, s[2]);   // the middle: rotated after the pushes
    expected = {"c", "c", "a", "a", "a", "b", "c", "c", "c"};
    EXPECT_EQ(std::vector<std::string>(s.begin(), s.end()), expected);
    std::string outside = "z";
    s.insert(s.begin() + 1, 1, outside);   // not an element: no copy before the pushes
    EXPECT_EQ(s[1], "z");
    EXPECT_EQ(s.size(), 10u);
}
