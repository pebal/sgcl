//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// toml: TOML v1.0.0. Against Python's tomllib (tools/toml_oracle.py writes
// toml_tests.h): the value of the spec's examples, of the edge cases of every
// construct, valid and not, and of 400 documents the writer here wrote of
// random values, read here as tomllib reads them. Then the typed dates and
// times, the errors and their places, the limits, the writer's round trip,
// and the boundaries of every member.
#include "common.h"
#include "toml_tests.h"

#include <cstring>
#include <random>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string quoted(std::string_view s) {
        std::string out = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += char(c);
            } else if (c >= 0x20 && c < 0x7F) {
                out += char(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            }
        }
        return out + "\"";
    }

    std::string two(int v, int width = 2) {
        std::string s = std::to_string(v);
        return std::string(size_t(width) > s.size() ? size_t(width) - s.size() : 0, '0') + s;
    }

    // The value in the oracle's form (tools/toml_oracle.py)
    void dump(const toml& t, std::string& out) {
        switch (t.type()) {
            case toml::kind::string: out += "s" + quoted(t.as_string()->view()); return;
            case toml::kind::integer: out += "i" + std::to_string(*t.as_int()); return;
            case toml::kind::boolean: out += *t.as_bool() ? "b1" : "b0"; return;
            case toml::kind::floating: {
                double d = *t.as_double();
                if (std::isnan(d)) {
                    out += "f-nan";
                } else {
                    char b[24];
                    std::snprintf(b, sizeof b, "f%016llx", (unsigned long long)std::bit_cast<uint64_t>(d));
                    out += b;
                }
                return;
            }
            case toml::kind::offset_datetime: {
                // from the date, the time and the offset: as_datetime has no instant past the years 1677 to 2262
                std::string text(t.text().view());
                int offset = 0;
                if ((text.back() | 32) != 'z') {
                    std::string o = text.substr(text.size() - 6);
                    offset = (o[0] == '-' ? -1 : 1) * (std::stoi(o.substr(1, 2)) * 3600 + std::stoi(o.substr(4, 2)) * 60);
                }
                int64_t days = std::chrono::sys_days(*t.as_date()).time_since_epoch().count();
                int64_t ns = t.as_time()->nanoseconds();
                out += "O" + std::to_string(days * 86400 + ns / 1000000000 - offset) + "." + two(int(ns % 1000000000 / 1000), 6);
                if (auto v = t.as_datetime()) {
                    EXPECT_EQ(v->unix(), days * 86400 + std::min<int64_t>(ns / 1000000000, 86399) - offset) << text;
                }
                return;
            }
            case toml::kind::local_datetime:
            case toml::kind::local_date: {
                auto d = *t.as_date();
                out += (t.type() == toml::kind::local_date ? "D" : "L") + two(d.year(), 4) + "-" + two(int(d.month())) + "-" + two(d.day());
                if (t.type() == toml::kind::local_date) {
                    return;
                }
                out += "T";
                [[fallthrough]];
            }
            case toml::kind::local_time: {
                if (t.type() == toml::kind::local_time) {
                    out += "T";
                }
                int64_t ns = t.as_time()->nanoseconds();
                int64_t s = ns / 1000000000;
                out += two(int(s / 3600)) + ":" + two(int(s / 60 % 60)) + ":" + two(int(s % 60)) + "." + two(int(ns % 1000000000 / 1000), 6);
                return;
            }
            case toml::kind::array:
                out += "[ ";
                for (auto& e : t.elements()) {
                    dump(e, out);
                    out += " ";
                }
                out += "]";
                return;
            case toml::kind::table: {
                std::vector<const toml::member*> ms;
                for (auto& m : t.members()) {
                    ms.push_back(&m);
                }
                std::sort(ms.begin(), ms.end(), [](auto a, auto b) { return a->key.view() < b->key.view(); });
                out += "{ ";
                for (auto m : ms) {
                    out += quoted(m->key.view()) + "=";
                    dump(m->value, out);
                    out += " ";
                }
                out += "}";
                return;
            }
        }
    }

    std::string value_of(std::string_view text) {
        auto v = toml::parse(sgcl::string(text));
        if (!v) {
            return "error";
        }
        std::string out;
        dump(*v, out);
        return out;
    }

    toml t(const char* text) {
        auto v = toml::parse(sgcl::string(text));
        if (!v) {
            throw std::runtime_error(std::string("bad test TOML: ") + text + ": " + std::string(v.error().message().view()));
        }
        return *v;
    }

    toml random_value(std::mt19937_64& r, int depth, bool table_only) {
        static const char* strings[] = {"plain", "", "a b", "a.b", "\"", "'", "\\", "\n", "\x01", "日本", "[x]", "1979-05-27", "=", "#"};
        auto pick = [&] { return sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]); };
        int k = table_only ? 9 : int(r() % (depth > 3 ? 8 : 11));
        switch (k) {
            case 0: return toml(bool(r() & 1));
            case 1: return toml(int64_t(r()) >> (r() % 64));
            case 2: return toml(std::bit_cast<double>(r()));
            case 3:
            case 4: return toml(pick());
            case 5: {
                auto z = sgcl::time::zone::fixed(sgcl::duration(std::chrono::minutes((int64_t(r() % 97) - 48) * 15)));
                return toml(sgcl::time::datetime::from_unix_nano(int64_t(r() % 400000000000000000ull) - 100000000000000000, z));
            }
            case 6: return toml(sgcl::time::date(int(r() % 10000), int(1 + r() % 12), int(1 + r() % 28)));
            case 7: {
                auto d = sgcl::duration(std::chrono::nanoseconds(int64_t(r() % 86400000000000ull)));
                return r() & 1 ? toml::local_time(d) : toml::local_datetime(sgcl::time::date(int(r() % 10000), 1 + int(r() % 12), 1 + int(r() % 28)), d);
            }
            case 8: {
                sgcl::vector<toml> items;
                bool tables = r() % 3 == 0;
                for (int i = int(r() % 4); i > 0; --i) {
                    items.push_back(random_value(r, depth + 1, tables));
                }
                return toml::array(items);
            }
            default: {
                toml table;
                for (int i = int(r() % 5); i > 0; --i) {
                    table = table.set(pick(), random_value(r, depth + 1, false));
                }
                return table;
            }
        }
    }
}

