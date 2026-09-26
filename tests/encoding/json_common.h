//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the JSON tests share: the generator the oracle uses
// (tools/json_oracle.go), the tokens of a reader written as the oracle
// writes them, and the files of JSONTestSuite.
#pragma once

#include "common.h"
#include "json_tests.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace json_test {
    using namespace enc_test;
    using sgcl::encoding::json;

    struct rng {
        uint64_t state;

        uint64_t next() {
            state += 0x9E3779B97F4A7C15ull;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
    };

    struct fnv_hash {
        uint64_t h = 0xcbf29ce484222325ull;

        void add(std::string_view s) {
            for (char c : s) {
                h ^= uint8_t(c);
                h *= 0x100000001b3ull;
            }
        }
    };

    // A random literal of the grammar: tools/json_oracle.go's literal()
    inline std::string literal(rng& r) {
        std::string s;
        if (r.next() & 1) {
            s += '-';
        }
        uint64_t kind = r.next() % 16;
        int digits;
        if (kind < 12) {
            digits = 1 + int(r.next() % 20);
        } else if (kind < 15) {
            digits = 1 + int(r.next() % 40);
        } else {
            digits = 400 + int(r.next() % 500);
        }
        int int_digits = int(r.next() % uint64_t(digits + 1));
        if (int_digits == 0) {
            s += '0';
        } else {
            s += char('1' + r.next() % 9);
            for (int i = 1; i < int_digits; ++i) {
                s += char('0' + r.next() % 10);
            }
        }
        if (int frac = digits - int_digits; frac > 0) {
            s += '.';
            for (int i = 0; i < frac; ++i) {
                s += char('0' + r.next() % 10);
            }
        }
        if (r.next() % 3 != 0) {
            s += 'e';
            switch (r.next() % 3) {
                case 0: s += '-'; break;
                case 1: s += '+'; break;
            }
            s += std::to_string(int(r.next() % 341));
        }
        return s;
    }

    // The letter of a token, as the oracle writes it
    inline char letter(json::token::kind k) {
        switch (k) {
            case json::token::kind::begin_object: return '{';
            case json::token::kind::end_object: return '}';
            case json::token::kind::begin_array: return '[';
            case json::token::kind::end_array: return ']';
            case json::token::kind::key: return 'k';
            case json::token::kind::string: return 's';
            case json::token::kind::number: return 'n';
            case json::token::kind::boolean: return 't';
            case json::token::kind::null: return 'z';
        }
        return '?';
    }

    inline void add_token(std::string& out, const json::token& t) {
        char l = letter(t.type());
        if (l == 't' && t.text().view() == "false") {
            l = 'f';
        }
        out += l;
        if (l == '{' || l == '}' || l == '[' || l == ']') {
            out += l;
        } else {
            out.append(t.text().view());
        }
        out += '\n';
    }

    // Every token of the reader to its end: the text of them, or nullopt
    // at an error
    inline std::optional<std::string> all_tokens(json::reader& r) {
        std::string out;
        while (auto t = r.next()) {
            add_token(out, *t);
        }
        if (r.last_error()) {
            return std::nullopt;
        }
        return out;
    }

    inline uint64_t hash_of(std::string_view s) {
        fnv_hash f;
        f.add(s);
        return f.h;
    }

    inline std::filesystem::path suite_dir() {
        const char* home = std::getenv("HOME");
        return std::filesystem::path(home ? home : "") / "Programming" / "oracles" / "JSONTestSuite" / "test_parsing";
    }

    inline std::string read_file(const std::filesystem::path& p) {
        std::ifstream f(p, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    inline sgcl::string text(std::string_view s) {
        return sgcl::string(s);
    }
}
