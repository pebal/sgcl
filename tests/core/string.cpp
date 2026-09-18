//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// string: an immutable string on the managed heap, one word, shared by
// copying, compared and hashed by its contents, no destructor.
#include "tests/types.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
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

TEST(String_Tests, AMapKeyedByStringsIsSearchedWithAViewAndNoStringIsMade) {
    static_assert(detail::TransparentLookup<std::hash<string>, std::equal_to<string>>);
    static_assert(detail::TransparentCompare<std::less<string>>);
    EXPECT_EQ(std::hash<string>()(std::string_view("alice")), string("alice").hash());   // the hash of a view is the string's
    EXPECT_EQ(std::hash<string>()("alice"), string("alice").hash());
    EXPECT_EQ(std::hash<string>()(std::string_view()), string().hash());
    EXPECT_TRUE(std::equal_to<string>()(string("a"), std::string_view("a")) && std::less<string>()(std::string_view("a"), string("b")));
    unordered_map<string, int> ages = {{"alice", 30}, {"bob", 40}};
    map<string, int> sorted = {{"alice", 30}, {"bob", 40}};
    set<string> names = {"alice", "bob"};
    unordered_set<string> hashed = {"alice", "bob"};
    std::string_view view = "alice";
    EXPECT_EQ(ages.find(view)->second, 30);       // a string_view converts to no string: this is the transparent lookup
    EXPECT_EQ(ages.at(view), 30);
    EXPECT_EQ(ages.count(view), 1u);
    EXPECT_TRUE(ages.contains(view) && !ages.contains(std::string_view("carol")));
    EXPECT_EQ(sorted.find(view)->second, 30);
    EXPECT_EQ(sorted.at(view), 30);
    EXPECT_EQ(sorted.lower_bound(std::string_view("b"))->first, "bob");
    EXPECT_EQ(sorted.upper_bound(view)->first, "bob");
    EXPECT_EQ(sorted.equal_range(view).first->second, 30);
    EXPECT_TRUE(names.contains(view) && hashed.contains(view) && names.find(view) != names.end());
    EXPECT_THROW(ages.at(std::string_view("carol")), std::out_of_range);
    EXPECT_THROW(sorted.at(std::string_view("carol")), std::out_of_range);
    EXPECT_EQ(ages.erase(std::string_view("bob")), 1u);
    EXPECT_EQ(sorted.erase(std::string_view("bob")), 1u);
    EXPECT_EQ(ages.size(), 1u);
    EXPECT_EQ(sorted.size(), 1u);
    settle();
    auto before = collector::get_statistics().live_bytes;   // managed memory in use: a temporary string would add to it
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += ages.find("alice")->second + sorted.at("alice") + (int)names.count("bob") + (int)hashed.contains("alice");
    }
    EXPECT_EQ(sum, 10000 * 62);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for any lookup (a sweep meanwhile can only lower the count)
}

TEST(String_Tests, AStringViewHoldsTheObject) {
    static_assert(sizeof(string_view) == 2 * sizeof(void*));
    string s = "hello, world";
    string_view whole = s.view();
    string_view world = s.view(7, 5);
    EXPECT_EQ(whole, "hello, world");
    EXPECT_EQ(world, "world");
    EXPECT_EQ(world.size(), 5u);
    EXPECT_EQ(whole.object(), s.object());                     // the same object, held
    EXPECT_EQ(world.object(), s.object());
    EXPECT_EQ(world.substr(1, 3), "orl");
    EXPECT_EQ(world.substr(1, 3).object(), s.object());         // a view of a view: still that object
    EXPECT_THROW(s.view(13), std::out_of_range);
    EXPECT_THROW(world.substr(6), std::out_of_range);
    EXPECT_TRUE(world.starts_with("wo") && world.ends_with('d') && world.contains("rl") && world.find('r') == 2 && world.rfind('l') == 3);
    EXPECT_EQ(world.compare("world"), 0);
    EXPECT_EQ(world[0], 'w');
    EXPECT_EQ(world.at(4), 'd');
    EXPECT_THROW(world.at(5), std::out_of_range);
    EXPECT_EQ(world.front(), 'w');
    EXPECT_EQ(world.back(), 'd');
    EXPECT_EQ(std::string(world.begin(), world.end()), "world");
    std::string_view std_view = world;                          // converts to the std view
    EXPECT_EQ(std_view, "world");
    EXPECT_EQ(string(whole).object(), s.object());              // a string of the whole view: the same object
    string copy(world);                                         // of a piece: a new string
    EXPECT_EQ(copy, "world");
    EXPECT_NE(copy.object(), s.object());
    EXPECT_EQ(world.str(), "world");
    string_view padded = string("  x  ").view();
    EXPECT_EQ(padded.trim(), "x");
    EXPECT_EQ(padded.trim_left(), "x  ");
    EXPECT_EQ(padded.trim_right(), "  x");
    EXPECT_EQ(padded.trim().object(), padded.object());         // trimming makes views, not strings
    EXPECT_EQ(whole.trim_prefix("hello"), ", world");
    EXPECT_EQ(whole.trim_suffix("world"), "hello, ");
    EXPECT_EQ(whole.trim_prefix("x"), whole);
    string_view narrowed = world;
    narrowed.remove_prefix(1);
    narrowed.remove_suffix(1);
    EXPECT_EQ(narrowed, "orl");
    string_view empty;
    EXPECT_TRUE(empty.empty() && empty == "" && empty.object() == nullptr && string(empty).empty());
    EXPECT_TRUE(whole == s && s == whole && whole != world && whole < string("z") && (whole <=> "hello, world") == 0 && "hello, world" == whole);
    EXPECT_EQ(std::hash<string_view>()(world), string("world").hash());   // the hash a string of the characters has
    EXPECT_EQ(std::hash<string_view>()(whole), s.hash());
    std::ostringstream out;
    out << whole << '|' << world;
    EXPECT_EQ(out.str(), "hello, world|world");
    unordered_map<string, int> ages = {{"world", 1}};           // a view finds a string key, transparently
    EXPECT_EQ(ages.find(world)->second, 1);
    unordered_set<string_view> views;                           // and keys a container of views
    views.insert(world);
    EXPECT_TRUE(views.contains(world) && views.contains(std::string_view("world")) && views.contains("world"));
    settle();
    auto base = live_string_objects();
    string_view kept;
    off_frame([&] {
        string temporary = "a,b";
        kept = *temporary.split(',').begin();                   // the piece holds the object
    });
    settle();
    EXPECT_EQ(kept, "a");                                       // alive: the view is a root
    EXPECT_EQ(live_string_objects(), base + 1);
    off_frame([&] {
        kept = string_view();
    });
    settle();
    EXPECT_EQ(live_string_objects(), base);                     // and gone once no view holds it
}

