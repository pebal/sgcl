//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the calls of txt leave on the managed heap (the audit of 2026-09-26):
// the scratch of a call — a mapped text, the elements of a weighed one, the
// arrays of the bidirectional algorithm, the code points of a normalization
// — is plain memory, and a call takes from the managed heap its answer and
// nothing else. Each test failed on the headers before the change: the
// numbers in the comments are the managed bytes a call took then and takes
// now (benchmarks/heap).
#include "tests/managed_pages.h"

#include <string>

namespace {
    string of(std::string_view s) {
        return string(s.data(), s.size());
    }

    // Ten kilobytes of English with one fox near the end
    string ten_kilobytes() {
        std::string text;
        while (text.size() < 10000) {
            text += "the quick brown dog jumps over the lazy cat ";
        }
        text += "the quick brown fox jumps over the lazy dog";
        return of(text);
    }

    // A kilobyte of left to right and right to left text, digits and brackets
    string mixed_kilobyte() {
        std::string text;
        while (text.size() < 1024) {
            text += "abc \xD7\x90\xD7\x91\xD7\x92 123 (x) ";
        }
        return of(text);
    }
}

// A search blind to case maps the text for the call alone: 262 KB of
// managed buffers for one search through ten kilobytes, now none
TEST(TxtHeap_Tests, AFoldSearchTakesNoManagedMemory) {
    string text = ten_kilobytes();
    txt::fold_searcher fox("FOX");
    ASSERT_TRUE(fox.find(text).has_value());
    size_t pages = heap_count::pages_of(40, [&] {
        EXPECT_TRUE(fox.find(text).has_value());
    });
    EXPECT_LE(pages, 1u);
    pages = heap_count::pages_of(40, [&] {
        EXPECT_TRUE(txt::find_normalized(text, of("fox")).has_value());
    });
    EXPECT_LE(pages, 1u);
    pages = heap_count::pages_of(40, [&] {
        EXPECT_EQ(fox.count(text), 1u);
    });
    EXPECT_LE(pages, 1u);
}

// And a prepared text keeps what it mapped, which is its data: a
// folded_text still answers after the thread's scratch has been lent
// to another search
TEST(TxtHeap_Tests, APreparedTextKeepsItsOwnMapping) {
    string text = ten_kilobytes();
    txt::folded_text prepared(text);
    txt::fold_searcher fox("FOX");
    auto found = prepared.find(fox);
    ASSERT_TRUE(found.has_value());
    size_t at = found->pos;
    EXPECT_TRUE(txt::find_fold(of("A DIFFERENT TEXT, LONGER THAN THE PATTERN"), of("text")).has_value());
    EXPECT_EQ(prepared.find(fox), found);
    EXPECT_EQ(text.view().substr(at, 3), "fox");
}

// collator::find weighs the text for the call alone: 327 KB for ten
// kilobytes, now none
TEST(TxtHeap_Tests, ACollatorFindTakesNoManagedMemory) {
    string text = ten_kilobytes();
    txt::collator c;
    string fox = of("fox");
    auto hit = c.find(text, fox);
    ASSERT_TRUE(hit.has_value());
    size_t pages = heap_count::pages_of(40, [&] {
        EXPECT_EQ(c.find(text, fox)->at, hit->at);
        EXPECT_TRUE(c.contains(text, fox));
    });
    EXPECT_LE(pages, 1u);
    // the prepared text answers the same, from elements of its own
    txt::collated_text prepared(c, text);
    EXPECT_EQ(prepared.find(fox)->at, hit->at);
    EXPECT_TRUE(c.ends_with(text, of("lazy dog")));
    EXPECT_TRUE(prepared.ends_with(of("lazy dog")));
    EXPECT_FALSE(c.starts_with(text, of("lazy dog")));
}

// levels() and visual_order() take their answer and nothing else: a
// kilobyte took 47 managed objects and 85 KB, now the one vector
TEST(TxtHeap_Tests, TheBidiAnswersAreTheOnlyManagedObjects) {
    string text = mixed_kilobyte();
    size_t n = txt::levels(text).size();
    ASSERT_GT(n, 500u);
    size_t answer = heap_count::pages_of(100, [&] {
        vector<uint8_t> v(n);
        EXPECT_EQ(v.size(), n);
    });
    size_t pages = heap_count::pages_of(100, [&] {
        EXPECT_EQ(txt::levels(text).size(), n);
    });
    EXPECT_LE(pages, answer + 1);
    size_t order = txt::visual_order(text).size();
    answer = heap_count::pages_of(100, [&] {
        vector<size_t> v(order);
        EXPECT_EQ(v.size(), order);
    });
    pages = heap_count::pages_of(100, [&] {
        EXPECT_EQ(txt::visual_order(text).size(), order);
    });
    EXPECT_LE(pages, answer + 1);
}

