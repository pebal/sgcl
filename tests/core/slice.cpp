//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <array>
#include <cstring>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct Block {
        int values[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        char text[16] = "  hello world  ";
    };

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Slice_Tests, UnmanagedMemoryGivesASliceWithoutAnOwner) {
    static_assert(sizeof(slice<int>) == 3 * sizeof(void*));
    int raw[4] = {1, 2, 3, 4};
    slice s(raw);
    static_assert(std::is_same_v<decltype(s), slice<int>>);
    EXPECT_FALSE(s.owned());
    EXPECT_EQ(s.owner(), nullptr);
    EXPECT_EQ(s.size(), 4u);
    EXPECT_EQ(s[2], 3);
    s[2] = 30;                                  // writable: the elements are T
    EXPECT_EQ(raw[2], 30);
    std::vector<int> v = {5, 6, 7};
    slice sv(v);
    EXPECT_EQ(sv.front(), 5);
    EXPECT_EQ(sv.back(), 7);
    const std::vector<int>& cv = v;
    slice csv(cv);
    static_assert(std::is_same_v<decltype(csv), slice<const int>>);
    std::array<int, 2> a = {8, 9};
    slice sa(a);
    EXPECT_EQ(sa.size(), 2u);
    std::span<int> sp(raw);
    slice ssp(sp);
    EXPECT_EQ(ssp.data(), raw);
    std::span<const int> back = csv;            // to a std span, for a std interface
    EXPECT_EQ(back.size(), 3u);
    slice<int> empty;
    EXPECT_TRUE(empty.empty() && !empty.owned() && empty.begin() == empty.end());
    slice<const int> widened = s;               // T to const T
    EXPECT_EQ(widened[2], 30);
}

TEST(Slice_Tests, AnArrayGivesASliceWithoutAnOwner) {
    sgcl::array<int, 4> a = {1, 2, 3, 4};
    slice s(a);
    static_assert(std::is_same_v<decltype(s), slice<int>>);
    EXPECT_FALSE(s.owned());
    EXPECT_EQ(s.data(), a.data());
    EXPECT_EQ(s.size(), 4u);
    s[1] = 20;
    EXPECT_EQ(a[1], 20);
    const sgcl::array<int, 4>& ca = a;
    slice cs(ca);
    static_assert(std::is_same_v<decltype(cs), slice<const int>>);
    auto sum = [](slice<const int> v) { int n = 0; for (int x : v) n += x; return n; };
    EXPECT_EQ(sum(a), 1 + 20 + 3 + 4);            // a parameter of a slice takes the array
    EXPECT_EQ(a.as_slice(2).size(), 2u);
    EXPECT_THROW((void)a.as_slice(5), sgcl::out_of_range);
    sgcl::array<int, 0> none;
    EXPECT_TRUE(none.as_slice().empty());
    sgcl::array<byte, 8> bytes = {};
    slice<byte> b = bytes;
    EXPECT_EQ(b.size(), 8u);
}