TEST(String_Tests, AViewIsTheOnlyHolderOfAStringOfAnySize) {
    // A string of every size class and past them (a slot, a buffer of a
    // page, a buffer of many pages) held by nothing but a view of a piece
    // in its middle, across full collections: the view's word is the
    // object's own address, so the collector sees the object whole
    settle();
    auto base = live_string_objects();
    sgcl::vector<string_view> views;
    sgcl::vector<size_t> sizes = {5, 40, 300, 5000, 70000, 300000};
    off_frame([&] {
        for (size_t n : sizes) {
            std::string text(n, 'x');
            text[n / 2] = 'M';
            string s(text);
            views.push_back(s.view(n / 2 - 1, 3));              // "xMx", the string held by the view alone
        }
    });
    settle();
    EXPECT_EQ(live_string_objects(), base + sizes.size());
    for (auto& v : views) {
        EXPECT_EQ(v, "xMx");
        EXPECT_EQ(v.size(), 3u);
    }
    string whole(views.back().substr(0, 0));                    // nothing of it: empty, no allocation
    EXPECT_TRUE(whole.empty());
    off_frame([&] {
        views.clear();
    });
    settle();
    EXPECT_EQ(live_string_objects(), base);
}

TEST(String_Tests, SplitAndFieldsAreARangeOfViewsJoinTakesAnyRange) {
    string csv = "a,b,,c";
    static_assert(std::ranges::forward_range<string::pieces>);
    static_assert(std::is_same_v<std::ranges::range_value_t<string::pieces>, string_view>);   // views that hold the string
    auto collect = [](auto&& pieces) {             // the pieces as they come, marked
        std::string s;
        for (std::string_view piece : pieces) {
            s += '[';
            s += piece;
            s += ']';
        }
        return s;
    };
    EXPECT_EQ(collect(csv.split(',')), "[a][b][][c]");
    EXPECT_EQ(collect(csv.split(",")), "[a][b][][c]");
    EXPECT_EQ(collect(csv.split(std::string_view(","))), "[a][b][][c]");
    EXPECT_EQ(collect(csv.split(string(","))), "[a][b][][c]");
    EXPECT_EQ(collect(csv.split(",", 2)), "[a][b,,c]");          // the last piece holds the rest
    EXPECT_EQ(collect(csv.split(',', 1)), "[a,b,,c]");
    EXPECT_EQ(collect(string("a::b::c").split("::")), "[a][b][c]");
    EXPECT_EQ(collect(string("a,").split(',')), "[a][]");        // a separator at the end: an empty piece
    EXPECT_EQ(collect(string(",a").split(',')), "[][a]");
    EXPECT_EQ(collect(csv.split(';')), "[a,b,,c]");              // no separator: the whole string
    EXPECT_EQ(collect(string("abc").split("")), "[a][b][c]");    // an empty separator: every character
    EXPECT_EQ(collect(string("abc").split("", 2)), "[a][bc]");
    EXPECT_TRUE(string().split(',').empty());                    // an empty string: no piece
    EXPECT_EQ(collect(string("  the quick\tbrown\n fox ").fields()), "[the][quick][brown][fox]");
    EXPECT_TRUE(string("   ").fields().empty());
    auto pieces = csv.split(',');                                // a value: the string held, the pieces views into it
    EXPECT_EQ(pieces.text().object(), csv.object());
    EXPECT_EQ(pieces.begin()->object(), csv.object());           // each piece holds the string's object
    EXPECT_EQ(std::ranges::distance(pieces), 4);
    auto it = pieces.begin();
    EXPECT_EQ(*it, "a");
    EXPECT_EQ(*++it, "b");
    EXPECT_EQ(it->size(), 1u);
    auto before = collector::get_statistics().live_bytes;       // walking the pieces allocates nothing
    size_t total = 0;
    for (int i = 0; i < 1000; ++i) {
        for (std::string_view piece : csv.split(',')) {
            total += piece.size();
        }
    }
    EXPECT_EQ(total, 3000u);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // nothing allocated (a sweep meanwhile can only lower the count)
    for (string_view piece : csv.split(',')) {                   // a string of a piece: explicit, as std's from a view
        EXPECT_LE(string(piece).size(), 1u);
    }
    vector<string> strings(csv.split(','));                      // or a container of them: each constructed from its view
    EXPECT_EQ(strings.size(), 4u);
    EXPECT_EQ(strings[3], "c");
    EXPECT_EQ(string::join(csv.split(','), "-"), "a-b--c");     // join takes the range as it is
    EXPECT_EQ(string::join(csv.split(','), '+'), "a+b++c");
    EXPECT_EQ(string::join(csv.fields(), std::string_view(", ")), "a,b,,c");
    std::vector<std::string_view> views = {"x", "y", "z"};       // any range of what a view is made of
    EXPECT_EQ(string::join(views, ""), "xyz");
    EXPECT_EQ(string::join(std::vector<string>{}, ","), "");
    EXPECT_EQ(string::join(std::vector<const char*>{"only"}, ","), "only");
    wstring w = L"a b";
    EXPECT_EQ(wstring::join(w.split(L' '), L"-"), L"a-b");
}

