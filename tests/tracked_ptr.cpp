//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

TEST(TrackedGcTrackedPtr_Tests, DefaultConstructor) {
    { // stack
        tracked_ptr<int> ptr;
        EXPECT_EQ(ptr, nullptr);
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<int>>();
        EXPECT_EQ(*ptr, nullptr);
    }
}

TEST(TrackedGcTrackedPtr_Tests, NullConstructor) {
    { // stack
        tracked_ptr<int> ptr(nullptr);
        EXPECT_EQ(ptr, nullptr);
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<int>>(nullptr);
        EXPECT_EQ(*ptr, nullptr);
    }
}

TEST(TrackedGcTrackedPtr_Tests, UniqueConstructor) {
    { // stack
        tracked_ptr<Bar> ptr = make_tracked<Foo>(3);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(ptr->get_value(), 3);
    }
    { // heap
        auto ptr = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(3));
        ASSERT_NE(*ptr, nullptr);
        EXPECT_EQ((*ptr)->get_value(), 3);
    }
}

TEST(TrackedGcTrackedPtr_Tests, RawConstructor) {
    { // stack
        tracked_ptr<Foo> foo = make_tracked<Foo>(10);
        tracked_ptr<int> alias(&foo->value);
        ASSERT_NE(alias, nullptr);
        EXPECT_EQ(*alias, 10);
    }
    { // heap
        tracked_ptr<Foo> foo = make_tracked<Foo>(10);
        auto alias = make_tracked<tracked_ptr<int>>(&foo->value);
        ASSERT_NE(*alias, nullptr);
        EXPECT_EQ(**alias, 10);
    }
}

TEST(TrackedGcTrackedPtr_Tests, CopyConstructor) {
    tracked_ptr<int> ptr1 = make_tracked<int>(8);
    { // stack
        tracked_ptr<int> ptr2(ptr1);
        ASSERT_NE(ptr2, nullptr);
        EXPECT_EQ(*ptr2, 8);
    }
    { // heap
        auto ptr2 = make_tracked<tracked_ptr<int>>(ptr1);
        ASSERT_NE(*ptr2, nullptr);
        EXPECT_EQ(**ptr2, 8);
    }
}

TEST(TrackedGcTrackedPtr_Tests, CopyCastConstructor) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(4);
    { // stack
        tracked_ptr<Bar> bar = foo;
        ASSERT_NE(bar, nullptr);
        EXPECT_EQ(bar->get_value(), 4);
    }
    { // heap
        auto bar = make_tracked<tracked_ptr<Bar>>(foo);
        ASSERT_NE(*bar, nullptr);
        EXPECT_EQ((*bar)->get_value(), 4);
    }
}

TEST(TrackedGcTrackedPtr_Tests, MoveConstructor) {
    { // stack
        tracked_ptr<int> s = make_tracked<int>(8);
        auto h = make_tracked<tracked_ptr<int>>(make_tracked<int>(9));
        tracked_ptr<int> ss(std::move(s));
        ASSERT_NE(ss, nullptr);
        EXPECT_EQ(*ss, 8);
        ASSERT_NE(s, nullptr);
        tracked_ptr<int> sh(std::move(*h));
        ASSERT_NE(sh, nullptr);
        EXPECT_EQ(*sh, 9);
        ASSERT_NE(*h, nullptr);
    }
    { // heap
        tracked_ptr<int> s = make_tracked<int>(8);
        auto h = make_tracked<tracked_ptr<int>>(make_tracked<int>(9));
        auto hs = make_tracked<tracked_ptr<int>>(std::move(s));
        ASSERT_NE(*hs, nullptr);
        EXPECT_EQ(**hs, 8);
        ASSERT_NE(s, nullptr);
        auto hh = make_tracked<tracked_ptr<int>>(std::move(*h));
        ASSERT_NE(*hh, nullptr);
        EXPECT_EQ(**hh, 9);
        ASSERT_NE(*h, nullptr);
    }
}

