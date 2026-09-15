//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// function and move_only_function: std::function's interface, a closure
// with tracked pointers in a managed object of its own, followed by the
// collector.
#include "types.h"

#include <memory>
#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static std::atomic<int> alive = {0};
    };

    struct Holder {
        function<int()> get;
        tracked_ptr<Holder> next;
    };

    int twice(int x) {
        return 2 * x;
    }

    struct Counted {
        Counted() { ++alive; }
        Counted(const Counted&) { ++alive; }
        ~Counted() { --alive; }
        int operator()() const { return 1; }
        tracked_ptr<Node> node;
        inline static std::atomic<int> alive = {0};
    };

    // Inlined into the test's frame: a frame of its own would sit where
    // the dead frames were and keep their words (gc.cpp: live_after_collect)
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Function_Tests, TheInterfaceOfStdFunction) {
    static_assert(sizeof(function<int(int)>) == 32);
    function<int(int)> empty;
    EXPECT_FALSE(empty);
    EXPECT_TRUE(empty == nullptr);
    EXPECT_THROW(empty(1), bad_function_call);
    EXPECT_TRUE(empty.target_type() == typeid(void));
    function<int(int)> f = twice;
    EXPECT_EQ(f(2), 4);
    EXPECT_TRUE(f.target_type() == typeid(int (*)(int)));
    ASSERT_NE(f.target<int (*)(int)>(), nullptr);
    EXPECT_EQ(*f.target<int (*)(int)>(), &twice);
    EXPECT_EQ(f.target<int>(), nullptr);
    function<int(int)> g = [](int x) { return x + 1; };
    EXPECT_EQ(g(2), 3);
    int calls = 0;
    function<void()> counter = [&calls] { ++calls; };
    counter();
    counter();
    EXPECT_EQ(calls, 2);
    function<int(int)> copy = f;
    EXPECT_EQ(copy(3), 6);
    function<int(int)> moved = std::move(copy);
    EXPECT_FALSE(copy);
    EXPECT_EQ(moved(3), 6);
    moved = g;
    EXPECT_EQ(moved(3), 4);
    moved = nullptr;
    EXPECT_FALSE(moved);
    swap(f, g);
    EXPECT_EQ(f(2), 3);
    EXPECT_EQ(g(2), 4);
    function<int(int)> null_pointer = (int (*)(int))nullptr;
    EXPECT_FALSE(null_pointer);
    function<int(int)> from_empty = empty;                   // an empty function: nothing to hold
    EXPECT_FALSE(from_empty);
    function<int(int)> from_std = std::function<int(int)>(); // and an empty std::function
    EXPECT_FALSE(from_std);
    struct Getter {
        int value = 5;
        int operator()() const { return value; }
    } getter;
    function<int()> by_reference = std::ref(getter);       // the functor itself, not a copy
    getter.value = 6;
    EXPECT_EQ(by_reference(), 6);
    by_reference = std::ref(getter);
    EXPECT_EQ(by_reference(), 6);
    function deduced = twice;
    static_assert(std::is_same_v<decltype(deduced), function<int(int)>>);
    function deduced_lambda = [](double x) { return x; };
    static_assert(std::is_same_v<decltype(deduced_lambda), function<double(double)>>);
    struct Member { int v; int get() const { return v; } };
    function<int(const Member&)> member = &Member::get;
    EXPECT_EQ(member(Member{7}), 7);
    function<int(const Member&)> null_member = (int (Member::*)() const)nullptr;
    EXPECT_FALSE(null_member);
}

