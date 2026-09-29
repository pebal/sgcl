//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the parsers of encoding leave on the managed heap (the audit of
// 2026-09-26): numbers that nothing but their owner reads are plain memory,
// and the stacks a JSON parse gathers its values on, which have to be
// managed, are the thread's and not made again for every parse. The memory
// tests failed on the headers before the change: the numbers in the
// comments are what a call took then and takes now (benchmarks/heap).
#include "tests/managed_pages.h"
#include "sgcl/encoding/encoding.h"

#include <string>
#include <thread>

using sgcl::encoding::csv;
using sgcl::encoding::json;

namespace {
    string of(std::string_view s) {
        return string(s.data(), s.size());
    }

    // The strings long enough to have a buffer of their own
    size_t live_long_strings() {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (s.buffers && s.type && *s.type == typeid(sgcl::detail::StringByte[])) {
                n += s.live_objects;
            }
        }
        return n;
    }

    string numbers(size_t bytes) {
        std::string text = "[";
        for (int i = 0; text.size() < bytes; ++i) {
            if (i) {
                text += ",";
            }
            text += std::to_string(i * 7);
        }
        return of(text + "]");
    }

    string large_object(int members) {
        std::string text = "{";
        for (int i = 0; i < members; ++i) {
            if (i) {
                text += ",";
            }
            text += "\"key" + std::to_string(i) + "\":" + std::to_string(i);
        }
        return of(text + "}");
    }
}

// A row's places are numbers: plain memory its destructor frees, and the
// row one managed object, its text, where it was two
TEST(EncodingHeap_Tests, ARowIsItsTextAlone) {
    string text = of("a,b,c\n1,\"two, quoted\",3\nx,y,z\n");
    heap_count::settle();
    size_t before = heap_count::live_buffers_of<uint32_t>();
    vector<csv::row> rows;
    for (int i = 0; i < 10; ++i) {
        csv::reader r(text);
        while (auto row = r.next()) {
            rows.push_back(*row);
        }
    }
    ASSERT_EQ(rows.size(), 30u);
    EXPECT_EQ(heap_count::live_buffers_of<uint32_t>(), before);
    // and a copy is a row of its own, places and all
    csv::row copy = rows[4];
    auto place = rows[4].position(2);
    rows.clear();
    heap_count::settle();
    ASSERT_EQ(copy.size(), 3u);
    EXPECT_EQ(copy[1].view(), "two, quoted");
    EXPECT_EQ(copy.line(), 2u);
    EXPECT_EQ(copy.position(2), place);
    EXPECT_EQ(place.first, 2u);
}

// The index of an object past sixteen members is numbers too: one managed
// buffer fewer for every large object, and the lookups the same
TEST(EncodingHeap_Tests, ALargeObjectsIndexIsPlainMemory) {
    string text = large_object(40);
    heap_count::settle();
    size_t before = heap_count::live_buffers_of<uint32_t>();
    vector<json> kept;
    for (int i = 0; i < 10; ++i) {
        kept.push_back(*json::parse(text));
    }
    EXPECT_EQ(heap_count::live_buffers_of<uint32_t>(), before);
    for (auto& j : kept) {
        ASSERT_EQ(j.size(), 40u);
        for (int k = 0; k < 40; ++k) {
            EXPECT_EQ(j[of("key" + std::to_string(k))].as_int(), k);
        }
        EXPECT_TRUE(j[of("key40")].is_null());
    }
    // built by hand, and copied, it is the same
    json copy = kept[3];
    kept.clear();
    heap_count::settle();
    EXPECT_EQ(copy[of("key39")].as_int(), 39);
}

// The stacks of a parse are the thread's: ten kilobytes of numbers made
// 187 KB of managed memory for an answer of 37 (the stacks doubled afresh
// every parse), now the answer alone
TEST(EncodingHeap_Tests, AParseTakesItsAnswerAndNotItsStacks) {
    string text = numbers(10000);
    size_t n = json::parse(text)->size();
    ASSERT_GT(n, 1500u);
    size_t answer = heap_count::pages_of(50, [&] {
        vector<json> v(n);
        EXPECT_EQ(v.size(), n);
    });
    size_t pages = heap_count::pages_of(50, [&] {
        EXPECT_EQ(value_of(json::parse(text)).size(), n);
    });
    EXPECT_LE(pages, answer + 1);
}