TEST(TrackedGcTrackedPtr_Tests, MoveCastConstructor) {
    { // stack
        tracked_ptr<Foo> s = make_tracked<Foo>(4);
        auto h = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(5));
        tracked_ptr<Bar> ss(std::move(s));
        ASSERT_NE(ss, nullptr);
        EXPECT_EQ(ss->get_value(), 4);
        ASSERT_NE(s, nullptr);
        tracked_ptr<Bar> sh(std::move(*h));
        ASSERT_NE(sh, nullptr);
        EXPECT_EQ(sh->get_value(), 5);
        ASSERT_NE(*h, nullptr);
    }
    { // heap
        tracked_ptr<Foo> s = make_tracked<Foo>(4);
        auto h = make_tracked<tracked_ptr<Foo>>(make_tracked<Foo>(5));
        auto hs = make_tracked<tracked_ptr<Bar>>(std::move(s));
        ASSERT_NE(*hs, nullptr);
        EXPECT_EQ((*hs)->get_value(), 4);
        ASSERT_NE(s, nullptr);
        auto hh = make_tracked<tracked_ptr<Bar>>(std::move(*h));
        ASSERT_NE(*hh, nullptr);
        EXPECT_EQ((*hh)->get_value(), 5);
        ASSERT_NE(*h, nullptr);
    }
}

TEST(TrackedGcTrackedPtr_Tests, CopyAssignmentOperator) {
    tracked_ptr<int> ptr1 = make_tracked<int>(3);
    tracked_ptr<int> ptr2;
    ptr2 = ptr1;
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr2, 3);
}

TEST(TrackedGcTrackedPtr_Tests, CopyCastAssignmentOperator) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(5);
    tracked_ptr<Bar> bar;
    bar = foo;
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 5);
}

TEST(TrackedGcTrackedPtr_Tests, NullAssignmentOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(8);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 8);
    ptr = nullptr;
    EXPECT_EQ(ptr, nullptr);
}

TEST(TrackedGcTrackedPtr_Tests, UniqueCastAssignmentOperator) {
    auto foo = make_tracked<Foo>(11);
    tracked_ptr<Bar> bar;
    bar = std::move(foo);
    ASSERT_NE(bar, nullptr);
    EXPECT_EQ(bar->get_value(), 11);
}

TEST(TrackedGcTrackedPtr_Tests, VoidRefOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(3);
    ASSERT_NE(ptr, nullptr);
    tracked_ptr<void>& ref = ptr;
    ASSERT_NE(ref, nullptr);
}

TEST(TrackedGcTrackedPtr_Tests, BoolOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(5);
    EXPECT_TRUE(ptr);
    ptr = nullptr;
    EXPECT_FALSE(ptr);
}

TEST(TrackedGcTrackedPtr_Tests, IndirectionOperator) {
    tracked_ptr<int> ptr = make_tracked<int>(15);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, *ptr.get());
}

TEST(TrackedGcTrackedPtr_Tests, StructureDereferenceOperator) {
    tracked_ptr<Foo> ptr = make_tracked<Foo>(3);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, ptr.get()->value);
}

TEST(TrackedGcTrackedPtr_Tests, reset) {
    tracked_ptr<int> ptr = make_tracked<int>(9);
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

TEST(TrackedGcTrackedPtr_Tests, swap) {
    tracked_ptr<int> ptr1 = make_tracked<int>(2);
    tracked_ptr<int> ptr2 = make_tracked<int>(5);
    ptr1.swap(ptr2);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(*ptr1, 5);
    EXPECT_EQ(*ptr2, 2);
}

TEST(TrackedGcTrackedPtr_Tests, is) {
    tracked_ptr<Foo> foo = make_tracked<Foo>(6);
    tracked_ptr<Bar> bar = foo;
    EXPECT_TRUE(bar.is<Foo>());
    EXPECT_FALSE(bar.is<Bar>());
    tracked_ptr<int> alias(&foo->value);
    EXPECT_TRUE(alias.is<Foo>());
    EXPECT_FALSE(alias.is<int>());
}

TEST(TrackedGcTrackedPtr_Tests, as) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(8);
    tracked_ptr<Baz> baz = bar.as<Baz>();
    EXPECT_EQ(baz, nullptr);
    tracked_ptr<Foo> foo = bar.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 8);
    tracked_ptr<int> alias(&foo->value);
    *alias = 12;
    foo = alias.as<Foo>();
    ASSERT_NE(foo, nullptr);
    EXPECT_EQ(foo->value, 12);
}

TEST(TrackedGcTrackedPtr_Tests, type) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(10);
    EXPECT_EQ(bar.type(), typeid(Foo));
}

