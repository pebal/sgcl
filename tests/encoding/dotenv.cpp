//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// dotenv: the .env of docker compose, python-dotenv and godotenv as dotenv.h
// states it. Against bash (tools/dotenv_oracle.py writes dotenv_tests.h) for
// the subset shell and .env agree on: plain values, quotes, comments, the
// expansions. Then what is dotenv's own (escapes in double quotes, blanks
// around '=', a comment's blank, CRLF, keys with dots), the errors and their
// places, the environment, the typed lookups, the writer's round trip, new
// versions, apply, and the boundaries.
#include "common.h"
#include "dotenv_tests.h"

#include <cstdlib>
#include <map>
#include <random>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string escape(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (c >= 0x20 && c < 0x7F && c != ';' && c != '\\') {
                out += char(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            }
        }
        return out;
    }

    // The entries sorted by key, in the oracle's form; the environment not read
    std::string value_of(std::string_view text) {
        dotenv::options o;
        o.use_environment = false;
        auto v = dotenv::parse(sgcl::string(text), o);
        if (!v) {
            return "error";
        }
        std::map<std::string, std::string> sorted;
        for (auto& m : v->members()) {
            sorted[std::string(m.key.view())] = std::string(m.value.view());
        }
        std::string out;
        for (auto& [k, val] : sorted) {
            out += k + "=" + escape(val) + ";";
        }
        return out;
    }

    dotenv d(const char* text) {
        dotenv::options o;
        o.use_environment = false;
        auto v = dotenv::parse(sgcl::string(text), o);
        if (!v) {
            throw std::runtime_error(std::string("bad test .env: ") + text + ": " + std::string(v.error().message().view()));
        }
        return *v;
    }
}

TEST(Dotenv_Tests, AsBashReadsThem) {
    // a line's \r\n is its end in a .env (bash keeps the \r in the value)
    std::map<std::string_view, std::string> differs = {
        {"crlf lines", "A=1;B=two;C=three;"},
    };
    size_t compared = 0;
    for (auto& c : dotenv_oracle::reads) {
        auto it = differs.find(c.name);
        std::string want = it == differs.end() ? std::string(c.value) : it->second;
        EXPECT_EQ(value_of(c.text), want) << c.name << "\n" << c.text;
        ++compared;
    }
    EXPECT_GT(compared, 20u);
}

TEST(Dotenv_Tests, OwnRules) {
    dotenv v = d("A = spaced\n  B=  trimmed  \nC=\"tab\\tnl\\ncr\\rq\\\"bs\\\\d\\$x\"\nD=\"other \\q kept\"\nE=a #c\nF=a#b\n"
                 "G=\"x\" # c\nH='y'#c\nexport I=1\nexport=2\nJ.K=dots\n_=u\nL=$1x\nM=${A}\n");
    EXPECT_EQ(v.get("A"), "spaced");
    EXPECT_EQ(v.get("B"), "trimmed");
    EXPECT_EQ(v.get("C"), "tab\tnl\ncr\rq\"bs\\d$x");
    EXPECT_EQ(v.get("D"), "other \\q kept");
    EXPECT_EQ(v.get("E"), "a");
    EXPECT_EQ(v.get("F"), "a#b");
    EXPECT_EQ(v.get("G"), "x");
    EXPECT_EQ(v.get("H"), "y");
    EXPECT_EQ(v.get("I"), "1");
    EXPECT_EQ(v.get("export"), "2");
    EXPECT_EQ(v.get("J.K"), "dots");
    EXPECT_EQ(v.get("_"), "u");
    EXPECT_EQ(v.get("L"), "$1x");   // '$' before a digit: kept
    EXPECT_EQ(v.get("M"), "spaced");
    // a key given twice: its first place, its last value
    dotenv twice = d("A=1\nB=2\nA=3");
    EXPECT_EQ(twice.members()[0].key, "A");
    EXPECT_EQ(twice.members()[0].value, "3");
    EXPECT_EQ(twice.size(), 2u);
    // expansion off: every '$' as written
    dotenv::options raw;
    raw.expand = false;
    EXPECT_EQ(dotenv::parse("A=1\nB=${A}$A\nC=\"${A}\"", raw)->get("B"), "${A}$A");
    // ${NAME:?message}
    auto required = dotenv::parse("A=${NOPE:?set NOPE first}");
    ASSERT_FALSE(required);
    EXPECT_EQ(required.error().code(), errc::missing_field);
    EXPECT_EQ(std::string(required.error().message().view()), "1:3: NOPE: set NOPE first");
    EXPECT_EQ(d("A=x\nB=${A:?}").get("B"), "x");
    EXPECT_FALSE(dotenv::parse("E=\nB=${E:?}"));
    EXPECT_TRUE(dotenv::parse("E=\nB=${E?}"));
    // expansions that double a value at every line stop at max_size
    std::string bomb = "A=0123456789abcdef\n";
    for (int k = 0; k < 40; ++k) {
        bomb += "A=$A${A}\n";
    }
    auto big = dotenv::parse(sgcl::string(bomb));
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), errc::limit_exceeded);
    dotenv::options small;
    small.max_size = 40;
    EXPECT_TRUE(dotenv::parse("A=0123456789\nB=$A$A$A", small));   // 10 + 30
    EXPECT_FALSE(dotenv::parse("A=0123456789\nB=$A$A$A$A", small));
    EXPECT_FALSE(dotenv::parse("A=0123456789\nB=${NOPE:-${A}${A}${A}${A}}", small));
    // a default inside double quotes may hold escapes
    EXPECT_EQ(d("A=\"${NOPE:-a\\\"b}\"").get("A"), "a\"b");
}