TEST(Toml_Tests, AsTomllibReadsThem) {
    // where TOML 1.0 and tomllib differ, by name: what the reading here gives
    std::map<std::string_view, std::string> differs = {};
    size_t compared = 0, refused = 0;
    for (auto& c : toml_oracle::reads) {
        auto it = differs.find(c.name);
        std::string want = it == differs.end() ? std::string(c.value) : it->second;
        EXPECT_EQ(value_of(c.text), want) << c.name << "\n" << c.text;
        ++compared;
        refused += want == "error";
    }
    EXPECT_GT(compared, 570u);
    EXPECT_GT(refused, 100u);
}

TEST(Toml_Tests, Kinds) {
    toml v = t("s = 'x'\ni = 0xff\nf = 1e2\nb = true\nodt = 1979-05-27T07:32:00.5-08:00\nldt = 1979-05-27 07:32:00\n"
               "ld = 1979-05-27\nlt = 07:32:00.000000001\na = [1, 'two']\nt = {}");
    EXPECT_EQ(v["s"].type(), toml::kind::string);
    EXPECT_EQ(v["i"].type(), toml::kind::integer);
    EXPECT_EQ(v["f"].type(), toml::kind::floating);
    EXPECT_EQ(v["b"].type(), toml::kind::boolean);
    EXPECT_EQ(v["odt"].type(), toml::kind::offset_datetime);
    EXPECT_EQ(v["ldt"].type(), toml::kind::local_datetime);
    EXPECT_EQ(v["ld"].type(), toml::kind::local_date);
    EXPECT_EQ(v["lt"].type(), toml::kind::local_time);
    EXPECT_EQ(v["a"].type(), toml::kind::array);
    EXPECT_EQ(v["t"].type(), toml::kind::table);
    EXPECT_TRUE(v["s"].is_string());
    EXPECT_TRUE(v["i"].is_integer() && v["i"].is_number() && !v["i"].is_floating());
    EXPECT_TRUE(v["f"].is_floating() && v["f"].is_number());
    EXPECT_TRUE(v["b"].is_bool());
    for (const char* k : {"odt", "ldt", "ld", "lt"}) {
        EXPECT_TRUE(v[sgcl::string(k)].is_datetime()) << k;
    }
    EXPECT_FALSE(v["s"].is_datetime());
    EXPECT_TRUE(v["a"].is_array());
    EXPECT_TRUE(v["t"].is_table());
    // the text as written
    EXPECT_EQ(v["i"].text(), "0xff");
    EXPECT_EQ(v["i"].as_int(), 255);
    EXPECT_EQ(v["f"].text(), "1e2");
    EXPECT_EQ(v["f"].as_double(), 100.0);
    EXPECT_EQ(v["i"].as_double(), 255.0);
    EXPECT_EQ(v["odt"].text(), "1979-05-27T07:32:00.5-08:00");
    EXPECT_EQ(v["a"].text(), "");
    // dates and times through sgcl::time
    auto odt = *v["odt"].as_datetime();
    EXPECT_EQ(odt.unix(), 296667120);
    EXPECT_EQ(odt.nanosecond(), 500000000);
    EXPECT_EQ(odt.offset(), sgcl::duration(std::chrono::hours(-8)));
    EXPECT_EQ(odt.hour(), 7);
    EXPECT_EQ(v["odt"].as_datetime(sgcl::time::zone::utc())->hour(), 15);
    EXPECT_EQ(v["odt"].as_date(), sgcl::time::date(1979, 5, 27));
    EXPECT_EQ(v["odt"].as_time(), sgcl::duration(std::chrono::milliseconds(27120500)));
    EXPECT_FALSE(v["ldt"].as_datetime());
    auto warsaw = sgcl::time::zone::fixed(sgcl::duration(std::chrono::hours(2)));
    EXPECT_EQ(v["ldt"].as_datetime(warsaw)->unix(), 296667120 - 10 * 3600);
    EXPECT_EQ(v["ldt"].as_datetime(warsaw)->hour(), 7);
    EXPECT_EQ(v["ld"].as_datetime(warsaw)->hour(), 0);
    EXPECT_EQ(v["ld"].as_date(), sgcl::time::date(1979, 5, 27));
    EXPECT_FALSE(v["ld"].as_time());
    EXPECT_FALSE(v["lt"].as_date());
    EXPECT_FALSE(v["lt"].as_datetime(warsaw));
    EXPECT_EQ(v["lt"].as_time(), sgcl::duration(std::chrono::nanoseconds(27120000000001)));
    EXPECT_EQ(v["ldt"].as_time(), sgcl::duration(std::chrono::seconds(27120)));
    // a fraction past the nanosecond is cut, its text kept
    toml fine = t("a = 07:32:00.1234567899\nb = 1979-05-27T07:32:00.9999999999Z");
    EXPECT_EQ(fine["a"].as_time()->nanoseconds() % 1000000000, 123456789);
    EXPECT_EQ(fine["b"].as_datetime()->nanosecond(), 999999999);
    EXPECT_EQ(fine["a"].text(), "07:32:00.1234567899");
    // outside time::datetime's years (1677 to 2262) no datetime, the date and the time all the same
    toml far = t("a = 9999-12-31T23:59:59Z\nb = 1000-01-01T00:00:00\nc = 0000-01-01T00:00:00+01:00");
    EXPECT_FALSE(far["a"].as_datetime());
    EXPECT_FALSE(far["a"].as_datetime(warsaw));
    EXPECT_FALSE(far["b"].as_datetime(warsaw));
    EXPECT_EQ(far["a"].as_date(), sgcl::time::date(9999, 12, 31));
    EXPECT_EQ(far["c"].as_date(), sgcl::time::date(0, 1, 1));
    EXPECT_EQ(far["b"].as_time(), sgcl::duration());
    EXPECT_EQ(far["a"], t("a = 9999-12-31T22:59:59-01:00")["a"]);   // the same instant
    EXPECT_EQ(far["a"].hash(), t("a = 9999-12-31T22:59:59-01:00")["a"].hash());
    // a leap second as RFC 3339 writes it reads as the second before
    toml leap = t("a = 2016-12-31T23:59:60Z");
    EXPECT_EQ(leap["a"].as_datetime()->unix(), 1483228799);
    EXPECT_EQ(leap["a"].as_time(), sgcl::duration(std::chrono::seconds(86400)));
    // made here
    EXPECT_EQ(toml(sgcl::time::datetime::from_unix(296667120, sgcl::time::zone::fixed(sgcl::duration(std::chrono::minutes(-480))))).text(), "1979-05-27T07:32:00-08:00");
    EXPECT_EQ(toml(sgcl::time::datetime::from_unix_nano(1500000000, sgcl::time::zone::utc())).text(), "1970-01-01T00:00:01.5Z");
    EXPECT_EQ(toml(sgcl::time::date(1979, 5, 27)).text(), "1979-05-27");
    EXPECT_EQ(toml::local_time(sgcl::duration(std::chrono::microseconds(27120000001))).text(), "07:32:00.000001");
    EXPECT_EQ(toml::local_datetime(sgcl::time::date(1, 1, 1), sgcl::duration()).text(), "0001-01-01T00:00:00");
    EXPECT_THROW(toml::local_time(sgcl::duration(std::chrono::hours(24))), sgcl::invalid_argument);
    EXPECT_THROW(toml::local_time(sgcl::duration(std::chrono::nanoseconds(-1))), sgcl::invalid_argument);
    EXPECT_THROW(toml::local_datetime(sgcl::time::date(1, 1, 1), sgcl::duration(std::chrono::hours(25))), sgcl::invalid_argument);
    EXPECT_EQ(toml(1.0).text(), "1.0");
    EXPECT_EQ(toml(1e300).text(), "1e+300");
    EXPECT_EQ(toml(-0.0).text(), "-0.0");
    EXPECT_EQ(toml(INFINITY).text(), "inf");
    EXPECT_EQ(toml(-INFINITY).text(), "-inf");
    EXPECT_EQ(toml(NAN).text(), "nan");
    EXPECT_EQ(toml(INT64_MIN).as_int(), INT64_MIN);
    EXPECT_EQ(toml(true).text(), "true");
    EXPECT_EQ(toml("x").as_string(), "x");
}

