//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The collector's diagnostics: live objects by type, the phases of a cycle.
#include "tests/types.h"

namespace {
    struct Apple { long weight = 1; };
    struct Pear { long a = 1, b = 2, c = 3; };

    template<class T>
    const collector::type_statistics* find(const std::vector<collector::type_statistics>& stats, bool buffers = false) {
        for (auto& s : stats) {
            if (*s.type == typeid(T) && s.buffers == buffers) {
                return &s;
            }
        }
        return nullptr;
    }
}

TEST(Statistics_Tests, LiveObjectsByType) {
    sgcl::vector<tracked_ptr<Apple>> apples;
    sgcl::vector<tracked_ptr<Pear>> pears;
    for (int i = 0; i < 100; ++i) {
        apples.push_back(make_tracked<Apple>());
    }
    for (int i = 0; i < 7; ++i) {
        pears.push_back(make_tracked<Pear>());
    }
    auto stats = collector::get_type_statistics();
    auto apple = find<Apple>(stats);
    auto pear = find<Pear>(stats);
    ASSERT_TRUE(apple);
    ASSERT_TRUE(pear);
    EXPECT_EQ(apple->live_objects, 100u);
    EXPECT_EQ(pear->live_objects, 7u);
    EXPECT_GE(apple->object_size, sizeof(Apple));
    EXPECT_EQ(apple->live_bytes, 100 * apple->object_size);
    EXPECT_GE(apple->pages, 1u);
    EXPECT_FALSE(apple->buffers);
    // sorted by bytes
    for (size_t i = 1; i < stats.size(); ++i) {
        EXPECT_GE(stats[i - 1].live_bytes, stats[i].live_bytes);
    }
    apples.clear();
    stats = collector::get_type_statistics();
    apple = find<Apple>(stats);
    EXPECT_TRUE(!apple || apple->live_objects == 0u);
    EXPECT_EQ(find<Pear>(stats)->live_objects, 7u);
}

TEST(Statistics_Tests, BuffersByElementType) {
    sgcl::vector<tracked_ptr<Apple>> pointers(10);
    sgcl::vector<Pear> pears(1000);       // a buffer past a page: a range of pages
    auto stats = collector::get_type_statistics();
    auto buffer = find<tracked_ptr<Apple>[]>(stats, true);   // a buffer's type is the array type
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->live_objects, 1u);
    EXPECT_EQ(buffer->object_size, sizeof(tracked_ptr<Apple>));
    EXPECT_GE(buffer->live_bytes, 10 * sizeof(tracked_ptr<Apple>));
    EXPECT_EQ(buffer->pages, 0u);
    auto big = find<Pear[]>(stats, true);
    ASSERT_TRUE(big);
    EXPECT_EQ(big->live_objects, 1u);
    EXPECT_GE(big->live_bytes, 1000 * sizeof(Pear));
}

TEST(Statistics_Tests, ThePhasesOfTheLastCycle) {
    collector::force_collect(true);
    auto s = collector::get_statistics();
    double total = 0;
    for (int i = 0; i < 8; ++i) {
        EXPECT_GE(s.phases_ms[i], 0.0);
        total += s.phases_ms[i];
    }
    EXPECT_LE(total, s.last_cycle_ms + 1.0);   // the phases add up to the cycle, give or take the clock
    EXPECT_STREQ(collector::phase_names[3], "marking");
}