TEST(Dotenv_Tests, Environment) {
    ::setenv("SGCL_DOTENV_TEST_VAR", "from-env", 1);
    auto v = dotenv::parse("A=$SGCL_DOTENV_TEST_VAR\nSGCL_DOTENV_TEST_VAR=file\nB=$SGCL_DOTENV_TEST_VAR");
    ASSERT_TRUE(v);
    EXPECT_EQ(v->get("A"), "from-env");
    EXPECT_EQ(v->get("B"), "file");   // the file's own key first
    dotenv::options o;
    o.use_environment = false;
    EXPECT_EQ(dotenv::parse("A=$SGCL_DOTENV_TEST_VAR", o)->get("A"), "");
    // apply: kept unless overwrite
    dotenv::from({{"SGCL_DOTENV_TEST_VAR", "applied"}, {"SGCL_DOTENV_TEST_NEW", "new"}}).apply();
    EXPECT_STREQ(std::getenv("SGCL_DOTENV_TEST_VAR"), "from-env");
    EXPECT_STREQ(std::getenv("SGCL_DOTENV_TEST_NEW"), "new");
    dotenv::from({{"SGCL_DOTENV_TEST_VAR", "applied"}}).apply(true);
    EXPECT_STREQ(std::getenv("SGCL_DOTENV_TEST_VAR"), "applied");
    EXPECT_THROW(dotenv::from({{"A=B", "x"}}).apply(), sgcl::invalid_argument);
    EXPECT_THROW(dotenv::from({{"", "x"}}).apply(), sgcl::invalid_argument);
    ::unsetenv("SGCL_DOTENV_TEST_VAR");
    ::unsetenv("SGCL_DOTENV_TEST_NEW");
}

TEST(Dotenv_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"A", errc::syntax, 1, 2},
        {"1A=x", errc::syntax, 1, 1},
        {"A-B=x", errc::syntax, 1, 2},
        {"=x", errc::syntax, 1, 1},
        {"A='open", errc::unexpected_end, 1, 3},
        {"A=\"open\nB=1", errc::unexpected_end, 1, 3},
        {"A='x' y", errc::syntax, 1, 7},
        {"A=\"x\"y", errc::syntax, 1, 6},
        {"A=${}", errc::syntax, 1, 3},
        {"A=${B", errc::unexpected_end, 1, 3},
        {"A=${B:-x", errc::unexpected_end, 1, 3},
        {"A=${B*x}", errc::syntax, 1, 6},
        {"A=1\nB=${C:?no}", errc::missing_field, 2, 3},
        {"A=\xff", errc::invalid_utf8, 1, 3},
    };
    for (auto& c : cases) {
        dotenv::options o;
        o.use_environment = false;
        auto r = dotenv::parse(sgcl::string(c.text), o);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text;
        EXPECT_EQ(r.error().column(), c.column) << c.text;
    }
    auto nul = dotenv::parse(sgcl::string(std::string("A=x\0", 4)));
    ASSERT_FALSE(nul);
    EXPECT_EQ(nul.error().code(), errc::invalid_character);
}

TEST(Dotenv_Tests, Lookups) {
    dotenv v = d("PORT=8080\nRATIO=0.5\nDEBUG=yes\nNAME=app\nBAD=12a");
    EXPECT_TRUE(v.contains("PORT"));
    EXPECT_FALSE(v.contains("port"));
    EXPECT_EQ(v.get("NAME"), "app");
    EXPECT_FALSE(v.get("NONE"));
    EXPECT_EQ(v.get("NONE", "x"), "x");
    EXPECT_EQ(v.get("NAME", "x"), "app");
    EXPECT_EQ(v.get_int("PORT"), 8080);
    EXPECT_FALSE(v.get_int("BAD"));
    EXPECT_EQ(v.get_int("BAD", 1), 1);
    EXPECT_EQ(v.get_int("NONE", 2), 2);
    EXPECT_EQ(v.get_double("RATIO"), 0.5);
    EXPECT_EQ(v.get_double("NAME", 2.5), 2.5);
    EXPECT_EQ(v.get_bool("DEBUG"), true);
    EXPECT_FALSE(v.get_bool("NAME"));
    EXPECT_EQ(v.get_bool("NAME", true), true);
    EXPECT_EQ(v.members().size(), 5u);
    EXPECT_EQ(v.members()[4].key, "BAD");
}