// The code points of a normalization, a case mapping, a fold and IDNA
// are the call's: what is left is the string of the answer. A kilobyte
// with combining marks normalized took 6.8 KB, now its 0.85 KB answer;
// to_lower_full took two objects, idna::ascii_form seven, now one.
TEST(TxtHeap_Tests, TheTextMappingsTakeOnlyTheirAnswer) {
    std::string marks;
    while (marks.size() < 1024) {
        marks += "e\xCC\x81";
    }
    string combining = of(marks);
    string composed = txt::normalize(combining, txt::nfc);
    size_t answer = heap_count::pages_of(1000, [&] {
        EXPECT_EQ(string(composed.data(), composed.size()).size(), composed.size());
    });
    size_t pages = heap_count::pages_of(1000, [&] {
        EXPECT_EQ(txt::normalize(combining, txt::nfc).size(), composed.size());
    });
    EXPECT_LE(pages, answer + 1);
    pages = heap_count::pages_of(1000, [&] {
        EXPECT_EQ(txt::without_marks(combining).size(), marks.size() / 3);
    });
    EXPECT_LE(pages, answer + 1);

    string mixed = of("The Quick Brown Fox Jumps Over The Lazy Dog And Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 G\xC4\x99\xC5\x9Bl\xC4\x85 Ja\xC5\xBA\xC5\x84");
    string lower = txt::to_lower_full(mixed);
    answer = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(string(lower.data(), lower.size()).size(), lower.size());
    });
    pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(txt::to_lower_full(mixed).size(), lower.size());
    });
    EXPECT_LE(pages, answer + 1);
    pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(txt::nfkc_casefold(mixed).size(), lower.size());
    });
    EXPECT_LE(pages, answer + 1);

    string host = of("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87-g\xC4\x99\xC5\x9Bl\xC4\x85.example.com");
    string ascii = txt::idna::ascii_form(host).text;
    ASSERT_EQ(ascii.view().substr(0, 4), "xn--");
    answer = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(string(ascii.data(), ascii.size()).size(), ascii.size());
    });
    pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(txt::idna::ascii_form(host).text, ascii);
    });
    EXPECT_LE(pages, answer + 1);
    EXPECT_EQ(txt::idna::unicode_form(ascii).text, host);
}

// Two scratch buffers asked for at once are two, not one lent twice: the
// comparisons normalize both sides into buffers of the thread's, and a
// call inside a call gets its own when the thread's are all out
TEST(TxtHeap_Tests, ScratchInsideScratchIsItsOwn) {
    string a = of("Cafe\xCC\x81 au lait");
    string b = of("Caf\xC3\xA9 au lait");
    EXPECT_TRUE(txt::equal_normalized(a, b));
    EXPECT_EQ(txt::compare_normalized(a, b), 0);
    EXPECT_EQ(txt::hash_normalized(a), txt::hash_normalized(b));
    EXPECT_LT(txt::compare_normalized(of("abc"), of("abd")), 0);
    // more at once than a thread keeps: every one of them its own
    std::vector<std::unique_ptr<txt::detail::lent<txt::detail::code_points>>> held;
    for (size_t i = 0; i < txt::detail::LentSlots + 3; ++i) {
        held.push_back(std::make_unique<txt::detail::lent<txt::detail::code_points>>());
        (*held.back())->push_back(char32_t(i));
    }
    for (size_t i = 0; i < held.size(); ++i) {
        ASSERT_EQ((*held[i])->size(), 1u);
        EXPECT_EQ((**held[i])[0], char32_t(i));
    }
    EXPECT_EQ(txt::normalize(a, txt::nfc), b);   // with every slot of the thread lent out
    held.clear();
    // and what goes back comes back empty
    txt::detail::lent<txt::detail::code_points> again;
    EXPECT_TRUE(again->empty());
}