TEST(String_Tests, TrimReplaceRepeatAndCase) {
    string padded = " \t hello \n";
    EXPECT_EQ(padded.trim(), "hello");
    EXPECT_EQ(padded.trim_left(), "hello \n");
    EXPECT_EQ(padded.trim_right(), " \t hello");
    EXPECT_EQ(string("xxhixx").trim("x"), "hi");
    EXPECT_EQ(string("xxhixx").trim_left("x"), "hixx");
    EXPECT_EQ(string("xxhixx").trim_right("x"), "xxhi");
    EXPECT_TRUE(string("   ").trim().empty() && string("   ").trim_left().empty() && string("   ").trim_right().empty());
    string plain = "hello";
    EXPECT_EQ(plain.trim().object(), plain.object());          // nothing to trim: the same object
    EXPECT_EQ(plain.trim_left().object(), plain.object());
    EXPECT_EQ(plain.trim_right().object(), plain.object());
    EXPECT_EQ(plain.trim_prefix("he"), "llo");
    EXPECT_EQ(plain.trim_suffix("lo"), "hel");
    EXPECT_EQ(plain.trim_prefix("lo").object(), plain.object());   // not a prefix: the same object
    EXPECT_EQ(plain.trim_suffix("he").object(), plain.object());
    string text = "one two two three";
    EXPECT_EQ(text.replace("two", "2"), "one 2 2 three");
    EXPECT_EQ(text.replace("two", "2", 1), "one 2 two three");
    EXPECT_EQ(text.replace(' ', '_'), "one_two_two_three");
    EXPECT_EQ(text.replace("two", "twotwo"), "one twotwo twotwo three");   // no overlap with what was put in
    EXPECT_EQ(text.replace("aa", "b").object(), text.object());   // no occurrence: the same object
    EXPECT_EQ(text.replace("", "b").object(), text.object());     // an empty `from`: the same object
    EXPECT_EQ(string("aaa").replace("a", ""), "");
    EXPECT_EQ(string("ab").repeat(3), "ababab");
    EXPECT_TRUE(string("ab").repeat(0).empty());
    EXPECT_EQ(plain.repeat(1).object(), plain.object());
    string mixed = "Hello, World 42!";
    EXPECT_EQ(mixed.to_lower(), "hello, world 42!");
    EXPECT_EQ(mixed.to_upper(), "HELLO, WORLD 42!");
    string lower = "already lower";
    EXPECT_EQ(lower.to_lower().object(), lower.object());        // no letter changes: the same object
    string digits = "42!";
    EXPECT_EQ(digits.to_upper().object(), digits.object());      // no letter at all: the same object
    u8string u8 = u8"MiXed";                                     // the other character types
    EXPECT_TRUE(u8.to_lower() == u8"mixed" && u8.to_upper() == u8"MIXED");
    wstring w = L" a b ";
    EXPECT_EQ(wstring::join(w.trim().split(L' '), L"-"), L"a-b");
    EXPECT_EQ(text, "one two two three");                        // every operation left its string as it was
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
    map<string, int> ordered;
    ordered[b] = 1;
    ordered[c] = 2;
    ordered[string("a")] = 0;
    EXPECT_EQ(ordered.begin()->first, "a");
    set<string> sorted = {string("pear"), string("apple"), string("fig")};
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

TEST(String_Tests, TheOtherCharacters) {
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