TEST(Toml_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"a = 1\na = 2", errc::duplicate_key, 2, 1},
        {"[t]\n[t]", errc::duplicate_key, 2, 1},
        {"a = {x = 1}\na.y = 2", errc::duplicate_key, 2, 1},
        {"a = [1, 2", errc::unexpected_end, 1, 10},
        {"a = \"x", errc::unexpected_end, 1, 5},
        {"a = 'x\nb'", errc::syntax, 1, 7},
        {"a = \"\\q\"", errc::invalid_escape, 1, 6},
        {"a = \"\\uD800\"", errc::invalid_escape, 1, 6},
        {"a = 012", errc::syntax, 1, 5},
        {"a = 9223372036854775808", errc::out_of_range, 1, 5},
        {"a = -9223372036854775809", errc::out_of_range, 1, 5},
        {"a = 0x8000000000000000", errc::out_of_range, 1, 5},
        {"a = 1979-02-30", errc::syntax, 1, 5},
        {"a = 1 b = 2", errc::syntax, 1, 7},
        {"a = { x = 1,\n y = 2 }", errc::syntax, 1, 13},
        {"a = 1\n\n  = 2", errc::syntax, 3, 3},
        {"a = \"\x01\"", errc::invalid_character, 1, 6},
        {"# \x7f", errc::invalid_character, 1, 3},
        {"a = 1\n[[a]]", errc::syntax, 2, 1},
        {"a = 'x' \xff", errc::invalid_utf8, 1, 9},
    };
    for (auto& c : cases) {
        auto r = toml::parse(sgcl::string(c.text));
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text;
        EXPECT_EQ(r.error().column(), c.column) << c.text;
    }
}

