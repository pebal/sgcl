//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ini: the INI of Python's configparser as ini.h states it. Against
// configparser (tools/ini_oracle.py writes ini_tests.h): the sections of the
// edge cases of every construct, valid and not, and of 300 documents the
// writer here wrote of random sections, read here as configparser reads them,
// the stated differences by name. Then the errors and their places, the
// options, the typed lookups, the writer and its refusals, new versions, and
// the boundaries.
#include "common.h"
#include "ini_tests.h"

#include <map>
#include <random>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string escape(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (c >= 0x20 && c < 0x7F && c != ';' && c != '[' && c != ']' && c != '=' && c != '\\') {
                out += char(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            }
        }
        return out;
    }

    // The sections in the oracle's form (tools/ini_oracle.py)
    std::string value_of(std::string_view text) {
        auto v = ini::parse(sgcl::string(text));
        if (!v) {
            return "error";
        }
        std::string out;
        for (auto& s : v->sections()) {
            out += "[" + escape(s.name.view()) + "]";
            for (auto& m : s.members) {
                out += escape(m.key.view()) + "=" + escape(m.value.view()) + ";";
            }
        }
        return out;
    }

    ini i(const char* text) {
        auto v = ini::parse(sgcl::string(text));
        if (!v) {
            throw std::runtime_error(std::string("bad test INI: ") + text + ": " + std::string(v.error().message().view()));
        }
        return *v;
    }
}

TEST(Ini_Tests, AsConfigparserReadsThem) {
    // the stated difference: after a section's ']' only blanks or a comment
    // (configparser passes over whatever follows)
    std::map<std::string_view, std::string> differs = {
        {"keys with spaces and symbols", "error"},
    };
    size_t compared = 0, refused = 0;
    for (auto& c : ini_oracle::reads) {
        auto it = differs.find(c.name);
        std::string want = it == differs.end() ? std::string(c.value) : it->second;
        EXPECT_EQ(value_of(c.text), want) << c.name << "\n" << c.text;
        ++compared;
        refused += want == "error";
    }
    EXPECT_GT(compared, 330u);
    EXPECT_GE(refused, 6u);
}

TEST(Ini_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"[a]\nx = 1\n[a]", errc::duplicate_key, 3, 1},
        {"[a]\nx = 1\ny = 2\nx = 3", errc::duplicate_key, 4, 1},
        {"[s]\nlonely", errc::syntax, 2, 1},
        {"[s]\n  = 1", errc::syntax, 2, 3},
        {"[s] x", errc::syntax, 1, 5},
        {"[]", errc::syntax, 1, 1},
        {"a = \xff", errc::invalid_utf8, 1, 5},
    };
    for (auto& c : cases) {
        auto r = ini::parse(sgcl::string(c.text));
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text;
        EXPECT_EQ(r.error().column(), c.column) << c.text;
    }
    auto nul = ini::parse(sgcl::string(std::string("a = x\0y", 7)));
    ASSERT_FALSE(nul);
    EXPECT_EQ(nul.error().code(), errc::invalid_character);
    EXPECT_EQ(nul.error().column(), 6u);
    // a line deeper than its key's continues the key's value, whatever it holds
    EXPECT_EQ(i("[a]\nx = 1\n  x = 2").get("a", "x"), "1\nx = 2");
}

TEST(Ini_Tests, Options) {
    ini::options o;
    o.allow_duplicates = true;
    auto d = ini::parse("[a]\nx = 1\ny = 2\n[b]\n[a]\nx = 3\nz = 4", o);
    ASSERT_TRUE(d);
    EXPECT_EQ(d->size(), 2u);
    EXPECT_EQ(d->get("a", "x"), "3");
    EXPECT_EQ(d->sections()[0].members.size(), 3u);
    EXPECT_EQ(d->sections()[0].members[0].key, "x");   // the first place kept
    ini::options n;
    n.allow_no_value = true;
    auto v = ini::parse("[s]\nlonely\nother = 1", n);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->get("s", "lonely"), "");
    EXPECT_EQ(v->get("s", "other"), "1");
}

