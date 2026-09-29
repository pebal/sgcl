//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog against Go's log/slog: the same records, the lines compared byte
// for byte (tests/slog/oracle_cases.h, made by tools/slog_oracle.go): the
// quoting and escaping of texts as keys, values and messages, the numbers,
// durations, times and levels, and the scenarios of with(), groups and the
// values a type described by its fields becomes.
#include "common.h"
#include "oracle_cases.h"

#include <bit>
#include <cmath>
#include <limits>

using namespace std::chrono_literals;
using slog_test::both;

namespace {
    std::string_view text_of(const slog_oracle::Line& l) {
        return std::string_view(l.text, l.text_n);
    }

    std::string_view json_of(const slog_oracle::Line& l) {
        return std::string_view(l.json, l.json_n);
    }

    // The printable form of a line in a failure's message
    std::string shown(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (c >= 0x20 && c < 0x7f) {
                out += char(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            }
        }
        return out;
    }

    void expect_line(const std::pair<std::string, std::string>& got, const slog_oracle::Line& want, const std::string& what) {
        EXPECT_EQ(shown(got.first), shown(text_of(want))) << what << " (text)";
        EXPECT_EQ(shown(got.second), shown(json_of(want))) << what << " (json)";
    }

    struct Req {
        int id = 5;
        string path = "/a";

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("path", path);
        }
    };

    struct Home {
        string city = "Kraków";

        void describe(encoding::field_list& f) {
            f.add("city", city);
        }
    };

    struct User {
        string name = "Ala";
        Home home;
        int age = 30;

        void describe(encoding::field_list& f) {
            f.add("name", name);
            f.add("home", home);
            f.add("age", age);
        }
    };

    template<class T>
    struct One {
        T v;

        void describe(encoding::field_list& f) {
            f.add("v", v);
        }
    };

    struct Tags {
        vector<string> tags{"a b", "<c>", ""};

        void describe(encoding::field_list& f) {
            f.add("tags", tags);
        }
    };

    struct Numbers {
        vector<double> v{1e6, 0.5, 1e21, 1e-7, 0.0};
        vector<int> i{1, -2, 3};

        void describe(encoding::field_list& f) {
            f.add("v", v);
            f.add("i", i);
        }
    };

    struct Mapped {
        map<string, int> m{{"b", 1}, {"a", 2}, {"c d", 3}};

        void describe(encoding::field_list& f) {
            f.add("m", m);
        }
    };

    struct P {
        int id = 0;
        string path;

        void describe(encoding::field_list& f) {
            f.add("ID", id);
            f.add("Path", path);
        }
    };

    struct Nulls {
        optional<int> o;
        int n = 1;

        void describe(encoding::field_list& f) {
            f.add("o", o);
            f.add("n", n);
        }
    };

    struct Empty {
        void describe(encoding::field_list&) {
        }
    };

    // The records of the scenarios, by the name the oracle gives them
    void run(std::string_view name, const slog::logger& l) {
        if (name == "plain") {
            l.info("server started", "port", 8080, "tls", true);
        } else if (name == "no_attrs") {
            l.info("m");
        } else if (name == "empty_message") {
            l.info("", "k", "v");
        } else if (name == "with_groups") {
            l.with("a", 1).group("g").with("b", 2).group("h").info("m", "c", 3);
        } else if (name == "group_no_attrs") {
            l.group("g").info("m");
        } else if (name == "group_empty_group") {
            l.group("g").info("m", slog::group("e"));
        } else if (name == "group_group_with") {
            l.group("g").group("h").with("x", 1).info("m");
        } else if (name == "group_group_with_call") {
            l.group("g").group("h").with("x", 1).info("m", "y", 2);
        } else if (name == "inline_groups") {
            l.info("m", slog::group("", "a", 1), slog::group("gg", slog::group("hh")), "", nullptr, "a=b", "k");
        } else if (name == "keys_twice") {
            l.info("m", "a", 1, "a", 2);
        } else if (name == "group_empty_key") {
            l.group("g").info("m", "", 1);
        } else if (name == "group_quoted") {
            l.group("a b").info("m", "k", 1, "x=y", 2);
        } else if (name == "with_nothing") {
            l.with().info("m", "a", 1);
        } else if (name == "with_empty_group") {
            l.with(slog::group("e")).group("g").info("m", "a", 1);
        } else if (name == "nested_groups") {
            l.info("m", slog::group("r", "id", 5, slog::group("in", "x", 1.5, "s", "a b")), "after", true);
        } else if (name == "with_then_group") {
            l.with("a", 1).group("g").info("m");
        } else if (name == "group_with_empty_group") {
            l.group("g").with(slog::group("e")).info("m", "a", 1);
        } else if (name == "group_with_empty_group_no_call") {
            l.group("g").with(slog::group("e")).info("m");
        } else if (name == "with_twice") {
            l.with("a", 1).with("b", "x y").info("m", "c", 3);
        } else if (name == "deep") {
            l.group("a").with("x", 1).group("b").group("c").with("y", 2).info("m", "z", 3);
        } else if (name == "deep_with_group_values") {
            l.group("a").with(slog::group("v", "w", 1)).info("m", slog::group("q", "r", "s"));
        } else if (name == "level_debug_offset") {
            l.log(slog::level(-3), "m");
        } else if (name == "described") {
            l.info("request", "req", Req());
        } else if (name == "described_nested") {
            l.info("m", "user", User());
        } else if (name == "described_vector") {
            l.info("m", "r", Tags());
        } else if (name == "described_numbers") {
            l.info("m", "r", Numbers());
        } else if (name == "described_map") {
            l.info("m", "r", Mapped());
        } else if (name == "described_nan") {
            l.info("m", "r", One<vector<double>>{{1.0, std::nan("")}});
        } else if (name == "described_inf") {
            l.info("m", "r", One<vector<double>>{{-std::numeric_limits<double>::infinity()}});
        } else if (name == "described_escapes") {
            l.info("m", "r", One<vector<string>>{{string("\b\f\x1b\x7f\xff\xe2\x80\xa8\xc2\xa0\"\\\t\n")}});
        } else if (name == "described_bools") {
            l.info("m", "r", One<std::array<bool, 2>>{{true, false}});
        } else if (name == "described_records") {
            l.info("m", "r", One<vector<P>>{{P{5, "/a"}, P{6, "x y"}}});
        } else if (name == "described_nested_vectors") {
            l.info("m", "r", One<vector<vector<int>>>{{vector<int>{1, 2}, vector<int>{}, vector<int>{3}}});
        } else if (name == "described_map_of_vectors") {
            l.info("m", "r", One<map<string, vector<string>>>{{{"k", vector<string>{"a", "b"}}}});
        } else if (name == "described_null") {
            l.info("m", "r", Nulls());
        } else if (name == "null_value") {
            l.info("m", "o", optional<int>());
        } else if (name == "described_empty") {
            l.info("m", "r", Empty(), "x", 1);
        } else if (name == "with_described") {
            l.with("req", Req()).info("m", "k", 1);
        } else if (name == "duration_and_time") {
            l.info("m", "took", 1500ms, "at", time::datetime::from_unix_nano(slog_oracle::TimeNs, time::zone::fixed(2h)));
        } else if (name == "sampled_out") {
            l.warn("records sampled out", "count", uint64_t(7));
        } else {
            ADD_FAILURE() << "a scenario the test does not know: " << name;
        }
    }
}

