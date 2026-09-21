//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// rooted: a value in a managed object of its own, held by a root, where a
// tracked_ptr may not lie: an exception object, a std container, a global.
#include "tests/types.h"

#include <exception>
#include <stdexcept>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        Node(const Node& o) : value(o.value) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    // A value with pointers inside, as an exception carries it
    struct Context {
        Context(string l, tracked_ptr<Node> n) : line(std::move(l)), node(std::move(n)) {}
        string line;
        tracked_ptr<Node> node;
    };

    struct parse_error : std::runtime_error {
        parse_error(const string& what, Context c)
        : std::runtime_error(what.c_str())
        , context(std::move(c)) {
        }
        rooted<Context> context;
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // Throws with the frame that made the node gone, and collects while
    // the exception is in flight: the node lives only through the rooted
    // in the exception object
    SGCL_NOINLINE void throw_and_collect() {
        off_frame([] {
            throw parse_error("unexpected token", Context(string("let x ="), make_tracked<Node>(42)));
        });
    }
}

TEST(Rooted_Tests, MadeInPlaceOrFromAValue) {
    off_frame([] {
        rooted<Context> a(std::in_place, string("a"), make_tracked<Node>(1));
        rooted<Context> b(Context(string("b"), make_tracked<Node>(2)));
        rooted c(string("c"));                                     // deduced: rooted<string>
        EXPECT_EQ(a->node->value, 1);
        EXPECT_EQ((*b).node->value, 2);
        EXPECT_EQ(*c, "c");
        EXPECT_EQ(a.get(), &*a);
        EXPECT_EQ(a.ptr().get(), a.get());
        EXPECT_EQ(Node::alive.load(), 2);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 0);                              // dropped with the rooted values
}

TEST(Rooted_Tests, ACopySharesAMoveEmpties) {
    off_frame([] {
        rooted<Node> a(std::in_place, 5);
        rooted<Node> b = a;                                         // a cell of its own, the same object
        EXPECT_EQ(a.get(), b.get());
        b->value = 6;
        EXPECT_EQ(a->value, 6);
        rooted<Node> c = std::move(a);
        EXPECT_EQ(c.get(), b.get());
        EXPECT_EQ(a.get(), nullptr);                                // moved from: empty
        a = c;                                                      // assigned to again
        EXPECT_EQ(a.get(), c.get());
        rooted<Node> d(*c);                                         // a copy of the value: another object
        EXPECT_NE(d.get(), c.get());
        EXPECT_EQ(d->value, 6);
        swap(c, d);
        EXPECT_EQ(d.get(), b.get());
        EXPECT_NE(c.get(), b.get());
        EXPECT_EQ(Node::alive.load(), 2);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 0);
}

// The reason it exists: an exception object with pointers inside, caught
// after a collection ran while it was in flight (a tracked_ptr member in
// the exception would trip the placement assertion of debug builds and
// be unprotected in release)
TEST(Rooted_Tests, KeepsAnExceptionsPayloadInFlight) {
    int seen = 0;
    off_frame([&] {                                                 // the reads in a frame of their own: a word they spill would root the node (7f74b62)
        try {
            throw_and_collect();
        } catch (const parse_error& e) {
            settle();                                               // the exception in flight, the frames that made it gone
            EXPECT_EQ(Node::alive.load(), 1);
            EXPECT_STREQ(e.what(), "unexpected token");
            EXPECT_EQ(e.context->line, "let x =");
            seen = e.context->node->value;
        }
    });
    EXPECT_EQ(seen, 42);
    settle();
    EXPECT_EQ(Node::alive.load(), 0);                              // the exception destroyed, the payload with it
}

// The copies the runtime makes: exception_ptr, rethrow, catch by value
TEST(Rooted_Tests, SurvivesExceptionPtrAndRethrow) {
    std::exception_ptr kept;
    off_frame([&] {
        try {
            throw_and_collect();
        } catch (...) {
            kept = std::current_exception();
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 1);                              // held by the exception in the exception_ptr
    int seen = 0;
    off_frame([&] {
        try {
            std::rethrow_exception(kept);
        } catch (parse_error e) {                                   // by value: a copy sharing the payload
            settle();
            seen = e.context->node->value;
        }
    });
    EXPECT_EQ(seen, 42);
    kept = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), 0);
}

// bad_expected_access carries its error the same way
TEST(Rooted_Tests, BadExpectedAccessCarriesItsError) {
    int seen = 0;
    off_frame([&] {
        try {
            expected<int, tracked_ptr<Node>> e(unexpect, make_tracked<Node>(9));
            e.value();
        } catch (const bad_expected_access<tracked_ptr<Node>>& e) {
            settle();
            seen = e.error()->value;
        }
    });
    EXPECT_EQ(seen, 9);
    settle();
    EXPECT_EQ(Node::alive.load(), 0);
}

// Where else a tracked_ptr may not lie: a std container on the heap
TEST(Rooted_Tests, InAStdContainer) {
    auto* v = new std::vector<rooted<Node>>();
    off_frame([&] {
        for (int i : range(3)) {
            v->emplace_back(std::in_place, i);
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), 3);
    EXPECT_EQ((*v)[2]->value, 2);
    delete v;
    settle();
    EXPECT_EQ(Node::alive.load(), 0);
}
