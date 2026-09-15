//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// string: an immutable string on the managed heap, one word, shared by
// copying, compared and hashed by its contents, no destructor.
#include "types.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    size_t live_string_objects() {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (std::string(s.type->name()).find("StringSlot") != std::string::npos || (s.buffers && *s.type == typeid(unsigned char[]))) {
                n += s.live_objects;
            }
        }
        return n;
    }
}

TEST(String_Tests, OneWordMadeFromWhatAStdStringIsMadeOf) {
    static_assert(sizeof(string) == sizeof(void*));
    string empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.object(), nullptr);        // nothing allocated
    EXPECT_EQ(*empty.data(), '\0');
    EXPECT_EQ(empty, "");
    string literal = "hello";
    string counted("hello, world", 5);
    string from_view = std::string_view("hello");
    std::string std_s = "hello";
    string from_std(std_s);
    string repeated(3, 'x');
    string from_range(std_s.begin(), std_s.end());
    string from_list{'h', 'i'};
    EXPECT_EQ(literal.size(), 5u);
    EXPECT_EQ(literal, "hello");
    EXPECT_EQ(counted, literal);
    EXPECT_EQ(from_view, literal);
    EXPECT_EQ(from_std, literal);
    EXPECT_EQ(repeated, "xxx");
    EXPECT_EQ(from_range, std_s);
    EXPECT_EQ(from_list, "hi");
    EXPECT_STREQ(literal.c_str(), "hello");    // terminated
    EXPECT_EQ(literal[5], '\0');
    EXPECT_EQ(literal.at(1), 'e');
    EXPECT_THROW(literal.at(5), std::out_of_range);
    EXPECT_EQ(literal.front(), 'h');
    EXPECT_EQ(literal.back(), 'o');
    EXPECT_EQ(std::string(literal.begin(), literal.end()), "hello");
    EXPECT_EQ(std::string(literal.rbegin(), literal.rend()), "olleh");
    EXPECT_EQ(literal.str(), "hello");
    EXPECT_EQ(std::string_view(literal), "hello");
    literal = "other";
    EXPECT_EQ(literal, "other");
    literal = std_s;
    EXPECT_EQ(literal, "hello");
    literal = std::string_view("view");
    EXPECT_EQ(literal, "view");
}

TEST(String_Tests, ACopyIsTheSameObject) {
    string a = "shared text";
    string b = a;
    string c;
    c = a;
    EXPECT_EQ(a.object(), b.object());
    EXPECT_EQ(a.data(), c.data());
    string d = "shared text";                   // made again: another object, equal contents
    EXPECT_NE(a.object(), d.object());
    EXPECT_EQ(a, d);
    EXPECT_EQ(a.substr(0), a);                  // the whole string: the same object
    EXPECT_EQ(a.substr(0).object(), a.object());
    EXPECT_EQ(a.substr(7), "text");
    EXPECT_EQ(a.substr(0, 6), "shared");
    EXPECT_THROW(a.substr(12), std::out_of_range);
    swap(a, c);
    EXPECT_EQ(a, c);
}

TEST(String_Tests, TheSearchesAndComparisonsOfStringView) {
    string s = "the quick brown fox";
    EXPECT_TRUE(s.starts_with("the") && s.starts_with('t') && !s.starts_with("fox"));
    EXPECT_TRUE(s.ends_with("fox") && s.ends_with('x') && !s.ends_with("the"));
    EXPECT_TRUE(s.contains("quick") && s.contains('q') && !s.contains("slow"));
    EXPECT_EQ(s.find("quick"), 4u);
    EXPECT_EQ(s.find('o'), 12u);
    EXPECT_EQ(s.find('o', 13), 17u);
    EXPECT_EQ(s.rfind('o'), 17u);
    EXPECT_EQ(s.find_first_of("xyz"), 18u);
    EXPECT_EQ(s.find_last_of("qt"), 4u);
    EXPECT_EQ(s.find_first_not_of("the "), 4u);
    EXPECT_EQ(s.find_last_not_of("xof "), 14u);
    EXPECT_EQ(s.find("missing"), string::npos);
    EXPECT_EQ(s.compare("the quick brown fox"), 0);
    EXPECT_LT(s.compare("the slow"), 0);
    EXPECT_EQ(s.compare(4, 5, "quick"), 0);
    char buffer[6] = {};
    EXPECT_EQ(s.copy(buffer, 5, 4), 5u);
    EXPECT_STREQ(buffer, "quick");
    string a = "apple", b = "banana", a2 = "apple";
    EXPECT_TRUE(a < b && b > a && a <= a2 && a >= a2 && a == a2 && a != b);
    EXPECT_TRUE((a <=> b) == std::strong_ordering::less);
    EXPECT_TRUE(a < std::string_view("apricot") && a == "apple" && "apple" == a && std::string_view("apple") == a);
    EXPECT_TRUE(a < "b" && "a" < a);
    std::ostringstream out;
    out << a << ' ' << b;
    EXPECT_EQ(out.str(), "apple banana");
}

TEST(String_Tests, ConcatenationMakesANewString) {
    string a = "hello", b = "world";
    string c = a + ", " + b + '!';
    EXPECT_EQ(c, "hello, world!");
    EXPECT_EQ("[" + a + "]", "[hello]");
    EXPECT_EQ('<' + a, "<hello");
    EXPECT_EQ(a + std::string_view(" there"), "hello there");
    EXPECT_EQ(std::string_view("oh ") + a, "oh hello");
    EXPECT_EQ(a + b, "helloworld");
    EXPECT_EQ(a, "hello");                       // untouched
}