TEST(Ini_Tests, Lookups) {
    ini c = i("top = yes\n[server]\nhost = example.com\nport = 8080\nratio = 0.25\ntls = On\nbad = 12x\nneg = -7\nplus = +3\n");
    EXPECT_EQ(c.size(), 2u);
    EXPECT_EQ(c.sections()[0].name, "");
    EXPECT_TRUE(c.contains(""));
    EXPECT_TRUE(c.contains("server"));
    EXPECT_FALSE(c.contains("Server"));   // exact
    EXPECT_TRUE(c.contains("server", "host"));
    EXPECT_FALSE(c.contains("server", "HOST"));
    EXPECT_FALSE(c.contains("none", "host"));
    EXPECT_EQ(c.get("server", "host"), "example.com");
    EXPECT_FALSE(c.get("server", "missing"));
    EXPECT_EQ(c.get("server", "missing", "dflt"), "dflt");
    EXPECT_EQ(c.get("server", "host", "dflt"), "example.com");
    EXPECT_EQ(c.get_int("server", "port"), 8080);
    EXPECT_EQ(c.get_int("server", "neg"), -7);
    EXPECT_EQ(c.get_int("server", "plus"), 3);
    EXPECT_FALSE(c.get_int("server", "bad"));
    EXPECT_FALSE(c.get_int("server", "host"));
    EXPECT_EQ(c.get_int("server", "bad", 5), 5);
    EXPECT_EQ(c.get_int("none", "x", 6), 6);
    EXPECT_EQ(c.get_double("server", "ratio"), 0.25);
    EXPECT_EQ(c.get_double("server", "port"), 8080.0);
    EXPECT_EQ(c.get_double("server", "host", 1.5), 1.5);
    EXPECT_EQ(c.get_bool("server", "tls"), true);
    EXPECT_EQ(c.get_bool("", "top"), true);
    EXPECT_FALSE(c.get_bool("server", "port"));
    EXPECT_EQ(c.get_bool("server", "port", false), false);
    for (const char* t : {"true", "TRUE", "Yes", "on", "1"}) {
        EXPECT_EQ(ini().set("s", "b", t).get_bool("s", "b"), true) << t;
    }
    for (const char* f : {"false", "No", "OFF", "0"}) {
        EXPECT_EQ(ini().set("s", "b", f).get_bool("s", "b"), false) << f;
    }
    EXPECT_FALSE(ini().set("s", "b", "2").get_bool("s", "b"));
    EXPECT_FALSE(ini().set("s", "n", "99999999999999999999").get_int("s", "n"));
    EXPECT_FALSE(ini().set("s", "n", "+-1").get_int("s", "n"));
}

TEST(Ini_Tests, Writing) {
    ini c = ini().set("server", "host", "example.com").set("server", "port", "8080").set("", "name", "app").set("empty", "k", "").set("multi", "text", "one\ntwo");
    EXPECT_EQ(std::string(c.to_string().view()), "name = app\n\n[server]\nhost = example.com\nport = 8080\n\n[empty]\nk =\n\n[multi]\ntext = one\n    two\n");
    EXPECT_EQ(c.sections()[0].name, "");   // the section "" made first
    auto back = ini::parse(c.to_string());
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, c);
    EXPECT_EQ(ini().to_string(), "");
    EXPECT_EQ(ini().set("s", "k", "v").erase("s", "k").to_string(), "[s]\n");
    // what would read back otherwise is refused
    for (auto [key, value] : std::initializer_list<std::pair<const char*, const char*>>{
             {"", "v"}, {" k", "v"}, {"k ", "v"}, {"a=b", "v"}, {"a:b", "v"}, {"a\nb", "v"}, {";k", "v"}, {"#k", "v"}, {"[k]", "v"},
             {"[k", "v]"}, {"k", " v"}, {"k", "v "}, {"k", "a\n\nb"}, {"k", "a\n b"}, {"k", "a\n;b"}, {"k", "a\r\nb"}, {"k", "a\r"}}) {
        EXPECT_THROW(ini().set("s", key, value).to_string(), sgcl::invalid_argument) << "[" << key << "] [" << value << "]";
    }
    EXPECT_THROW(ini().set("a\nb", "k", "v").to_string(), sgcl::invalid_argument);
    EXPECT_THROW(ini().set("", "\xEF\xBB\xBFk", "v").to_string(), sgcl::invalid_argument);   // the reading would pass over it
    EXPECT_EQ(ini().set("s", "\xEF\xBB\xBFk", "v").to_string(), "[s]\n\xEF\xBB\xBFk = v\n");
    EXPECT_THROW(ini().set("s", "k", sgcl::string(std::string("a\0b", 3))).to_string(), sgcl::invalid_argument);
    // a value whose first line is empty reads back, a key of '[' first without a ']', a carriage return inside a line
    for (ini e : {ini().set("s", "k", "\nsecond"), ini().set("s", "[k", "v"), ini().set("s", "k", "a\rb"), ini().set("s\r", "k\rx", "v")}) {
        EXPECT_EQ(*ini::parse(e.to_string()), e) << e.to_string();
    }
}

