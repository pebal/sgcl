//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What a record costs on the managed heap: nothing (DESIGN 283). The line
// is made in the thread's plain memory, a logger's with() was rendered
// when it was made, a type described by its fields is read through its
// operations, a group is on the stack of the call; a buffered record goes
// into its worker's plain batch. Twenty thousand records of each kind,
// counted in pages the managed heap gives out with the collector parked
// (tests/managed_pages.h): a byte per record would show as a page.
#include "common.h"
#include "tests/managed_pages.h"

using namespace std::chrono_literals;

namespace {
    struct Req {
        int id = 5;
        string path = "/users/42";
        double score = 0.75;

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("path", path);
            f.add("score", score);
        }
    };

    struct Quiet {
        size_t* n;

        void handle(const slog::record& r) const {
            for (auto a : r) {
                (void)a;
                ++*n;
            }
        }
    };

    constexpr size_t Records = 20000;
}

TEST(SlogHeap_Tests, ARecordTakesNoManagedMemory) {
    const Req req;
    const string user = "ala";
    auto text = slog::logger(io::discard).with("service", "api", "version", 3).group("http");
    auto json = slog::logger(slog::options{.out = io::discard, .json = true}).with("service", "api").group("http").with("node", "n1");
    auto buffered = slog::logger(slog::options{.out = io::discard, .buffered = true}).with("service", "api");
    size_t seen = 0;
    auto custom = slog::logger(Quiet{&seen}).with("service", "api");
    auto sampled = slog::logger(slog::options{.out = io::discard, .sample_first = 100, .sample_then = 10, .sample_per = 1s});

    size_t pages = heap_count::pages_of(Records, [&] {
        text.info("request", "method", "GET", "status", 200, "took", 1500us, "user", user);
    });
    EXPECT_LE(pages, 1u) << "text";
    pages = heap_count::pages_of(Records, [&] {
        json.info("request", "req", req, "ok", true, slog::group("peer", "port", 443, "tls", true));
    });
    EXPECT_LE(pages, 1u) << "json with a described type and a group";
    pages = heap_count::pages_of(Records, [&] {
        text.warn("slow", "req", req, "ratio", 0.125, "none", optional<int>());
    });
    EXPECT_LE(pages, 1u) << "text with a described type";
    pages = heap_count::pages_of(Records, [&] {
        buffered.info("batched", "i", 7, "user", user);
    });
    EXPECT_LE(pages, 1u) << "buffered";
    buffered.flush();
    pages = heap_count::pages_of(Records, [&] {
        custom.info("to a handler", "i", 7, "req", req);
    });
    EXPECT_LE(pages, 1u) << "a handler of the program";
    EXPECT_GT(seen, 0u);
    pages = heap_count::pages_of(Records, [&] {
        sampled.info("sampled", "i", 7);
    });
    EXPECT_LE(pages, 1u) << "sampled";
    pages = heap_count::pages_of(Records, [&] {
        text.debug("below the level", "req", req);
    });
    EXPECT_LE(pages, 1u) << "a record below the level";
}

// The probe's control: a value that makes its text as a managed string
// (to_text) costs that string at every record, and the count shows it
TEST(SlogHeap_Tests, AValueThatMakesAStringCostsTheString) {
    struct Tagged {
        int n;

        string to_text() const {
            return string("tag-number-") + string(std::to_string(n));
        }
    };
    auto text = slog::logger(io::discard);
    size_t pages = heap_count::pages_of(Records, [&] {
        text.info("m", "t", Tagged{7});
    });
    EXPECT_GT(pages, 1u);
}

TEST(SlogHeap_Tests, TheFreeFunctionsTakeNoManagedMemory) {
    auto before = slog::default_logger();
    slog::set_default(slog::logger(slog::options{.out = io::discard, .json = true}));
    size_t pages = heap_count::pages_of(Records, [&] {
        slog::info("default", "k", 1, "s", "text");
    });
    slog::set_default(before);
    EXPECT_LE(pages, 1u);
}

// The growth of a line cannot throw: a failed realloc ends the program, as
// running out of managed memory does, so the quoting and escaping that
// write through it cannot throw either. They threw bad_alloc.
TEST(SlogHeap_Tests, GrowingALineCannotThrow) {
    slog::detail::Buf b;
    static_assert(noexcept(b.put('a')));
    static_assert(noexcept(b.put("a", 1)));
    std::string_view text;
    static_assert(noexcept(b.put(text)));
    static_assert(noexcept(b.reserve(1)));
    static_assert(noexcept(slog::detail::text_escaped(b, "a", 1)));
    static_assert(noexcept(slog::detail::text_string(b, "a", 1)));
    static_assert(noexcept(slog::detail::json_escaped(b, "a", 1)));
    static_assert(noexcept(slog::detail::json_string(b, "a", 1)));
    static_assert(noexcept(slog::detail::put_uint(b, 1)));
    static_assert(noexcept(slog::detail::put_int(b, -1)));
    slog::detail::Buf line;
    for (int i = 0; i < 100000; ++i) {
        line.put('x');
    }
    slog::detail::json_string(line, "\"\n", 2);
    EXPECT_EQ(line.view().size(), 100000u + 6);
}
