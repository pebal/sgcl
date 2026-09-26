//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The differential fuzzing against Go's v2 (tools/json_fuzz.go), run when
// SGCL_JSON_FUZZ names the file of texts and verdicts:
//   - the verdicts: the reader takes a text as one value where jsontext
//     does, parse takes it where Unmarshal into any does;
//   - the tokens are Go's, and the value written back is Go's value;
//   - the reader in pieces of 1 to 16 bytes gives the tokens (or the
//     error, at the same place) it gives whole;
//   - what parse writes, parse reads back as the same value and the same text.
#include "json_common.h"

using namespace json_test;

namespace {
    struct Record {
        std::string text;
        bool valid;
        bool parses;
        uint64_t tokens;
        std::string canonical;
    };

    bool next_record(std::ifstream& f, Record& r) {
        auto u32 = [&](uint32_t& v) {
            unsigned char b[4];
            if (!f.read(reinterpret_cast<char*>(b), 4)) {
                return false;
            }
            v = uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
            return true;
        };
        uint32_t n;
        if (!u32(n)) {
            return false;
        }
        r.text.resize(n);
        f.read(r.text.data(), n);
        char flags;
        f.read(&flags, 1);
        r.valid = flags & 1;
        r.parses = flags & 2;
        unsigned char b[8];
        f.read(reinterpret_cast<char*>(b), 8);
        r.tokens = 0;
        for (int k = 7; k >= 0; --k) {
            r.tokens = r.tokens << 8 | b[k];
        }
        u32(n);
        r.canonical.resize(n);
        f.read(r.canonical.data(), n);
        return bool(f);
    }

    std::string hex_of(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            char b[4];
            snprintf(b, sizeof b, "%02x", c);
            out += b;
        }
        return out;
    }
}

TEST(JsonFuzz_Tests, AgainstGo) {
    const char* path = std::getenv("SGCL_JSON_FUZZ");
    if (!path) {
        GTEST_SKIP() << "SGCL_JSON_FUZZ names no file (go run tools/json_fuzz.go <count> <seed> > file)";
    }
    std::ifstream f(path, std::ios::binary);
    ASSERT_TRUE(f) << path;
    Record rec;
    size_t count = 0, valid = 0, parsed = 0, failures = 0;
    while (next_record(f, rec) && failures < 20) {
        ++count;
        auto& t = rec.text;
        // the reader, whole: its tokens and how many values at the top
        json::reader whole(text(t));
        std::string tokens;
        size_t top = 0;
        while (auto tok = whole.next()) {
            add_token(tokens, *tok);
            if (whole.depth() == 0 && tok->type() != json::token::kind::key) {
                ++top;
            }
        }
        bool reads = !whole.last_error() && top == 1;
        auto p = json::parse(text(t));
        bool ok = true;
        if (reads != rec.valid || p.has_value() != rec.parses) {
            ADD_FAILURE() << "verdict: reads " << reads << " (Go " << rec.valid << "), parses " << p.has_value() << " (Go " << rec.parses << ") " << hex_of(t);
            ok = false;
        }
        if (reads && rec.valid && hash_of(tokens) != rec.tokens) {
            ADD_FAILURE() << "tokens of " << hex_of(t) << "\n" << tokens;
            ok = false;
        }
        if (p && rec.parses) {
            ++parsed;
            auto s = p->to_string();
            if (s.view() != rec.canonical) {
                ADD_FAILURE() << "written: " << s.view() << "\nGo:      " << rec.canonical << "\n" << hex_of(t);
                ok = false;
            }
            auto again = json::parse(s);
            if (!again || !(*again == *p) || again->to_string() != s) {
                ADD_FAILURE() << "not read back: " << s.view();
                ok = false;
            }
        }
        valid += rec.valid;
        // in pieces: the same tokens, or the same error at the same place
        size_t n = 1 + count % 16;
        json::reader pieces(sgcl::make_tracked<dribble>(t, n));
        std::string ptokens;
        while (auto tok = pieces.next()) {
            add_token(ptokens, *tok);
        }
        if (ptokens != tokens || pieces.last_error().has_value() != whole.last_error().has_value()
            || (pieces.last_error() && !(*pieces.last_error() == *whole.last_error()))) {
            ADD_FAILURE() << "in pieces of " << n << ": " << hex_of(t) << "\n"
                          << (whole.last_error() ? whole.last_error()->message().c_str() : "") << " / "
                          << (pieces.last_error() ? pieces.last_error()->message().c_str() : "");
            ok = false;
        }
        // read whole from the stream in pieces: the value parse gives
        if (p) {
            json::reader rv(sgcl::make_tracked<dribble>(t, n));
            auto v = rv.read();
            if (!v || !(*v == *p)) {
                ADD_FAILURE() << "read() in pieces of " << n << ": " << hex_of(t);
                ok = false;
            }
        }
        failures += !ok;
    }
    EXPECT_GT(count, 0u);
    std::cout << count << " texts, " << valid << " valid for Go, " << parsed << " parsed\n";
}