TEST(Function_Tests, AClosureWithPointersLivesInAManagedObject) {
    settle();
    const int before = Node::alive.load();
    function<int()> f;
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>(1);
        f = [node] { return node->value; };
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);   // the closure keeps the node
    EXPECT_EQ(f(), 1);
    function<int()> copy = f;                    // another managed object, the same node
    EXPECT_EQ(copy(), 1);
    off_frame([&] {
        f = nullptr;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);   // the copy still
    off_frame([&] {
        copy = [] { return 0; };
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Function_Tests, TheClosureObjectIsDestroyedAtOnce) {
    function<int()> f = Counted();
    EXPECT_EQ(Counted::alive.load(), 1);
    function<int()> g = f;
    EXPECT_EQ(Counted::alive.load(), 2);
    g = nullptr;
    EXPECT_EQ(Counted::alive.load(), 1);         // not waiting for a cycle
    f = [] { return 0; };
    EXPECT_EQ(Counted::alive.load(), 0);
    {
        function<int()> scoped = Counted();
        EXPECT_EQ(Counted::alive.load(), 1);
    }
    EXPECT_EQ(Counted::alive.load(), 0);
}

TEST(Function_Tests, ClosuresInsideManagedObjectsAreFollowed) {
    settle();
    const int before = Node::alive.load();
    constexpr int Count = 1000;
    tracked_ptr<Holder> list;
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr holder = make_tracked<Holder>();
            if (i % 2) {
                holder->get = [i] { return 0x10000 + i; };          // no pointers: inline
            } else {
                tracked_ptr node = make_tracked<Node>(i);
                holder->get = [node] { return node->value; };        // a managed closure
            }
            holder->next = list;
            list = holder;
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + Count / 2);
    int sum = 0;
    off_frame([&] {
        for (auto h = list; h; h = h->next) {
            sum += h->get() < 0x10000 ? 1 : 0;
        }
    });
    EXPECT_EQ(sum, Count / 2);
    list = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Function_Tests, AClosureCapturingItsOwnerIsACycle) {
    settle();
    const int before = Node::alive.load();
    struct Button {
        explicit Button(int v) : node(make_tracked<Node>(v)) {}
        tracked_ptr<Node> node;
        function<int()> on_click;
        move_only_function<int() const> once;
    };
    off_frame([&] {
        tracked_ptr button = make_tracked<Button>(1);
        button->on_click = [button] { return button->node->value; };   // the closure captures its owner
        button->once = [button] { return button->node->value; };
        EXPECT_EQ(button->on_click(), 1);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);                   // the cycle collected
}

TEST(Function_Tests, TheGcKindLivesAnywhere) {
    settle();
    const int before = Node::alive.load();
    auto* callbacks = new std::vector<gc::function<int()>>();
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            gc::tracked_ptr node = gc::make_tracked<Node>(i);
            callbacks->push_back([node] { return node->value; });
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 100);
    int sum = 0;
    for (auto& f : *callbacks) {
        sum += f();
    }
    EXPECT_EQ(sum, 99 * 100 / 2);
    delete callbacks;
    detail::cell_allocator.release();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(MoveOnlyFunction_Tests, TheInterfaceOfStdMoveOnlyFunction) {
    move_only_function<int(int)> empty;
    EXPECT_FALSE(empty);
    EXPECT_TRUE(empty == nullptr);
    move_only_function<int(int)> f = [p = std::make_unique<int>(3)](int x) { return x + *p; };   // move-only closure
    EXPECT_EQ(f(1), 4);
    move_only_function<int(int)> moved = std::move(f);
    EXPECT_FALSE(f);
    EXPECT_EQ(moved(1), 4);
    moved = nullptr;
    EXPECT_FALSE(moved);
    move_only_function<int(int)> in_place(std::in_place_type<std::negate<int>>);
    EXPECT_EQ(in_place(3), -3);
    static_assert(!std::is_copy_constructible_v<move_only_function<int(int)>>);
    move_only_function<int(int) const> c = [](int x) { return x; };
    const auto& cr = c;
    EXPECT_EQ(cr(5), 5);
    move_only_function<int(int) noexcept> n = [](int x) noexcept { return x; };
    static_assert(noexcept(n(1)));
    move_only_function<int(int) const noexcept> cn = [](int x) noexcept { return x; };
    EXPECT_EQ(cn(2), 2);
    int calls = 0;
    move_only_function<void()> mutating = [calls]() mutable { ++calls; };   // non-const call
    mutating();
    swap(mutating, mutating);
}

TEST(MoveOnlyFunction_Tests, AClosureWithPointersIsFollowed) {
    settle();
    const int before = Node::alive.load();
    move_only_function<int()> f;
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>(1);
        f = [node, p = std::make_unique<int>(2)] { return node->value + *p; };
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(f(), 3);
    off_frame([&] {
        f = nullptr;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}
