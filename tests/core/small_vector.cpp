//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// detail::SmallVector: the inline part and the block, the copy of what was
// written, the move in both states, and every byte let go of shown to the
// policy (a counting policy that stands for the wiping one: each wipe and
// each block given back is checked to cover exactly the elements dropped).
#include "tests/types.h"
#include "sgcl/core/detail/small_vector.h"

#include <cstdint>
#include <cstring>
#include <vector>

using sgcl::detail::SmallVector;

namespace {
    // What the vector let go of: bytes wiped in place, blocks given back
    // (each marked with 0xEE, so a read of freed or wiped bytes shows)
    struct Counting {
        static inline size_t wiped = 0;
        static inline size_t blocks_out = 0;
        static inline size_t blocks_back = 0;
        static inline size_t bytes_back = 0;

        static void reset() {
            wiped = blocks_out = blocks_back = bytes_back = 0;
        }

        static void* allocate(size_t bytes) {
            ++blocks_out;
            return ::operator new(bytes);
        }

        static void deallocate(void* p, size_t bytes) noexcept {
            std::memset(p, 0xEE, bytes);
            ++blocks_back;
            bytes_back += bytes;
            ::operator delete(p, bytes);
        }

        static void wipe(void* p, size_t bytes) noexcept {
            std::memset(p, 0xEE, bytes);
            wiped += bytes;
        }
    };

    using V = SmallVector<uint32_t, 4, Counting>;

    V filled(uint32_t n) {
        V v;
        for (uint32_t i = 0; i < n; ++i) {
            v.push_back(i * 10);
        }
        return v;
    }

    void expect_values(const V& v, uint32_t n) {
        ASSERT_EQ(v.size(), n);
        for (uint32_t i = 0; i < n; ++i) {
            EXPECT_EQ(v[i], i * 10);
        }
    }

    struct SmallVector_Tests : ::testing::Test {
        void SetUp() override {
            Counting::reset();
        }
    };
}

TEST_F(SmallVector_Tests, InlineThenOneBlock) {
    V v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), 4u);
    for (uint32_t i = 0; i < 4; ++i) {
        v.push_back(i * 10);
    }
    EXPECT_EQ(Counting::blocks_out, 0u);
    const uint32_t* inline_data = v.data();
    v.push_back(40);   // past the inline part: all of it moved to a block of 8
    EXPECT_EQ(Counting::blocks_out, 1u);
    EXPECT_EQ(v.capacity(), 8u);
    EXPECT_NE(v.data(), inline_data);
    EXPECT_EQ(Counting::wiped, 4 * sizeof(uint32_t));   // the inline elements the growth left
    for (uint32_t i = 5; i < 9; ++i) {
        v.push_back(i * 10);
    }
    EXPECT_EQ(v.capacity(), 16u);   // twice again
    EXPECT_EQ(Counting::blocks_back, 1u);
    EXPECT_EQ(Counting::bytes_back, 8 * sizeof(uint32_t));
    expect_values(v, 9);
    EXPECT_EQ(v.end() - v.begin(), 9);
}

TEST_F(SmallVector_Tests, PushBackOfItsOwnElementAcrossAGrowth) {
    V v = filled(4);
    v.push_back(v[1]);   // the argument lives in the inline part the growth wipes
    EXPECT_EQ(v[4], 10u);
}

TEST_F(SmallVector_Tests, CopyTakesOnlyWhatWasWritten) {
    V a = filled(3);
    V b(a);
    expect_values(b, 3);
    EXPECT_EQ(Counting::blocks_out, 0u);
    V big = filled(7);
    V c(big);   // a block of exactly 7
    expect_values(c, 7);
    EXPECT_EQ(c.capacity(), 7u);
    V d = filled(2);
    d = big;
    expect_values(d, 7);
    size_t wiped = Counting::wiped;
    d = a;   // into the block it has: the 4 dropped elements wiped
    expect_values(d, 3);
    EXPECT_EQ(Counting::wiped - wiped, 4 * sizeof(uint32_t));
    EXPECT_EQ(d.capacity(), 7u);
}

TEST_F(SmallVector_Tests, MoveOfInlineWipesTheSource) {
    V a = filled(3);
    Counting::reset();
    V b(std::move(a));
    expect_values(b, 3);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(Counting::wiped, 3 * sizeof(uint32_t));
    EXPECT_EQ(Counting::blocks_out, 0u);
}

