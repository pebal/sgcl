//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// string: the builders are one allocation. The program's operator new
// counts the allocations of the thread that asks (a count of its own per
// thread, so that the collector's threads do not add to it), as the codec
// module's probe does: a builder that gathered its text in a std::string
// before the string was made shows up here as a plain allocation.
#include "tests/types.h"

#include <cstdlib>
#include <new>
#include <cstring>
#include <string>

namespace {
    thread_local size_t news = 0;
}

void* operator new(size_t n) {
    ++news;
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void* operator new[](size_t n) {
    ++news;
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

namespace {
    // The plain allocations of n calls of f on this thread
    template<class F>
    size_t plain_allocations(size_t n, F&& f) {
        size_t before = news;
        for (size_t i = 0; i < n; ++i) {
            f();
        }
        return news - before;
    }
}

// join, replace, repeat and operator+ are one allocation: none of their own
// in plain memory (a thousand calls count a few, the managed allocator's
// own now and then, where a staging std::string was one or more a call),
// and in managed memory what as many strings of the same text made
// from a view take (one object of the exact class each; within a few
// pages, the granularity of that count)
TEST(StringAllocation_Tests, BuildersAreOneAllocation) {
    sgcl::vector<string> parts;
    for (int i = 0; i < 10; ++i) {
        parts.push_back(string("part-number-") + to_string(i));
    }
    string ab = string("ab-----------------").repeat(10);
    string a = string("abcdefghijklmnopqrstuvwxyz0123456789");
    EXPECT_LT(plain_allocations(1000, [&] { (void)string::join(parts, ", "); }), 100u);
    EXPECT_LT(plain_allocations(1000, [&] { (void)ab.replace("ab", "xyz"); }), 100u);
    EXPECT_LT(plain_allocations(1000, [&] { (void)a.repeat(10); }), 100u);
    EXPECT_LT(plain_allocations(1000, [&] { (void)(a + a); }), 100u);
    std::string joined_text(string::join(parts, ", ").view());
    std::string replaced_text(ab.replace("ab", "xyz").view());
    const size_t n = 20000;
    const size_t slack = 3 * config::page_size;
    size_t joined = managed_bytes_of(n, [&] { (void)string::join(parts, ", "); });
    size_t made = managed_bytes_of(n, [&] { (void)string(joined_text); });
    EXPECT_GT(joined, 0u);
    EXPECT_LE(joined, made + slack);
    EXPECT_GE(joined + slack, made);
    size_t replaced = managed_bytes_of(n, [&] { (void)ab.replace("ab", "xyz"); });
    size_t made_replaced = managed_bytes_of(n, [&] { (void)string(replaced_text); });
    EXPECT_LE(replaced, made_replaced + slack);
    EXPECT_GE(replaced + slack, made_replaced);
}

// concat: the pieces in order, whatever they are (a string, a slice, a
// std view, a literal, a character), in one allocation of the sum of
// their lengths, none in plain memory
TEST(StringAllocation_Tests, ConcatIsOneAllocation) {
    string host = "example.com";
    std::string_view scheme = "https";
    string path = "/a/b/c";
    auto part = path.as_slice(0, 2);
    EXPECT_EQ(string::concat(scheme, "://", host, ':', to_string(8443), part), "https://example.com:8443/a");
    EXPECT_EQ(string::concat(host), host);
    EXPECT_EQ(string::concat('x'), "x");
    EXPECT_TRUE(string::concat("", string()).empty());
    EXPECT_LT(plain_allocations(1000, [&] { (void)string::concat(scheme, "://", host, ':', path); }), 100u);
    std::string text = std::string(string::concat(scheme, "://", host, ':', path).view());
    const size_t n = 20000;
    const size_t slack = 3 * config::page_size;
    size_t made = managed_bytes_of(n, [&] { (void)string::concat(scheme, "://", host, ':', path); });
    size_t one = managed_bytes_of(n, [&] { (void)string(text); });
    EXPECT_GT(made, 0u);
    EXPECT_LE(made, one + slack);
    EXPECT_GE(made + slack, one);
}

// println's line is one string: the text and its '\n' made once (none in
// plain memory, and in managed memory what a string of that line made from
// a view takes), short on the stack's first pass and long through a second
// pass straight into the string
TEST(StringAllocation_Tests, PrintlnsLineIsOneString) {
    EXPECT_EQ(io::detail::line_of("{} and {}", 1, "x"), "1 and x\n");
    std::string long_text(600, 'l');
    EXPECT_EQ(io::detail::line_of("[{}]", long_text).view(), "[" + long_text + "]\n");
    EXPECT_EQ(*io::detail::line_of(txt::runtime("{}!"), 5), "5!\n");
    EXPECT_FALSE(io::detail::line_of(txt::runtime("{} {}"), 5).has_value());
    EXPECT_LT(plain_allocations(1000, [&] { (void)io::detail::line_of("{} and {}", 12345, "some text"); }), 100u);
    const size_t n = 20000;
    const size_t slack = 3 * config::page_size;
    size_t line = managed_bytes_of(n, [&] { (void)io::detail::line_of("{} and {}", 12345, "some text"); });
    size_t one = managed_bytes_of(n, [&] { (void)string("12345 and some text\n"); });
    EXPECT_GT(line, 0u);
    EXPECT_LE(line, one + slack);
    EXPECT_GE(line + slack, one);
}

// The two steps of a bounded string (StringAccess::unfilled and finish),
// for a writing that waits: the characters written where unfilled put
// them, the string of the ones used; none used is the empty string, less
// than half of the room a copy into the exact class
TEST(StringAllocation_Tests, AStringFinishedAfterItsWriting) {
    auto room = sgcl::detail::StringAccess::unfilled<string>(10);
    std::memcpy(room.chars, "0123456789", 10);
    string all = sgcl::detail::StringAccess::finish<string>(std::move(room), 10);
    EXPECT_EQ(all, "0123456789");
    auto half = sgcl::detail::StringAccess::unfilled<string>(1000);
    std::memcpy(half.chars, "abc", 3);
    string three = sgcl::detail::StringAccess::finish<string>(std::move(half), 3);
    EXPECT_EQ(three, "abc");
    EXPECT_EQ(three.c_str()[3], '\0');
    auto none = sgcl::detail::StringAccess::unfilled<string>(8);
    EXPECT_TRUE(sgcl::detail::StringAccess::finish<string>(std::move(none), 0).empty());
    auto zero = sgcl::detail::StringAccess::unfilled<string>(0);
    EXPECT_TRUE(sgcl::detail::StringAccess::finish<string>(std::move(zero), 0).empty());
}