// A match with groups keeps their positions in itself, up to four
// groups, and in plain memory past that: every match found made a
// managed buffer of them, now none
TEST(TxtHeap_Tests, AMatchKeepsItsGroupsOffTheManagedHeap) {
    auto re4 = *txt::regex::compile(of("(fox)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)"));
    auto re6 = *txt::regex::compile(of("(f)(o)(x)\\s+(\\w+)\\s+(\\w+)\\s+(\\w+)"));
    string text = of("the quick brown fox jumps over the lazy dog");
    heap_count::settle();
    size_t before = heap_count::live_buffers_of<size_t>();
    vector<txt::match> kept;
    kept.reserve(20);
    for (int i = 0; i < 10; ++i) {
        kept.push_back(*re4.find(text));
        kept.push_back(*re6.find(text));
    }
    EXPECT_EQ(heap_count::live_buffers_of<size_t>(), before);
    for (size_t i = 0; i < kept.size(); i += 2) {
        auto& four = kept[i];
        auto& six = kept[i + 1];
        ASSERT_EQ(four.group_count(), 4u);
        EXPECT_EQ(four[1].view(), "fox");
        EXPECT_EQ(four[4].view(), "the");
        ASSERT_EQ(six.group_count(), 6u);
        EXPECT_EQ(six[3].view(), "x");
        EXPECT_EQ(six[6].view(), "the");
        // a copy keeps them, the spilled ones included
        txt::match copy = six;
        EXPECT_EQ(copy[5].view(), "over");
        EXPECT_FALSE(copy.group(7).has_value());
    }
    size_t pages = heap_count::pages_of(20000, [&] {
        EXPECT_EQ(re4.find(text)->group_count(), 4u);
    });
    EXPECT_LE(pages, 1u);
}

// The program of a template is written once at its size, from scratch
// the parse builds it in: a small template made thirteen objects, seven
// of them the vectors' growth, now eight
TEST(TxtHeap_Tests, AStencilProgramIsWrittenOnce) {
    string source = of("Hello {{.name}}, you have {{.count}} items: {{range .items}}{{.}} "
                       "{{if .}}!{{else if $.count}}?{{else}}.{{end}}{{end}}");
    auto parsed = txt::stencil::parse(source);
    ASSERT_TRUE(parsed.has_value());
    txt::value data = txt::object{{"name", "World"}, {"count", 3}, {"items", txt::list{"a", "", "c"}}};
    EXPECT_EQ(std::string(parsed->render(data).view()), "Hello World, you have 3 items: a ! ?c !");
    // the copy of a template is its program at its size and no more;
    // the parse makes that and the source's expected, and nothing else
    auto copies = heap_count::pages_of(5000, [&] {
        auto copy = *parsed;
        EXPECT_EQ(copy.steps(), parsed->steps());
    });
    auto parses = heap_count::pages_of(5000, [&] {
        auto p = txt::stencil::parse(source);
        ASSERT_TRUE(p);
        EXPECT_EQ(p->steps(), parsed->steps());
    });
    EXPECT_LE(parses, copies + copies / 2 + 1);
    // the jumps of an else chain inside a loop inside a branch, each
    // block fixing its own
    auto nested = txt::stencil::parse(of("{{if .a}}{{range .xs}}{{if .}}1{{else if $.a}}2{{else}}3{{end}}{{end}}{{else}}4{{end}}"));
    ASSERT_TRUE(nested.has_value());
    txt::value d1 = txt::object{{"a", true}, {"xs", txt::list{"y", "", "z"}}};
    txt::value d2 = txt::object{{"a", false}, {"xs", txt::list{"y"}}};
    EXPECT_EQ(std::string(nested->render(d1).view()), "121");
    EXPECT_EQ(std::string(nested->render(d2).view()), "4");
}