TEST(Toml_Tests, Depth) {
    std::string deep = "a = " + std::string(600, '[') + std::string(600, ']');
    auto r = toml::parse(sgcl::string(deep));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::depth_limit);
    // as deep as the limit takes, read and written back
    std::string fits = "a = " + std::string(512, '[') + "1" + std::string(512, ']');
    auto f = toml::parse(sgcl::string(fits));
    ASSERT_TRUE(f);
    EXPECT_EQ(*toml::parse(f->to_string()), *f);
    std::string tables = std::string(1019, 'a');   // 510 tables, a.a.a…
    for (size_t i = 1; i < tables.size(); i += 2) {
        tables[i] = '.';
    }
    auto deep_tables = toml::parse(sgcl::string("[" + tables + "]\nx = [[1]]"));
    ASSERT_TRUE(deep_tables);
    EXPECT_EQ(*toml::parse(deep_tables->to_string()), *deep_tables);
    EXPECT_FALSE(toml::parse(sgcl::string("[" + tables + "]\nx = [[[1]]]")));
    toml::options o;
    o.max_depth = 4;
    // four tables and arrays inside one another below the root
    EXPECT_TRUE(toml::parse("a = [[[[1]]]]", o));
    EXPECT_FALSE(toml::parse("a = [[[[[1]]]]]", o));
    EXPECT_TRUE(toml::parse("a.b.c.d.e = 1", o));
    EXPECT_FALSE(toml::parse("a.b.c.d.e.f = 1", o));
    EXPECT_TRUE(toml::parse("[a.b.c.d]", o));
    EXPECT_FALSE(toml::parse("[a.b.c.d.e]", o));
    EXPECT_FALSE(toml::parse("[[a.b.c.d]]", o));
    EXPECT_TRUE(toml::parse("a = {b = {c = {d = {e = 1}}}}", o));
    EXPECT_FALSE(toml::parse("a = {b = {c = {d = {e = {}}}}}", o));
    // deep values written: up to 512 levels, past it invalid_argument
    toml v = toml::array({1});
    for (int i = 0; i < 600; ++i) {
        v = toml::array({v});
    }
    EXPECT_THROW(toml::table({{"a", v}}).to_string(), sgcl::invalid_argument);
    // a table of many keys reads in time
    std::string many;
    for (int i = 0; i < 100000; ++i) {
        many += "k" + std::to_string(i) + " = " + std::to_string(i) + "\n";
    }
    auto m = toml::parse(sgcl::string(many));
    ASSERT_TRUE(m);
    EXPECT_EQ(m->size(), 100000u);
    EXPECT_EQ((*m)["k99999"].as_int(), 99999);
}