// What the stacks held is let go when the parse ends, the failed ones
// included: a kept stack keeps nothing alive
TEST(EncodingHeap_Tests, AFailedParseLeavesNothingOnTheStacks) {
    std::string long_a(300, 'a');
    std::string long_b(300, 'b');
    string text = of("[\"" + long_a + "\", {\"k\": \"" + long_b + "\", \"n\": [1, 2, \"" + long_a + "\"");
    heap_count::settle();
    size_t before = live_long_strings();
    off_frame([&] {
        for (int i = 0; i < 3; ++i) {
            auto r = json::parse(text);
            ASSERT_FALSE(r.has_value());
            EXPECT_EQ(r.error().code(), sgcl::encoding::errc::unexpected_end);
        }
    });
    heap_count::settle();
    EXPECT_EQ(live_long_strings(), before);
    // and the next parse on the thread starts from empty stacks
    auto good = json::parse(of("[[1, 2], {\"a\": [3]}, 4]"));
    ASSERT_TRUE(good.has_value());
    EXPECT_EQ(good->size(), 3u);
    EXPECT_EQ((*good)[1][of("a")][0].as_int(), 3);
}

// A parse while the thread's stacks are lent out (a parse inside a parse)
// has stacks of its own, and each thread has its own
TEST(EncodingHeap_Tests, AParseWhileTheStacksAreOutHasItsOwn) {
    {
        sgcl::encoding::detail::JsonScratchLease outer;
        (*outer).values.push_back(json(int64_t(7)));
        auto inner = json::parse(of("[1, [2, 3], {\"x\": 4}]"));
        ASSERT_TRUE(inner.has_value());
        EXPECT_EQ((*inner)[1][1].as_int(), 3);
        EXPECT_EQ((*inner)[2][of("x")].as_int(), 4);
        ASSERT_EQ((*outer).values.size(), 1u);
        EXPECT_EQ((*outer).values[0].as_int(), 7);
    }
    string text = numbers(2000);
    size_t n = json::parse(text)->size();
    std::vector<std::thread> threads;
    std::atomic<int> wrong = 0;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 300; ++i) {
                auto r = json::parse(text);
                if (!r || r->size() != n || (*r)[n - 1].as_int() != int64_t(7 * (n - 1))) {
                    ++wrong;
                }
                auto o = json::parse(large_object(20));
                if (!o || (*o)[of("key19")].as_int() != 19) {
                    ++wrong;
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(wrong.load(), 0);
}

// A typed sequence built at once from what it gathered (a dynamic_array)
// gathers numbers in plain memory: the vector it grew was managed garbage
TEST(EncodingHeap_Tests, ATypedSequenceGathersNumbersInPlainMemory) {
    std::string text = "[";
    for (int i = 0; i < 1000; ++i) {
        text += (i ? "," : "") + std::to_string(i);
    }
    string ints = of(text + "]");
    auto parsed = json::parse<dynamic_array<int>>(ints);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->size(), 1000u);
    EXPECT_EQ((*parsed)[999], 999);
    size_t answer = heap_count::pages_of(200, [&] {
        dynamic_array<int> a(1000);
        EXPECT_EQ(a.size(), 1000u);
    });
    size_t pages = heap_count::pages_of(200, [&] {
        EXPECT_EQ(value_of(json::parse<dynamic_array<int>>(ints)).size(), 1000u);
    });
    EXPECT_LE(pages, answer + 1);
    // a sequence of strings still gathers on the managed heap, and comes out whole
    auto words = json::parse<dynamic_array<string>>(of("[\"a\", \"bb\", \"ccc\"]"));
    ASSERT_TRUE(words.has_value());
    EXPECT_EQ((*words)[2], "ccc");
    auto flags = json::parse<dynamic_array<bool>>(of("[true, false, true]"));
    ASSERT_TRUE(flags.has_value());
    EXPECT_FALSE((*flags)[1]);
}

// The keys a parse shares are kept per thread between calls, the short
// ones only: a key past the table's bound is made anew every time, and a
// key taken from the table is the one asked for, compared in full
TEST(EncodingHeap_Tests, OnlyShortKeysAreKeptBetweenParses) {
    std::string short_key = "name";
    std::string long_key(65, 'k');
    string text = of("{\"" + short_key + "\": 1, \"" + long_key + "\": 2}");
    auto first = json::parse(text);
    auto second = json::parse(text);
    ASSERT_TRUE(first && second);
    auto key_object = [](const json& j, std::string_view k) -> const void* {
        for (auto& m : j.members()) {
            if (m.key.view() == k) {
                return m.key.object();
            }
        }
        return nullptr;
    };
    ASSERT_NE(key_object(*first, short_key), nullptr);
    EXPECT_EQ(key_object(*first, short_key), key_object(*second, short_key));
    ASSERT_NE(key_object(*first, long_key), nullptr);
    EXPECT_NE(key_object(*first, long_key), key_object(*second, long_key));
    // keys of one length are still their own keys
    auto both = json::parse(of("{\"ab\": 1, \"ba\": 2, \"ab2\": 3}"));
    ASSERT_TRUE(both);
    EXPECT_EQ((*both)[of("ab")].as_int(), 1);
    EXPECT_EQ((*both)[of("ba")].as_int(), 2);
    EXPECT_EQ((*both)[of("ab2")].as_int(), 3);
}