TEST(Dotenv_Tests, Writing) {
    dotenv v = dotenv::from({{"PLAIN", "value"}, {"EMPTY", ""}, {"SPACED", "two words"}, {"LEAD", " x"}, {"QUOTES", "a\"b'c"},
                             {"DOLLAR", "$HOME"}, {"HASH", "a #b"}, {"LINES", "one\ntwo\r\tthree"}, {"BACK", "a\\b"}});
    EXPECT_EQ(std::string(v.to_string().view()),
              "PLAIN=value\nEMPTY=\nSPACED=two words\nLEAD=\" x\"\nQUOTES=\"a\\\"b'c\"\nDOLLAR=\"\\$HOME\"\nHASH=\"a #b\"\n"
              "LINES=\"one\\ntwo\\r\\tthree\"\nBACK=\"a\\\\b\"\n");
    auto back = dotenv::parse(v.to_string());
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, v);
    EXPECT_EQ(dotenv().to_string(), "");
    EXPECT_THROW(dotenv::from({{"1A", "x"}}).to_string(), sgcl::invalid_argument);
    EXPECT_THROW(dotenv::from({{"A B", "x"}}).to_string(), sgcl::invalid_argument);
    EXPECT_THROW(dotenv::from({{"A", sgcl::string(std::string("a\0b", 3))}}).to_string(), sgcl::invalid_argument);
}

TEST(Dotenv_Tests, WrittenReadsBack) {
    std::mt19937_64 r(5);
    const char* parts[] = {"a", " ", "\t", "\n", "\r", "'", "\"", "\\", "$", "{", "}", "#", "=", "${X}", "\x01", "日本", ":-"};
    for (int k = 0; k < 3000; ++k) {
        sgcl::vector<dotenv::member> ms;
        for (int n = int(r() % 5); n > 0; --n) {
            std::string value;
            for (size_t p = r() % 6; p > 0; --p) {
                value += parts[r() % (sizeof parts / sizeof *parts)];
            }
            ms.push_back({sgcl::string("K" + std::to_string(r() % 7)), sgcl::string(value)});
        }
        dotenv v = dotenv::from(ms);
        sgcl::string text = v.to_string();
        auto back = dotenv::parse(text);
        ASSERT_TRUE(back) << text << back.error().message();
        EXPECT_EQ(*back, v) << text;
        EXPECT_EQ(back->to_string(), text);
    }
}

TEST(Dotenv_Tests, NewVersions) {
    dotenv v = d("A=1\nB=2");
    EXPECT_EQ(v.set("A", "9").get("A"), "9");
    EXPECT_EQ(v.set("A", "9").members()[0].key, "A");
    EXPECT_EQ(v.set("C", "3").members()[2].key, "C");
    EXPECT_EQ(v.erase("A").size(), 1u);
    EXPECT_EQ(v.erase("Z"), v);
    EXPECT_EQ(v.get("A"), "1");
    EXPECT_NE(v, d("B=2\nA=1"));   // order counts
    EXPECT_EQ(dotenv::from({{"A", "1"}, {"A", "2"}}).get("A"), "2");
    sgcl::vector<dotenv::member> ms;
    ms.push_back({"X", "1"});
    EXPECT_EQ(dotenv::from(ms).get("X"), "1");
}

TEST(Dotenv_Tests, Boundaries) {
    dotenv none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.members().empty());
    EXPECT_EQ(none, d(""));
    EXPECT_EQ(none, d("# only\n\n   \n"));
    EXPECT_EQ(d("\xEF\xBB\xBF" "A=1").get("A"), "1");
    dotenv a = d("A=1");
    dotenv b = std::move(a);
    EXPECT_EQ(b.get("A"), "1");
    std::string text = "export A=1\nB=\"x ${A:-y}\n z\"\nC='q'\nD=e # f\n";
    for (size_t n = 0; n <= text.size(); ++n) {
        (void)dotenv::parse(sgcl::string(text.substr(0, n)));
    }
    std::string doc = "A=1\nB=2\n";
    for (size_t piece : {1, 3, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(doc, piece));
        auto r = dotenv::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ(r->get("B"), "2");
    }
    sgcl::io::reader bad(make_tracked<failing>("A=1"));
    EXPECT_EQ(dotenv::parse(bad).error().code(), errc::io);
    auto task = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 2));
        auto r = co_await dotenv::async_parse(in);
        co_return r && r->get("A") == "1" ? 1 : -1;
    }(doc));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}