TEST(Toml_Tests, WrittenReadsBack) {
    std::mt19937_64 r(7);
    for (int i = 0; i < 2000; ++i) {
        toml v = random_value(r, 0, true);
        sgcl::string text = v.to_string();
        auto back = toml::parse(text);
        ASSERT_TRUE(back) << i << "\n" << text << "\n" << back.error().message();
        EXPECT_EQ(*back, v) << i << "\n" << text;
        EXPECT_EQ(back->hash(), v.hash()) << i;
        EXPECT_EQ(back->to_string(), text) << i;
    }
}

TEST(Toml_Tests, Writing) {
    toml v = toml::table({
        {"title", "TOML"},
        {"n", 1},
        {"owner", toml::table({{"name", "Tom"}, {"dob", t("d = 1979-05-27T07:32:00-08:00")["d"]}})},
        {"empty", toml()},
        {"servers", toml::table({{"alpha", toml::table({{"ip", "10.0.0.1"}})}})},
        {"products", toml::array({toml::table({{"name", "Hammer"}}), toml(), toml::table({{"name", "Nail"}})})},
        {"points", toml::array({toml::table({{"x", 1}}), 2})},
        {"a b", toml::array({})},
        {"", "\"q\"\n\x01\x7f"},
    });
    EXPECT_EQ(std::string(v.to_string().view()),
              "title = \"TOML\"\n"
              "n = 1\n"
              "points = [{ x = 1 }, 2]\n"
              "\"a b\" = []\n"
              "\"\" = \"\\\"q\\\"\\n\\u0001\\u007F\"\n"
              "\n[owner]\nname = \"Tom\"\ndob = 1979-05-27T07:32:00-08:00\n"
              "\n[empty]\n"
              "\n[servers.alpha]\nip = \"10.0.0.1\"\n"
              "\n[[products]]\nname = \"Hammer\"\n"
              "\n[[products]]\n"
              "\n[[products]]\nname = \"Nail\"\n");
    EXPECT_EQ(toml().to_string(), "");
    EXPECT_THROW(toml(1).to_string(), sgcl::invalid_argument);
    EXPECT_THROW(toml::array({}).to_string(), sgcl::invalid_argument);
    // what was read keeps its text
    EXPECT_EQ(t("a = 0xFF\nb = 1_000\nc = +inf\nd = 1979-05-27 07:32:00Z").to_string(), "a = 0xFF\nb = 1_000\nc = +inf\nd = 1979-05-27 07:32:00Z\n");
}