TEST_F(SmallVector_Tests, MoveOfABlockTakesIt) {
    V a = filled(6);
    const uint32_t* block = a.data();
    V b(std::move(a));
    EXPECT_EQ(b.data(), block);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(a.capacity(), 4u);
    expect_values(b, 6);
    V c = filled(2);
    Counting::reset();
    c = std::move(b);   // its own two inline elements wiped first
    EXPECT_EQ(c.data(), block);
    EXPECT_EQ(Counting::wiped, 2 * sizeof(uint32_t));
    expect_values(c, 6);
    V d = filled(5);
    Counting::reset();
    d = std::move(c);   // its own block given back
    EXPECT_EQ(Counting::blocks_back, 1u);
    expect_values(d, 6);
}

TEST_F(SmallVector_Tests, ResizeZeroesGrowsExactlyAndWipesTheDropped) {
    V v(3);
    ASSERT_EQ(v.size(), 3u);
    for (auto x : v) {
        EXPECT_EQ(x, 0u);
    }
    v[0] = 7;
    v.resize(10);   // a block of exactly 10
    EXPECT_EQ(v.capacity(), 10u);
    EXPECT_EQ(v[0], 7u);
    for (size_t i = 1; i < 10; ++i) {
        EXPECT_EQ(v[i], 0u);
    }
    size_t wiped = Counting::wiped;
    v.resize(2);
    EXPECT_EQ(Counting::wiped - wiped, 8 * sizeof(uint32_t));
    EXPECT_EQ(v.capacity(), 10u);   // the room kept
    v.resize(5);
    EXPECT_EQ(v[2], 0u);   // grown back: zero, not what was there
    EXPECT_EQ(v[4], 0u);
    v.reserve(20);
    EXPECT_EQ(v.capacity(), 20u);
    EXPECT_EQ(v.size(), 5u);
}

TEST_F(SmallVector_Tests, ClearWipesAndKeepsTheBlock) {
    V v = filled(6);
    size_t cap = v.capacity();
    Counting::reset();
    v.clear();
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), cap);
    EXPECT_EQ(Counting::wiped, 6 * sizeof(uint32_t));
    EXPECT_EQ(Counting::blocks_back, 0u);
}

TEST_F(SmallVector_Tests, EraseFrontInBothStates) {
    V v = filled(4);
    v.erase_front(3);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 30u);
    V w = filled(9);
    Counting::reset();
    w.erase_front(5);
    ASSERT_EQ(w.size(), 4u);
    for (uint32_t i = 0; i < 4; ++i) {
        EXPECT_EQ(w[i], (i + 5) * 10);
    }
    EXPECT_EQ(Counting::wiped, 5 * sizeof(uint32_t));   // the vacated end
    w.erase_front(4);
    EXPECT_TRUE(w.empty());
}

TEST_F(SmallVector_Tests, AppendGrowsToWhatIsNeeded) {
    V v = filled(2);
    uint32_t more[9] = {20, 30, 40, 50, 60, 70, 80, 90, 100};
    v.append(more, 9);
    EXPECT_EQ(v.capacity(), 11u);   // more than twice 4
    expect_values(v, 11);
    v.append(more, 0);
    EXPECT_EQ(v.size(), 11u);
}

TEST_F(SmallVector_Tests, DestructorLetsGoOfEverything) {
    {
        V v = filled(3);
        Counting::reset();
    }
    EXPECT_EQ(Counting::wiped, 3 * sizeof(uint32_t));
    {
        V v = filled(12);
        Counting::reset();
    }
    EXPECT_EQ(Counting::blocks_back, 1u);
    EXPECT_EQ(Counting::bytes_back, 16 * sizeof(uint32_t));
}

TEST_F(SmallVector_Tests, AgainstStdVector) {
    // a fixed sequence of every operation, checked after each step
    V v;
    std::vector<uint32_t> model;
    uint32_t x = 1;
    auto check = [&] {
        ASSERT_EQ(v.size(), model.size());
        for (size_t i = 0; i < model.size(); ++i) {
            ASSERT_EQ(v[i], model[i]);
        }
    };
    for (int round = 0; round < 200; ++round) {
        x = x * 1103515245u + 12345u;
        switch ((x >> 16) % 6) {
        case 0:
        case 1:
            v.push_back(x);
            model.push_back(x);
            break;
        case 2: {
            size_t n = (x >> 8) % 12;
            v.resize(n);
            model.resize(n);
            break;
        }
        case 3: {
            size_t n = model.empty() ? 0 : (x >> 8) % (model.size() + 1);
            v.erase_front(n);
            model.erase(model.begin(), model.begin() + n);
            break;
        }
        case 4: {
            V copy(v);
            v = std::move(copy);
            break;
        }
        case 5: {
            uint32_t more[3] = {x, x + 1, x + 2};
            v.append(more, 3);
            model.insert(model.end(), more, more + 3);
            break;
        }
        }
        check();
    }
}
