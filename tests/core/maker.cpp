//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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