TEST(Toml_Tests, Members) {
    toml m = t("a = 1\nb = ['x', 'y']\n[c]\nd = true");
    EXPECT_TRUE(m.is_table());
    EXPECT_EQ(m.size(), 3u);
    EXPECT_EQ(m["a"].as_int(), 1);
    EXPECT_EQ(m[sgcl::string("b")][1].as_string(), "y");
    EXPECT_EQ(m["c"]["d"].as_bool(), true);
    EXPECT_TRUE(m["missing"].is_table());
    EXPECT_TRUE(m["missing"].empty());
    EXPECT_TRUE(m["a"]["x"].empty());
    EXPECT_TRUE(m.contains("a"));
    EXPECT_FALSE(m.contains("z"));
    EXPECT_FALSE(m["a"].contains("a"));
    EXPECT_TRUE(m["b"][5].empty());
    EXPECT_TRUE(m["b"][-1].empty());
    EXPECT_TRUE(m[0].empty());
    EXPECT_EQ(m.members().size(), 3u);
    EXPECT_EQ(m["b"].elements().size(), 2u);
    EXPECT_TRUE(m["a"].elements().empty());
    EXPECT_TRUE(m["b"].members().empty());
    EXPECT_EQ(m["a"].size(), 0u);
    EXPECT_EQ(m["a"].as_int(9), 1);
    EXPECT_EQ(m["b"].as_int(9), 9);
    EXPECT_EQ(m["a"].as_string("?"), "?");
    EXPECT_EQ(m["a"].as_double(0), 1.0);
    EXPECT_EQ(m["a"].as_bool(true), true);
    EXPECT_FALSE(m["a"].as_bool());
    EXPECT_FALSE(m["a"].as_string());
    EXPECT_FALSE(m["b"].as_double());
    EXPECT_FALSE(m["a"].as_datetime());
    EXPECT_FALSE(m["a"].as_date());
    EXPECT_FALSE(m["a"].as_time());
    // new versions
    EXPECT_EQ(m.set("a", 2)["a"].as_int(), 2);
    EXPECT_EQ(m.set("e", 3).size(), 4u);
    EXPECT_EQ(m.erase("a").size(), 2u);
    EXPECT_EQ(m.erase("zz").size(), 3u);
    EXPECT_EQ(m["b"].push_back("z").size(), 3u);
    EXPECT_EQ(m["b"].set(0, "w")[0].as_string(), "w");
    EXPECT_EQ(m["b"].set(2, "w").size(), 3u);
    EXPECT_EQ(m["b"].set(3, "w").size(), 2u);   // past the end: unchanged
    EXPECT_EQ(m.push_back(1).size(), 3u);         // a table: unchanged
    EXPECT_EQ(m["b"].set("k", 1).size(), 2u);     // an array: unchanged
    EXPECT_EQ(m.size(), 3u);
    // equality by the value read
    EXPECT_EQ(t("a = 0x10"), t("a = 16"));
    EXPECT_EQ(t("a = 1e1"), t("a = 10.0"));
    EXPECT_NE(t("a = 1"), t("a = 1.0"));
    EXPECT_EQ(t("a = nan"), t("a = -nan"));
    EXPECT_EQ(t("a = 0.0"), t("a = -0.0"));
    EXPECT_EQ(t("a = 1979-05-27T07:32:00-08:00"), t("a = 1979-05-27T15:32:00Z"));
    EXPECT_EQ(t("a = 1979-05-27T07:32:00"), t("a = 1979-05-27 07:32:00.000"));
    EXPECT_NE(t("a = 1979-05-27T07:32:00"), t("a = 1979-05-27T07:32:00Z"));
    EXPECT_NE(t("a = 07:32:00"), t("a = 07:32:01"));
    EXPECT_EQ(t("a = 1\nb = 2"), t("b = 2\na = 1"));
    EXPECT_EQ(t("a = 1\nb = 2").hash(), t("b = 2\na = 1").hash());
    EXPECT_EQ(t("a = 0x10").hash(), t("a = 16").hash());
    EXPECT_EQ(t("a = 1979-05-27T07:32:00-08:00").hash(), t("a = 1979-05-27T15:32:00Z").hash());
    EXPECT_NE(t("a = 'x'"), t("a = 'y'"));
    EXPECT_NE(t("a = [1]"), t("a = [1, 2]"));
    EXPECT_EQ(std::hash<toml>()(m), m.hash());
}