TEST(SlogOracle_Tests, TextsAsKeysValuesAndMessages) {
    slog_test::FixedTime at(slog_oracle::TimeNs);
    for (const auto& c : slog_oracle::Texts) {
        slice<const char> s(c.s, c.n);
        const bool nul = std::string_view(c.s, c.n).find('\0') != std::string_view::npos;
        if (!nul) {   // a key is a C string: none holds a NUL
            expect_line(both([&](const slog::logger& l) { l.info("m", c.s, s); }), c.attr, "attr " + shown(std::string_view(c.s, c.n)));
        }
        expect_line(both([&](const slog::logger& l) { l.info(s); }), c.message, "message " + shown(std::string_view(c.s, c.n)));
    }
}

TEST(SlogOracle_Tests, Floats) {
    slog_test::FixedTime at(slog_oracle::TimeNs);
    for (const auto& c : slog_oracle::Floats) {
        const double f = std::bit_cast<double>(c.bits);
        expect_line(both([&](const slog::logger& l) { l.info("m", "f", f); }), c.line, "float " + std::to_string(c.bits));
    }
}

TEST(SlogOracle_Tests, IntegersDurationsTimesLevels) {
    slog_test::FixedTime at(slog_oracle::TimeNs);
    for (const auto& c : slog_oracle::Ints) {
        expect_line(both([&](const slog::logger& l) { l.info("m", "i", c.value); }), c.line, "int " + std::to_string(c.value));
    }
    for (const auto& c : slog_oracle::Uints) {
        expect_line(both([&](const slog::logger& l) { l.info("m", "u", c.value); }), c.line, "uint " + std::to_string(c.value));
    }
    for (const auto& c : slog_oracle::Durations) {
        const sgcl::duration d{std::chrono::nanoseconds(c.value)};
        expect_line(both([&](const slog::logger& l) { l.info("m", "d", d); }), c.line, "duration " + std::to_string(c.value));
    }
    for (const auto& c : slog_oracle::Times) {
        auto t = c.offset ? time::datetime::from_unix_nano(c.ns, time::zone::fixed(std::chrono::seconds(c.offset)))
                          : time::datetime::from_unix_nano(c.ns, time::zone::utc());
        expect_line(both([&](const slog::logger& l) { l.info("m", "t", t); }), c.line, "time " + std::to_string(c.ns));
    }
    for (const auto& c : slog_oracle::Levels) {
        expect_line(both([&](const slog::logger& l) { l.log(slog::level(int8_t(c.level)), "m"); }), c.line, "level " + std::to_string(c.level));
    }
}

TEST(SlogOracle_Tests, Scenarios) {
    slog_test::FixedTime at(slog_oracle::TimeNs);
    for (const auto& c : slog_oracle::Scenarios) {
        expect_line(both([&](const slog::logger& l) { run(c.name, l); }), c.line, c.name);
    }
}