TEST(Ini_Tests, WrittenReadsBack) {
    std::mt19937_64 r(11);
    const char* parts[] = {"a", "b c", "x.y", "[", "]", ";", "#", "=", ":", " ", "\n", "日本", "?"};
    auto word = [&](size_t max) {
        std::string s;
        for (size_t n = r() % max; n > 0; --n) {
            s += parts[r() % (sizeof parts / sizeof *parts)];
        }
        return sgcl::string(s);
    };
    size_t written = 0;
    for (int k = 0; k < 3000; ++k) {
        ini v;
        for (int n = int(r() % 6); n > 0; --n) {
            v = v.set(word(3), word(3), word(5));
        }
        sgcl::string text;
        try {
            text = v.to_string();
        } catch (const sgcl::invalid_argument&) {
            continue;
        }
        ++written;
        auto back = ini::parse(text);
        ASSERT_TRUE(back) << text << back.error().message();
        // an empty section "" is not written: compared without it
        ini want = v;
        if (want.contains("") && want.sections()[0].members.empty()) {
            want = want.erase("");
        }
        EXPECT_EQ(*back, want) << text;
        EXPECT_EQ(back->to_string(), text);
    }
    EXPECT_GT(written, 300u);
}

TEST(Ini_Tests, NewVersions) {
    ini c = i("[a]\nx = 1\ny = 2\n[b]\nz = 3");
    EXPECT_EQ(c.set("a", "x", "9").get("a", "x"), "9");
    EXPECT_EQ(c.set("a", "x", "9").sections()[0].members[0].key, "x");   // in its place
    EXPECT_EQ(c.set("a", "w", "0").sections()[0].members[2].key, "w");   // at the end
    EXPECT_EQ(c.set("new", "k", "v").sections()[2].name, "new");
    EXPECT_EQ(c.erase("a", "x").get("a", "x"), nullopt);
    EXPECT_EQ(c.erase("a", "missing"), c);
    EXPECT_EQ(c.erase("a").size(), 1u);
    EXPECT_EQ(c.erase("missing"), c);
    EXPECT_EQ(c.get("a", "x"), "1");   // unchanged
    EXPECT_NE(c, c.set("a", "x", "9"));
    EXPECT_NE(c, i("[b]\nz = 3\n[a]\nx = 1\ny = 2"));   // order counts
}

TEST(Ini_Tests, Boundaries) {
    ini none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.sections().empty());
    EXPECT_FALSE(none.contains(""));
    EXPECT_FALSE(none.get("", "k"));
    EXPECT_EQ(none, i(""));
    EXPECT_EQ(none, i("; only a comment\n\n"));
    EXPECT_EQ(i("\xEF\xBB\xBF[s]\nk = v").get("s", "k"), "v");
    EXPECT_EQ(i("[s]").sections()[0].members.size(), 0u);
    // configparser's white space: carriage returns, form feeds, vertical tabs and 0x1C to 0x1F at a line's ends too
    EXPECT_EQ(i("[s]\na = \r \x1f\nb =\f1\v\n").get("s", "a"), "");
    EXPECT_EQ(i("[s]\na = \r \nb =\f1\v\n").get("s", "b"), "1");
    ini a = i("[s]\nk = v");
    ini b = std::move(a);
    EXPECT_EQ(b.get("s", "k"), "v");
    // every prefix of a document fails or reads, never faults
    std::string text = "top = 1\n[server]\nhost = x\nmulti = a\n    b\n; c\n[t] ; d\nk:v\n";
    for (size_t n = 0; n <= text.size(); ++n) {
        (void)ini::parse(sgcl::string(text.substr(0, n)));
    }
    std::string doc = "[a]\nx = 1\n";
    for (size_t piece : {1, 3, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(doc, piece));
        auto r = ini::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ(r->get("a", "x"), "1");
    }
    sgcl::io::reader bad(make_tracked<failing>("[a]"));
    EXPECT_EQ(ini::parse(bad).error().code(), errc::io);
    auto task = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 2));
        auto r = co_await ini::async_parse(in);
        co_return r && r->get("a", "x") == "1" ? 1 : -1;
    }(doc));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}