TEST(String_Tests, TheHashIsComputedOnceAndKept) {
    string a = "some text to hash";
    string b = "some text to hash";
    string c = "some text to hasH";
    std::hash<string> h;
    EXPECT_EQ(h(a), h(b));
    EXPECT_NE(h(a), h(c));
    EXPECT_EQ(h(a), h(a));                       // the second time from the object
    EXPECT_EQ(h(string()), h(string()));
    EXPECT_NE(h(string()), h(a));
    EXPECT_TRUE(a == b);                         // equal after both hashes are known
    EXPECT_FALSE(a == c);                        // unequal by the hash, without the characters
    string d = "different length";
    EXPECT_FALSE(a == d);
    unordered_map<string, int> counts;
    ++counts[a];
    ++counts[b];
    ++counts[c];
    EXPECT_EQ(counts.size(), 2u);
    EXPECT_EQ(counts[string("some text to hash")], 2);
    std::unordered_map<gc::string, int> std_counts;   // the gc kind in a std container
    ++std_counts[gc::string(a)];
    ++std_counts[gc::string(b)];
    EXPECT_EQ(std_counts.size(), 1u);
    map<string, int> ordered;
    ordered[b] = 1;
    ordered[c] = 2;
    ordered[string("a")] = 0;
    EXPECT_EQ(ordered.begin()->first, "a");
    std::set<gc::string> sorted = {gc::string("pear"), gc::string("apple"), gc::string("fig")};
    EXPECT_EQ(*sorted.begin(), "apple");
}

TEST(String_Tests, EverySizeClassAndPastThem) {
    // one object of exactly the class: 8 bytes of header, the characters,
    // a terminator, rounded up to 4 up to 256 bytes; by half again to a
    // page; a buffer past it
    settle();
    const auto before = live_string_objects();
    vector<string> kept;
    for (size_t len : {0, 1, 7, 8, 15, 16, 23, 100, 247, 248, 255, 256, 300, 1000, 5000, 40000, 70000, 200000}) {
        std::string text(len, 'a' + (char)(len % 26));
        string s(text);
        EXPECT_EQ(s.size(), len);
        EXPECT_EQ(s, text);
        EXPECT_EQ(s.data()[len], '\0');
        kept.push_back(s);
    }
    settle();
    EXPECT_EQ(live_string_objects(), before + 17);   // the empty one has no object
    size_t total = 0;
    for (auto& s : collector::get_type_statistics()) {
        if (std::string(s.type->name()).find("StringSlot") != std::string::npos) {
            EXPECT_EQ(s.object_size % 4, 0u);
            total += s.live_bytes;
        }
    }
    EXPECT_GT(total, 0u);
    kept.clear();
    settle();
    EXPECT_EQ(live_string_objects(), before);         // gone with the last word, no destructor to run
}

TEST(String_Tests, TheOtherKindsAndTheOtherCharacters) {
    string a = "text";
    gc::string g = a;                               // the same object, the word of the other kind
    EXPECT_EQ(g.object(), a.object());
    EXPECT_EQ(g, a);
    EXPECT_EQ(a, g);
    string back = g;
    EXPECT_EQ(back.object(), a.object());
    gc::string assigned;
    assigned = a;
    EXPECT_EQ(assigned, "text");
    auto* heap = new std::vector<gc::string>();     // gc::string lives anywhere
    heap->push_back(a);
    heap->push_back(gc::string("more"));
    settle();
    EXPECT_EQ((*heap)[0], "text");
    EXPECT_EQ((*heap)[1], "more");
    delete heap;
    detail::cell_allocator.release();
    wstring w = L"wide";
    EXPECT_EQ(w.size(), 4u);
    EXPECT_EQ(w, L"wide");
    EXPECT_EQ(w[4], L'\0');
    u16string u = u"sixteen";
    EXPECT_EQ(u.size(), 7u);
    EXPECT_EQ(u.substr(0, 3), u"six");
    u32string u32 = U"thirty-two";
    EXPECT_EQ(u32.find(U"two"), 7u);
    u8string u8 = u8"eight";
    EXPECT_EQ(u8.size(), 5u);
    static_assert(std::is_same_v<gc::string, sgcl::basic_string<char, std::char_traits<char>, gc::tracked_ptr>>);
}

namespace {
    struct Node {
        string name;
        tracked_ptr<Node> next;
    };
}

TEST(String_Tests, StringsInManagedObjectsAreSharedAndCollected) {
    settle();
    const auto before = live_string_objects();
    tracked_ptr<Node> list;
    off_frame([&] {
        string shared = "the one name";
        for (int i = 0; i < 1000; ++i) {
            tracked_ptr node = make_tracked<Node>();
            node->name = i % 2 ? shared : string("own " + std::to_string(i));
            node->next = list;
            list = node;
        }
    });
    settle();
    EXPECT_EQ(live_string_objects(), before + 501);   // one shared, five hundred own
    int shared_count = 0;
    off_frame([&] {
        for (auto n = list; n; n = n->next) {
            if (n->name == "the one name") {
                ++shared_count;
            }
        }
    });
    EXPECT_EQ(shared_count, 500);
    list = nullptr;
    settle();
    EXPECT_EQ(live_string_objects(), before);
}