// A slot the thread keeps holds at most LentKeep: one grown past it is let
// go when the call gives it back, one within it is kept for the next call,
// and what the thread keeps over all its slots stays within LentThreadKeep
TEST(TxtHeap_Tests, ALargeScratchIsLetGoAfterUse) {
    using txt::detail::code_points;
    using txt::detail::lent;
    const size_t small = txt::detail::LentKeep / sizeof(char32_t) / 2;
    const size_t large = txt::detail::LentKeep / sizeof(char32_t) + 1;
    {
        lent<code_points> s;
        s->resize(small);
    }
    {
        lent<code_points> s;   // the same slot, the first free one
        EXPECT_TRUE(s->empty());
        EXPECT_GE(s->capacity(), small);
        s->resize(large);
    }
    {
        lent<code_points> s;
        EXPECT_TRUE(s->empty());
        EXPECT_EQ(s->capacity(), 0u);
    }
    // eight slots of a quarter megabyte each are the whole of a thread's
    // two megabytes, of every kind together
    {
        std::vector<std::unique_ptr<lent<code_points>>> held;
        for (size_t i = 0; i < txt::detail::LentSlots; ++i) {
            held.push_back(std::make_unique<lent<code_points>>());
            (*held.back())->resize(txt::detail::LentKeep / sizeof(char32_t));
        }
    }
    EXPECT_LE(txt::detail::lent_kept_bytes(), txt::detail::LentThreadKeep);
    {
        lent<txt::detail::scratch_vector<size_t>> other;
        other->resize(1000);
    }
    EXPECT_LE(txt::detail::lent_kept_bytes(), txt::detail::LentThreadKeep);
}

// The growth of a scratch array cannot throw: a failed realloc ends the
// program, as running out of managed memory does, so the loops that
// append a code point at a time cannot throw either. They threw bad_alloc.
TEST(TxtHeap_Tests, GrowingScratchCannotThrow) {
    using txt::detail::scratch_vector;
    txt::detail::code_points points;
    static_assert(noexcept(points.push_back(U'a')));
    static_assert(noexcept(points.reserve(1)));
    static_assert(noexcept(points.resize(1)));
    static_assert(noexcept(points.assign(size_t(1), U'a')));
    static_assert(noexcept(points.append(points.begin(), points.end())));
    static_assert(noexcept(points.insert(points.begin(), U'a')));
    static_assert(noexcept(points.emplace_back(U'a')));
    static_assert(std::is_nothrow_copy_constructible_v<scratch_vector<size_t>>);
    static_assert(std::is_nothrow_copy_assignable_v<scratch_vector<size_t>>);
    static_assert(noexcept(txt::detail::case_one(points, std::string_view(), 0, 0, U'a',
                                                 txt::detail::casing::lower, txt::locale())));
    static_assert(noexcept(txt::detail::compose_buffer(points)));
    static_assert(noexcept(txt::detail::normalized_points(std::string_view(), txt::nfd, points)));
    static_assert(noexcept(txt::detail::decompose_into<false>(points, U'a')));
    txt::detail::code_points grown;
    for (char32_t c = 0; c < 100000; ++c) {
        grown.push_back(c);
    }
    EXPECT_EQ(grown.size(), 100000u);
    EXPECT_EQ(grown[99999], char32_t(99999));
}

// insert moves the elements after the place up by one, a run that overlaps
// its source by all but one element, and resize zeroes what it adds: both
// over every width the byte routines take apart (under four bytes, four,
// eight, sixteen, thirty-two and past it)
TEST(TxtHeap_Tests, ScratchInsertAndResize) {
    for (size_t n = 0; n <= 40; ++n) {
        for (size_t at = 0; at <= n; ++at) {
            txt::detail::scratch_vector<uint8_t> bytes;
            txt::detail::code_points points;
            for (size_t k = 0; k < n; ++k) {
                bytes.push_back(uint8_t(k + 1));
                points.push_back(char32_t(k + 1));
            }
            bytes.insert(bytes.begin() + at, uint8_t(0xEE));
            points.insert(points.begin() + at, U'\x10FFFF');
            ASSERT_EQ(bytes.size(), n + 1);
            ASSERT_EQ(points.size(), n + 1);
            for (size_t k = 0; k <= n; ++k) {
                size_t was = k < at ? k + 1 : k;
                ASSERT_EQ(bytes[k], k == at ? uint8_t(0xEE) : uint8_t(was)) << n << " " << at;
                ASSERT_EQ(points[k], k == at ? U'\x10FFFF' : char32_t(was)) << n << " " << at;
            }
        }
        txt::detail::code_points grown;
        grown.assign(size_t(3), U'\x10FFFF');
        grown.resize(3 + n);
        ASSERT_EQ(grown.size(), 3 + n);
        for (size_t k = 0; k < 3 + n; ++k) {
            ASSERT_EQ(grown[k], k < 3 ? U'\x10FFFF' : char32_t(0)) << n;
        }
    }
}