TEST(Toml_Tests, Json) {
    toml v = t("a = [1, 2.5, true, 'x', 1979-05-27, nan]\nb.c = 1979-05-27T07:32:00Z");
    EXPECT_EQ(std::string(v.to_json().to_string().view()), R"({"a":[1,2.5,true,"x","1979-05-27",null],"b":{"c":"1979-05-27T07:32:00Z"}})");
    auto j = json::parse(R"({"n":9007199254740993,"f":0.1,"s":"t","l":[{},null,1],"z":null,"o":{"p":null}})").value();
    toml from = toml::from_json(j);
    EXPECT_EQ(from["n"].as_int(), 9007199254740993);
    EXPECT_EQ(from["f"].as_double(), 0.1);
    EXPECT_EQ(from["l"].size(), 2u);
    EXPECT_FALSE(from.contains("z"));
    EXPECT_TRUE(from["o"].empty());
    EXPECT_EQ(std::string(from.to_string().view()), "n = 9007199254740993\nf = 0.1\ns = \"t\"\nl = [{}, 1]\n\n[o]\n");
    EXPECT_TRUE(toml::from_json(json()).empty());
    EXPECT_EQ(toml::from_json(json(3)).as_int(), 3);
}

TEST(Toml_Tests, Boundaries) {
    toml none;
    EXPECT_TRUE(none.is_table());
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.text(), "");
    EXPECT_EQ(none, toml::table({}));
    EXPECT_EQ(none, t(""));
    EXPECT_EQ(none, t("# only a comment\n\n"));
    EXPECT_EQ(none.size(), 0u);
    EXPECT_FALSE(none.as_bool());
    EXPECT_FALSE(none.as_int());
    EXPECT_EQ(none.to_json(), json::object({}));
    EXPECT_NE(none, toml::array({}));
    // a moved value is a copy of the handle
    toml a = t("a = [1]");
    toml b = std::move(a);
    EXPECT_EQ(b["a"].size(), 1u);
    // a table built from members, an array from a vector
    sgcl::vector<toml> items;
    items.push_back(1);
    items.push_back("x");
    EXPECT_EQ(toml::array(items).size(), 2u);
    sgcl::vector<toml::member> ms;
    ms.push_back({"k", 1});
    EXPECT_EQ(toml::table(ms)["k"].as_int(), 1);
    // every prefix of a document fails or reads, never faults
    std::string text = "a = [1, {b = 'c'}]\n[d]\ne = \"\"\"\nf\\\n  g\"\"\"\n[[h]]\ni = 1979-05-27T07:32:00Z";
    for (size_t n = 0; n <= text.size(); ++n) {
        (void)toml::parse(sgcl::string(text.substr(0, n)));
    }
    // a stream, to its end
    std::string doc = "a = 1\nb = [2, 3]\n";
    for (size_t piece : {1, 3, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(doc, piece));
        auto r = toml::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ((*r)["b"][1].as_int(), 3);
    }
    sgcl::io::reader bad(make_tracked<failing>("a = 1"));
    EXPECT_EQ(toml::parse(bad).error().code(), errc::io);
    auto task = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 2));
        auto r = co_await toml::async_parse(in);
        co_return r && (*r)["a"].as_int() == 1 ? 1 : -1;
    }(doc));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}
