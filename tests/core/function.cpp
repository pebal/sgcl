//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// function and move_only_function: std::function's interface, a closure
// with tracked pointers in a managed object of its own, followed by the
// collector.
#include "tests/types.h"

#include <cstring>
#include <memory>
#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
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
        inline static sgcl::atomic<int> alive = {0};
    };

    // Inlined into the test's frame: a frame of its own would sit where
    // the dead frames were and keep their words (root_ptr.cpp: live_after_collect)
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // The storage's word: the second word of the object, after the
    // manager (value_storage.h); null when nothing lies in it and no node
    // is held
    template<class S>
    uintptr_t word_of(const S& s) noexcept {
        uintptr_t w;
        std::memcpy(&w, (const char*)&s + sizeof(void*), sizeof(w));
        return w;
    }

    // A callable whose copy throws, going to a node (it may hold a pointer)
    struct ThrowingCallable {
        ThrowingCallable() = default;
        ThrowingCallable(const ThrowingCallable&) { throw 1; }
        int operator()() const { return 0; }
        tracked_ptr<Node> node;
    };
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

TEST(Function_Tests, AReferenceWrapperIsHeldInsideTheFunction) {
    struct Getter {
        int value = 5;
        int operator()() const { return value; }
    } getter;
    // no managed allocation for it: the assignment is noexcept, as std's,
    // and a limit of one byte would make one throw
    auto limit = collector::get_memory_limit();
    collector::set_memory_limit(1);
    function<int()> f;
    f = std::ref(getter);
    collector::set_memory_limit(limit);
    static_assert(noexcept(f = std::ref(getter)));
    EXPECT_EQ(f(), 5);
    auto p = f.target<std::reference_wrapper<Getter>>();
    ASSERT_NE(p, nullptr);
    EXPECT_GE((const char*)p, (const char*)&f);                 // in the function's own buffer
    EXPECT_LT((const char*)p, (const char*)&f + sizeof(f));
    EXPECT_FALSE(detail::Heap::contains(p));                    // not in a node
    EXPECT_EQ(word_of(f), 0u);
    getter.value = 6;
    EXPECT_EQ(f(), 6);                                          // the functor itself, not a copy
    function<int()> copy = f;
    EXPECT_EQ(copy(), 6);
}

TEST(Function_Tests, AClosureWhoseConstructorThrowsLeavesTheFunctionAsItWas) {
    function<int()> f = [] { return 1; };
    ThrowingCallable c;
    EXPECT_THROW(f = c, int);                                   // the copy into a node throws, in a temporary function
    EXPECT_EQ(f(), 1);
    EXPECT_THROW(function<int()>{c}, int);                     // braces: with parentheses the statement declares a c
    EXPECT_THROW(move_only_function<int()>(std::in_place_type<ThrowingCallable>, c), int);   // an emplace: the same path
    move_only_function<int()> m = [] { return 2; };
    EXPECT_THROW(m = c, int);
    EXPECT_EQ(m(), 2);
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

TEST(MoveOnlyFunction_Tests, FromAnEmptyOneOfAnotherSignatureItIsEmpty) {
    move_only_function<int(int)> empty;
    move_only_function<long(int)> from_empty = std::move(empty);   // another signature: the empty function is the callable
    EXPECT_FALSE(from_empty);
    EXPECT_TRUE(from_empty == nullptr);
    move_only_function<int(int) const> from_const_empty = move_only_function<int(int) const noexcept>();
    EXPECT_FALSE(from_const_empty);
    move_only_function<int(int)> from_std_empty = std::function<int(int)>();   // and an empty std::function
    EXPECT_FALSE(from_std_empty);
    move_only_function<long(int)> from_full = move_only_function<int(int)>([](int x) { return x + 1; });
    ASSERT_TRUE(from_full);
    EXPECT_EQ(from_full(2), 3);
    from_full = move_only_function<int(int)>();
    EXPECT_FALSE(from_full);
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