TEST(Slice_Tests, SubslicesShareTheOwner) {
    sgcl::tracked_ptr b = make_tracked<Block>();
    slice<int> s(b, b->values, 8);
    EXPECT_TRUE(s.owned());
    EXPECT_EQ(s.owner().get(), b.get());
    auto mid = s.subslice(2, 3);
    EXPECT_EQ(mid.size(), 3u);
    EXPECT_EQ(mid[0], 2);
    EXPECT_EQ(mid.owner(), s.owner());
    EXPECT_EQ(s.first(2).size(), 2u);
    EXPECT_EQ(s.last(2)[1], 7);
    EXPECT_EQ(s.subspan(6).size(), 2u);
    EXPECT_THROW(s.subslice(9), std::out_of_range);
    auto narrowed = s;
    narrowed.remove_prefix(1);
    narrowed.remove_suffix(1);
    EXPECT_EQ(narrowed.size(), 6u);
    EXPECT_EQ(narrowed.front(), 1);
    EXPECT_TRUE(std::equal(narrowed.rbegin(), narrowed.rend(), std::vector<int>{6, 5, 4, 3, 2, 1}.begin()));
    slice<int> other(b, b->values, 8);
    EXPECT_EQ(s, other);                        // the same range
    int copy[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(s, slice<int>(copy));             // equal elements, another range
    EXPECT_NE(mid, s);
}

TEST(Slice_Tests, TheOwnerIsKeptAlive) {
    settle();
    EXPECT_EQ(Int::counter, 0u);
    struct Holder {
        Int items[4] = {Int(1), Int(2), Int(3), Int(4)};
    };
    slice<const Int> kept;
    off_frame([&] {
        sgcl::tracked_ptr h = make_tracked<Holder>();
        kept = slice<const Int>(h, h->items + 1, 2);   // the owner arrives by assignment: the slice roots it
    });
    settle();
    EXPECT_EQ(Int::counter, 4u);                // the holder lives: the slice is its only root
    off_frame([&] {                             // the reads off the test frame: a reference into the holder left in
        EXPECT_EQ(kept[0], 2);                  // a spilled word would root it under the conservative scan (TSan's
        EXPECT_EQ(kept[1], 3);                  // frames showed it)
        kept = slice<const Int>();
    });
    settle();
    EXPECT_EQ(Int::counter, 0u);                // and dies once no slice holds it
}

TEST(Slice_Tests, ADeadSliceInAContainerRootsNothing) {
    // The raw begin and end of a slice inside a managed buffer look like
    // pointers to the collector's map: the destructor nulls them, so a
    // slot a slice was destroyed in keeps no object alive
    settle();
    struct Holder {
        Int items[2] = {Int(1), Int(2)};
    };
    sgcl::vector<slice<const Int>> slices;
    off_frame([&] {
        for (int i = 0; i < 3; ++i) {
            sgcl::tracked_ptr h = make_tracked<Holder>();
            slices.push_back(slice<const Int>(h, h->items, 2));
        }
    });
    settle();
    EXPECT_EQ(Int::counter, 6u);
    off_frame([&] {
        slices.clear();                         // the slots destroyed, their words null
    });
    settle();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(Slice_Tests, TextSlicesHaveTheStringInterface) {
    sgcl::tracked_ptr b = make_tracked<Block>();
    slice<const char> t(b, b->text, std::strlen(b->text));
    EXPECT_EQ(t, "  hello world  ");
    auto trimmed = t.trim();
    EXPECT_EQ(trimmed, "hello world");
    EXPECT_EQ(trimmed.owner(), t.owner());      // trimming makes slices of the same owner
    EXPECT_EQ(t.trim_left(), "hello world  ");
    EXPECT_EQ(t.trim_right(), "  hello world");
    EXPECT_EQ(trimmed.trim_prefix("hello "), "world");
    EXPECT_EQ(trimmed.trim_suffix(" world"), "hello");
    EXPECT_EQ(trimmed.trim_prefix("x"), trimmed);
    EXPECT_TRUE(trimmed.starts_with("hello") && trimmed.ends_with('d') && trimmed.contains("lo w"));
    EXPECT_EQ(trimmed.find('o'), 4u);
    EXPECT_EQ(trimmed.rfind('o'), 7u);
    EXPECT_EQ(trimmed.find_first_of("xyzw"), 6u);
    EXPECT_EQ(trimmed.substr(6), "world");
    EXPECT_EQ(trimmed.substr(6).owner(), t.owner());
    EXPECT_THROW(trimmed.substr(12), std::out_of_range);
    EXPECT_EQ(trimmed.compare("hello world"), 0);
    EXPECT_TRUE(trimmed < "hello!" && (trimmed <=> "hello world") == 0);
    EXPECT_EQ(trimmed.str(), "hello world");
    std::string_view v = trimmed;               // to the std view
    EXPECT_EQ(v, "hello world");
    EXPECT_EQ(trimmed.at(0), 'h');
    EXPECT_THROW(trimmed.at(11), std::out_of_range);
    EXPECT_EQ(std::hash<string_slice>()(trimmed), string("hello world").hash());
    string s(trimmed);                          // a string of a slice that is not a string's: a copy
    EXPECT_EQ(s, "hello world");
    EXPECT_NE(s.object(), b.get());
    std::ostringstream out;
    out << trimmed;
    EXPECT_EQ(out.str(), "hello world");
    slice<const char> from_view(std::string_view("abc"));   // a std view: no owner
    EXPECT_FALSE(from_view.owned());
    EXPECT_EQ(from_view, "abc");
}

TEST(Slice_Tests, BytesOfASlice) {
    sgcl::tracked_ptr b = make_tracked<Block>();
    slice<int> s(b, b->values, 8);
    auto bytes = as_bytes(s);
    static_assert(std::is_same_v<decltype(bytes), slice<const byte>>);
    EXPECT_EQ(bytes.size(), 8 * sizeof(int));
    EXPECT_EQ(bytes.owner(), s.owner());
    auto writable = as_writable_bytes(s);
    writable[4] = byte(9);                 // the low byte of values[1] on a little-endian machine
    EXPECT_EQ(b->values[1] & 0xFF, 9);
}

TEST(Slice_Tests, AnOwnedSliceOnAFreshThreadRegistersIt) {
    // A thread that never made a tracked_ptr receives an owned slice: the
    // assignment registers the thread's stack, so the slice roots the
    // object from there
    settle();
    struct Holder {
        Int items[1] = {Int(7)};
    };
    sgcl::atomic<bool> ready = {false};
    sgcl::atomic<bool> go = {false};
    sgcl::root_ptr<Holder> shared;
    off_frame([&] {
        shared = make_tracked<Holder>();
    });
    std::thread t([&] {
        slice<const Int> local;
        local = slice<const Int>(shared.ptr(), shared->items, 1);   // the first managed word on this thread's stack
        ready = true;
        while (!go.load()) {
            std::this_thread::yield();
        }
        EXPECT_EQ(local[0], 7);
    });
    while (!ready.load()) {
        std::this_thread::yield();
    }
    off_frame([&] {
        shared = nullptr;                       // the thread's slice is the only holder now
    });
    settle();
    EXPECT_EQ(Int::counter, 1u);
    go = true;
    t.join();
}