TEST(TrackedGcTrackedPtr_Tests, Comparisons) {
    tracked_ptr<Foo> a = make_tracked<Foo>(14);
    tracked_ptr<Foo> b = a;

    EXPECT_FALSE(a == nullptr);
    EXPECT_TRUE(a == b);

    EXPECT_FALSE(nullptr == a);
    EXPECT_TRUE(b == a);

    EXPECT_TRUE(a != nullptr);
    EXPECT_FALSE(a != b);

    EXPECT_TRUE(nullptr != a);
    EXPECT_FALSE(b != a);

    EXPECT_FALSE(a < nullptr);
    EXPECT_FALSE(a < b);

    EXPECT_TRUE(nullptr < a);
    EXPECT_FALSE(b < a);

    EXPECT_FALSE(a <= nullptr);
    EXPECT_TRUE(a <= b);

    EXPECT_TRUE(nullptr <= a);
    EXPECT_TRUE(b <= a);

    EXPECT_TRUE(a > nullptr);
    EXPECT_FALSE(a > b);

    EXPECT_FALSE(nullptr > a);
    EXPECT_FALSE(b > a);

    EXPECT_TRUE(a >= nullptr);
    EXPECT_TRUE(a >= b);

    EXPECT_FALSE(nullptr >= a);
    EXPECT_TRUE(b >= a);
}

TEST(TrackedGcTrackedPtr_Tests, Casts) {
    tracked_ptr<Bar> bar = make_tracked<Foo>(1);
    auto foo = static_pointer_cast<Foo>(bar);
    EXPECT_EQ(foo->value, 1);
    auto far = dynamic_pointer_cast<Far>(bar);
    EXPECT_EQ(far->value, 2);
    auto faz = dynamic_pointer_cast<Faz>(bar);
    EXPECT_EQ(faz->value, 3);
    tracked_ptr<const Bar> cbar = make_tracked<Foo>(5);
    auto pbar = const_pointer_cast<Bar>(cbar);
    EXPECT_EQ(pbar->get_value(), 5);
}

TEST(TrackedGcTrackedPtr_Tests, ToSharedHoldsTheObjectFromUnmanagedMemory) {
    struct Node {
        int value = 7;
        tracked_ptr<Node> next;
    };
    collector::force_collect(true);
    auto live0 = collector::get_live_object_count();
    std::vector<std::shared_ptr<Node>> unmanaged;   // a std container: where no tracked_ptr may live
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>();
        node->next = make_tracked<Node>();
        unmanaged.push_back(node.to_shared());
    });
    collector::clear_stack(SIZE_MAX);
    collector::force_collect(true);
    ASSERT_EQ(unmanaged.size(), 1u);
    off_frame([&] {
        EXPECT_EQ(unmanaged[0]->value, 7);
        ASSERT_NE(unmanaged[0]->next, nullptr);
        EXPECT_EQ(unmanaged[0]->next->value, 7);
    });
    EXPECT_EQ(collector::get_live_object_count(), live0 + 3);   // the node, its next, the holder
    auto copy = unmanaged[0];                        // shares the control block: no new holder
    unmanaged.clear();
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ(copy->value, 7);
    });
    EXPECT_EQ(collector::get_live_object_count(), live0 + 3);
    copy.reset();                                    // the last shared_ptr: the holder is released
    collector::clear_stack(SIZE_MAX);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), live0);
}

TEST(TrackedGcTrackedPtr_Tests, ToSharedOfNullAndOfAnAlias) {
    struct Node {
        int value = 7;
    };
    tracked_ptr<Node> null;
    EXPECT_EQ(null.to_shared(), nullptr);
    std::shared_ptr<int> value;
    {
        tracked_ptr node = make_tracked<Node>();
        tracked_ptr<int> alias(&node->value);
        value = alias.to_shared();                   // an alias into a member keeps the whole object
    }
    collector::clear_stack(SIZE_MAX);
    collector::force_collect(true);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 7);
}

TEST(TrackedGcTrackedPtr_Tests, ToSharedAcrossThreads) {
    struct Node {
        gc::atomic<int> hits = {0};
    };
    std::shared_ptr<Node> shared;
    {
        tracked_ptr node = make_tracked<Node>();
        shared = node.to_shared();
    }
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([shared] {              // a copy captured by a lambda run on another thread
            for (int i = 0; i < 1000; ++i) {
                ++shared->hits;
                if (i % 100 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }
    collector::force_collect(true);
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(shared->hits.load(), 4000);
}
