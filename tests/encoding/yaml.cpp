//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// yaml: YAML 1.2.2 with the core schema. Against libyaml (tools/yaml_oracle.cpp
// writes yaml_tests.h): the graph of every document of the examples of the
// spec's chapter 2, of the edge cases of every construct, and of 600 documents
// the writer here wrote of random values, read here as libyaml reads them,
// each difference named with its reason. Then the core schema, the errors and
// their places, the billion laughs, the limits, the writer's round trip, and
// the boundaries of every member.
#include "common.h"
#include "yaml_tests.h"

#include <cstring>
#include <functional>
#include <map>
#include <random>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string escape(const std::string& s) {
        std::string out;
        for (unsigned char c : s) {
            if (c == '\\' || c == '|') {
                out += '\\';
                out += char(c);
            } else if (c == '\0') {
                out += "\\0";
            } else if (c == '\n') {
                out += "\\n";
            } else {
                out += char(c);
            }
        }
        return out;
    }

    // The graph in the oracle's form (tools/yaml_oracle.cpp)
    void graph(const yaml& y, std::string& out) {
        std::string tag(y.tag().view());
        if (!tag.empty()) {
            out += "<" + tag + ">";
        }
        if (y.is_sequence()) {
            out += "[ ";
            for (auto& e : y.elements()) {
                graph(e, out);
            }
            out += "] ";
            return;
        }
        if (y.is_mapping()) {
            out += "{ ";
            for (auto& m : y.members()) {
                graph(m.key, out);
                graph(m.value, out);
            }
            out += "} ";
            return;
        }
        out += "nbifs"[int(y.type())];
        out += "|" + escape(std::string(y.text().view())) + "| ";
    }

    std::string graph_of(std::string_view text) {
        auto docs = yaml::parse_all(sgcl::string(text));
        if (!docs) {
            return "error";
        }
        std::string out;
        for (auto& d : *docs) {
            graph(d, out);
            out += "\n";
        }
        return out;
    }

    yaml y(const char* text) {
        auto v = yaml::parse(sgcl::string(text));
        if (!v) {
            throw std::runtime_error(std::string("bad test YAML: ") + text + ": " + std::string(v.error().message().view()));
        }
        return *v;
    }
}

TEST(Yaml_Tests, AsLibyamlReadsThem) {
    // where YAML 1.2 and libyaml (1.1) differ, by name: what the reading here gives
    std::map<std::string_view, std::string> differs = {
        // 1.2 §7.4: in a flow collection ':' before a flow indicator is a value
        // indicator; libyaml refuses it
        {"plain with trailing colon in flow", "[ { s|a| n|| } s|b| ] \n"},
        // §3.2.1.1: the keys of a mapping are unique; *k is the key "key"
        // again, which libyaml does not check
        {"anchors on keys and aliases as keys", "error"},
    };
    size_t compared = 0;
    for (auto& c : yaml_oracle::reads) {
        auto it = differs.find(c.name);
        std::string want = it == differs.end() ? std::string(c.graph) : it->second;
        EXPECT_EQ(graph_of(c.text), want) << c.name << "\n" << c.text;
        ++compared;
    }
    EXPECT_GT(compared, 650u);
}

TEST(Yaml_Tests, CoreSchema) {
    struct {
        const char* text;
        yaml::kind kind;
    } cases[] = {
        {"", yaml::kind::null}, {"~", yaml::kind::null}, {"null", yaml::kind::null}, {"Null", yaml::kind::null}, {"NULL", yaml::kind::null},
        {"nULL", yaml::kind::string}, {"true", yaml::kind::boolean}, {"True", yaml::kind::boolean}, {"TRUE", yaml::kind::boolean},
        {"tRUE", yaml::kind::string}, {"yes", yaml::kind::string}, {"on", yaml::kind::string}, {"0", yaml::kind::integer},
        {"-0", yaml::kind::integer}, {"+12", yaml::kind::integer}, {"0o17", yaml::kind::integer}, {"0o8", yaml::kind::string},
        {"-0o7", yaml::kind::string}, {"0x1fF", yaml::kind::integer}, {"0X1F", yaml::kind::string}, {"0x", yaml::kind::string},
        {"1_000", yaml::kind::string}, {"0b1", yaml::kind::string}, {"08", yaml::kind::integer}, {"1.5", yaml::kind::floating},
        {".5", yaml::kind::floating}, {"5.", yaml::kind::floating}, {"-.5e-3", yaml::kind::floating}, {"1e3", yaml::kind::floating},
        {"1E+3", yaml::kind::floating}, {"1e", yaml::kind::string}, {".", yaml::kind::string}, {".e3", yaml::kind::string},
        {".inf", yaml::kind::floating}, {"+.Inf", yaml::kind::floating}, {"-.INF", yaml::kind::floating}, {".iNf", yaml::kind::string},
        {".nan", yaml::kind::floating}, {"-.nan", yaml::kind::string}, {"12:30", yaml::kind::string}, {"2002-12-14", yaml::kind::string},
    };
    for (auto& c : cases) {
        auto v = yaml::parse(sgcl::string(std::string("v: ") + c.text));
        ASSERT_TRUE(v) << c.text;
        EXPECT_EQ((*v)["v"].type(), c.kind) << c.text;
        EXPECT_EQ((*v)["v"].text(), sgcl::string(c.text)) << c.text;
    }
    yaml n = y("[0o17, 0x1F, -12, 18446744073709551615, 18446744073709551616, -9223372036854775808, 1.5e300, 1e400, -.inf, .nan, 1e-400]");
    EXPECT_EQ(n[0].as_int(), 15);
    EXPECT_EQ(n[1].as_int(), 31);
    EXPECT_EQ(n[2].as_int(), -12);
    EXPECT_FALSE(n[2].as_uint());
    EXPECT_EQ(n[3].as_uint(), UINT64_MAX);
    EXPECT_FALSE(n[3].as_int());
    EXPECT_TRUE(n[4].is_integer());
    EXPECT_FALSE(n[4].as_uint());
    EXPECT_EQ(n[4].as_double(), 18446744073709551616.0);
    EXPECT_EQ(n[5].as_int(), INT64_MIN);
    EXPECT_EQ(n[6].as_double(), 1.5e300);
    EXPECT_EQ(n[7].as_double(), INFINITY);
    EXPECT_EQ(n[8].as_double(), -INFINITY);
    EXPECT_TRUE(std::isnan(*n[9].as_double()));
    EXPECT_EQ(n[10].as_double(), 0.0);
    EXPECT_EQ(y("[True, FALSE]")[0].as_bool(), true);
    EXPECT_EQ(y("[True, FALSE]")[1].as_bool(), false);
    // quoted is a string whatever its text
    EXPECT_EQ(y("['1', \"true\", 'null']")[0].type(), yaml::kind::string);
    EXPECT_EQ(y("['1', \"true\", 'null']")[2].as_string(), "null");
    // the core tags force the kind, and are no tag of the node
    yaml t = y("[!!str 1, !!int '7', !!float 3, !!bool 'true', !!null '', ! 12, !!str]");
    EXPECT_EQ(t[0].as_string(), "1");
    EXPECT_EQ(t[1].as_int(), 7);
    EXPECT_EQ(t[2].as_double(), 3.0);
    EXPECT_EQ(t[3].as_bool(), true);
    EXPECT_TRUE(t[4].is_null());
    EXPECT_EQ(t[5].as_string(), "12");
    EXPECT_EQ(t[6].as_string(), "");
    for (const char* bad : {"!!int x", "!!float x", "!!bool 1", "!!null 0", "!!seq x", "!!map x", "!!str [1]", "!!map [1]"}) {
        auto r = yaml::parse(bad);
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), errc::type_mismatch) << bad;
    }
    // an application's tag is kept, the handles resolved
    yaml app = y("%TAG !e! tag:example.com,2026:\n---\n[!Ref x, !e!point {x: 1}, !<urn:a> 1, !!binary aGk=]");
    EXPECT_EQ(app[0].tag(), "!Ref");
    EXPECT_EQ(app[0].as_string(), "x");
    EXPECT_EQ(app[1].tag(), "tag:example.com,2026:point");
    EXPECT_TRUE(app[1].is_mapping());
    EXPECT_EQ(app[2].tag(), "urn:a");
    EXPECT_EQ(app[3].tag(), "tag:yaml.org,2002:binary");
    EXPECT_EQ(app[3].as_string(), "aGk=");
}

TEST(Yaml_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"a: b: c", errc::syntax, 1, 5},
        {"a: [1, 2", errc::syntax, 1, 9},
        {"a: 'unterminated", errc::unexpected_end, 1, 4},
        {"a: \"bad \\q escape\"", errc::invalid_escape, 1, 9},
        {"- a\n b: c", errc::syntax, 2, 3},
        {"a: 1\na: 2", errc::duplicate_key, 2, 1},
        {"{a: 1, a: 2}", errc::duplicate_key, 1, 8},
        {"*x", errc::syntax, 1, 1},
        {"a: &x 1\nb: *y", errc::syntax, 2, 4},
        {"\ta: 1", errc::syntax, 1, 1},
        {"a:\n\t- b", errc::syntax, 2, 1},
        {"%YAML 2.0\n--- x", errc::syntax, 1, 1},
        {"!x!y z", errc::syntax, 1, 1},
        {"]", errc::syntax, 1, 1},
        {"a: @b", errc::syntax, 1, 4},
        {"a: b\n- c", errc::syntax, 2, 1},
        {"--- a\n--- b\nextra: 1\n:", errc::syntax, 3, 6},
        {"key: |0\n  x", errc::syntax, 1, 7},
        {"'a\n--- b'", errc::syntax, 2, 1},
        {"[a, , b]", errc::syntax, 1, 5},
        {"\"\\uD800\"", errc::invalid_escape, 1, 4},
    };
    for (auto& c : cases) {
        auto r = yaml::parse_all(c.text);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().column(), c.column) << c.text << ": " << r.error().message();
    }
    // invalid UTF-8, a second document where one is asked for
    auto bad = yaml::parse(sgcl::string("a: \"\xff\""));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), errc::invalid_utf8);
    EXPECT_EQ(bad.error().column(), 5u);
    for (const char* bad : {"a: \x01", "a: \x7f", "a: \xc2\x82", "a: \xef\xbf\xbe", "\x00"}) {
        auto r = yaml::parse(sgcl::string(std::string_view(bad, std::strlen(bad) + (bad[0] == 0))));
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), errc::invalid_character) << bad;
    }
    EXPECT_TRUE(yaml::parse("a: \"\\x82\\uFFFE\\x01\"\nb: \xc2\x85x\tc"));   // escaped they are fine; NEL and a tab raw too
    auto two = yaml::parse("a\n---\nb");
    ASSERT_FALSE(two);
    EXPECT_EQ(two.error().message(), "2:1: a second document where one was expected");
    EXPECT_EQ(yaml::parse_all("a\n---\nb")->size(), 2u);
    // the duplicate keys allowed: the last wins
    yaml::options o;
    o.allow_duplicate_keys = true;
    EXPECT_EQ(yaml::parse("a: 1\na: 2", o)->operator[]("a").as_int(), 2);
}

TEST(Yaml_Tests, BillionLaughs) {
    std::string bomb = "a: &a [\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\",\"lol\"]\n";
    char prev = 'a';
    for (char c = 'b'; c <= 'i'; ++c) {
        bomb += std::string(1, c) + ": &" + c + " [";
        for (int k = 0; k < 9; ++k) {
            bomb += std::string(k ? "," : "") + "*" + prev;
        }
        bomb += "]\n";
        prev = c;
    }
    auto r = yaml::parse(sgcl::string(bomb));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::limit_exceeded);
    // a few levels are fine, and the anchor's node is shared, not copied
    yaml::options o;
    o.max_alias_nodes = 1000;
    auto small = yaml::parse("a: &a [1, 2, 3]\nb: [*a, *a]\nc: *a", o);
    ASSERT_TRUE(small);
    EXPECT_EQ((*small)["b"][0].elements().data(), (*small)["a"].elements().data());
    o.max_alias_nodes = 7;
    EXPECT_EQ(yaml::parse("a: &a [1, 2, 3]\nb: [*a, *a]\nc: *a", o).error().code(), errc::limit_exceeded);
}

TEST(Yaml_Tests, Depth) {
    yaml::options o;
    o.max_depth = 3;
    EXPECT_TRUE(yaml::parse("[[[1]]]", o));
    EXPECT_EQ(yaml::parse("[[[[1]]]]", o).error().code(), errc::depth_limit);
    EXPECT_EQ(yaml::parse("a:\n b:\n  c:\n   d: 1", o).error().code(), errc::depth_limit);
    std::string deep(100000, '[');
    deep += std::string(100000, ']');
    EXPECT_EQ(yaml::parse(sgcl::string(deep)).error().code(), errc::depth_limit);
    // deep values made by hand: written, compared, hashed without recursion
    yaml v = yaml(1);
    for (int i = 0; i < 20000; ++i) {
        v = yaml::sequence({v});
    }
    EXPECT_EQ(v, v);
    (void)v.hash();
    auto text = v.to_string();
    EXPECT_GT(text.size(), 20000u);
    (void)v.to_json();
}

TEST(Yaml_Tests, WrittenReadsBack) {
    std::mt19937_64 r(77);
    static const char* strings[] = {"plain", "", " lead", "trail ", "a: b", "#c", "- x", "true", "null", "12", "0o7", "multi\nline\n",
                                    "\ttab", "x\r\ny", "\x01", " \n i", "---", "...", "\xc2\x85", "\xef\xbb\xbf" "bom", "a\xe2\x80\xa8"
                                    "b", "日本"};
    std::function<yaml(int)> make = [&](int depth) -> yaml {
        switch (int(r() % (depth > 4 ? 6 : 8))) {
            case 0: return yaml();
            case 1: return yaml(bool(r() & 1));
            case 2: return yaml(int64_t(r()));
            case 3: return yaml(double(int64_t(r() % 1000)) / 8.0);
            case 4:
            case 5: return yaml(sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]));
            case 6: {
                sgcl::vector<yaml> items;
                for (int i = int(r() % 4); i > 0; --i) {
                    items.push_back(make(depth + 1));
                }
                return r() % 5 == 0 ? yaml::tagged("!t", yaml::sequence(items)) : yaml::sequence(items);
            }
            default: {
                sgcl::vector<yaml::member> ms;
                for (int i = int(r() % 4); i > 0; --i) {
                    yaml key = r() % 5 == 0 ? make(depth + 2) : yaml(sgcl::string(strings[r() % (sizeof strings / sizeof *strings)]));
                    bool dup = false;
                    for (auto& m : ms) {
                        dup = dup || m.key == key;
                    }
                    if (!dup) {
                        ms.push_back(yaml::member{key, r() % 7 == 0 ? yaml::tagged("!x", make(depth + 1)) : make(depth + 1)});
                    }
                }
                return yaml::mapping(ms);
            }
        }
    };
    for (int i = 0; i < 3000; ++i) {
        yaml v = make(0);
        auto text = v.to_string();
        auto back = yaml::parse(text);
        ASSERT_TRUE(back) << text.view() << ": " << back.error().message().view();
        ASSERT_EQ(*back, v) << text.view();
        EXPECT_EQ(back->to_string(), text);
    }
}

TEST(Yaml_Tests, Writing) {
    yaml v = yaml::mapping({{"name", "x"}, {"list", yaml::sequence({1, 2.5, true, yaml()})}, {"empty", yaml::sequence({})},
                            {"map", yaml::mapping({})}, {"text", "line 1\nline 2\n"}, {"quote", "a: b"}, {"tagged", yaml::tagged("!Ref", "id")},
                            {yaml::sequence({1, 2}), "complex"}, {"nested", yaml::mapping({{"k", yaml::sequence({yaml::mapping({{"a", 1}, {"b", 2}})})}})}});
    EXPECT_EQ(std::string(v.to_string().view()),
              "name: x\n"
              "list:\n"
              "  - 1\n"
              "  - 2.5\n"
              "  - true\n"
              "  - null\n"
              "empty: []\n"
              "map: {}\n"
              "text: |\n"
              "  line 1\n"
              "  line 2\n"
              "quote: \"a: b\"\n"
              "tagged: !Ref id\n"
              "? - 1\n"
              "  - 2\n"
              ": complex\n"
              "nested:\n"
              "  k:\n"
              "    - a: 1\n"
              "      b: 2\n");
    EXPECT_EQ(std::string(yaml("x").to_string().view()), "x\n");
    EXPECT_EQ(std::string(yaml().to_string().view()), "null\n");
    EXPECT_EQ(std::string(yaml::sequence({}).to_string().view()), "[]\n");
    EXPECT_EQ(std::string(yaml(0.1).to_string().view()), "0.1\n");
    EXPECT_EQ(std::string(yaml(1.0).to_string().view()), "1.0\n");
    EXPECT_EQ(std::string(yaml(1e300).to_string().view()), "1e+300\n");
    EXPECT_EQ(std::string(yaml(-INFINITY).to_string().view()), "-.inf\n");
    EXPECT_EQ(std::string(yaml(NAN).to_string().view()), ".nan\n");
    EXPECT_EQ(std::string(yaml("  lead\nx").to_string().view()), "|2-\n    lead\n  x\n");
    EXPECT_EQ(std::string(yaml("keep\n\n").to_string().view()), "|+\n  keep\n\n");
    // a key past 1024 characters is explicit (§7.4.2), the characters YAML
    // has only escaped are escaped
    std::string long_key(1500, 'k');
    yaml lk = yaml::mapping({{sgcl::string(long_key), 1}, {"\xc2\x82\xef\xbf\xbf", "\x01"}});
    auto lt = lk.to_string();
    EXPECT_EQ(lt.view().substr(0, 2), "? ");
    EXPECT_NE(lt.view().find("\"\\x82\\uFFFF\": \"\\x01\""), std::string_view::npos);
    EXPECT_EQ(yaml::parse(lt).value(), lk);
    // what is read keeps its text: 0o14 is written 0o14
    EXPECT_EQ(std::string(y("[0o14, 0x1F, 1e3, ~, True]").to_string().view()), "- 0o14\n- 0x1F\n- 1e3\n- ~\n- True\n");
}

TEST(Yaml_Tests, Members) {
    yaml m = y("a: 1\nb: [x, y]\n? [k]\n: complex\n1: one");
    EXPECT_TRUE(m.is_mapping());
    EXPECT_EQ(m.size(), 4u);
    EXPECT_EQ(m["a"].as_int(), 1);
    EXPECT_EQ(m[sgcl::string("b")][1].as_string(), "y");
    EXPECT_EQ(m[yaml::sequence({"k"})].as_string(), "complex");
    EXPECT_EQ(m[yaml(1)].as_string(), "one");
    EXPECT_TRUE(m["missing"].is_null());
    EXPECT_TRUE(m.contains("a"));
    EXPECT_FALSE(m.contains("z"));
    EXPECT_EQ(m["b"][5].type(), yaml::kind::null);
    EXPECT_EQ(m["b"][-1].type(), yaml::kind::null);
    EXPECT_EQ(m.members().size(), 4u);
    EXPECT_EQ(m["b"].elements().size(), 2u);
    EXPECT_TRUE(m["a"].elements().empty());
    EXPECT_EQ(m["a"].as_int(9), 1);
    EXPECT_EQ(m["b"].as_int(9), 9);
    EXPECT_EQ(m["a"].as_string("?"), "?");
    EXPECT_EQ(m["a"].as_double(0), 1.0);
    EXPECT_EQ(m["a"].as_bool(true), true);
    EXPECT_EQ(m["a"].as_uint(5u), 1u);
    // new versions
    EXPECT_EQ(m.set("a", 2)["a"].as_int(), 2);
    EXPECT_EQ(m.set("c", 3).size(), 5u);
    EXPECT_EQ(m.erase("a").size(), 3u);
    EXPECT_EQ(m["b"].push_back("z").size(), 3u);
    EXPECT_EQ(m["b"].set(0, "w")[0].as_string(), "w");
    EXPECT_EQ(m["b"].erase(0)[0].as_string(), "y");
    EXPECT_EQ(yaml().push_back(1).size(), 1u);
    EXPECT_EQ(m.size(), 4u);   // unchanged
    // tags survive the new versions
    yaml t = yaml::tagged("!T", yaml::mapping({{"a", 1}}));
    EXPECT_EQ(t.set("b", 2).tag(), "!T");
    EXPECT_EQ(yaml::tagged("!U", t).tag(), "!U");
    // equality by the value read
    EXPECT_EQ(y("0x10"), y("16"));
    EXPECT_EQ(y("1e1"), y("10.0"));
    EXPECT_NE(y("1"), y("1.0"));
    EXPECT_EQ(y(".nan"), y(".NaN"));
    EXPECT_EQ(y("{a: 1, b: 2}"), y("{b: 2, a: 1}"));
    EXPECT_EQ(y("{a: 1, b: 2}").hash(), y("{b: 2, a: 1}").hash());
    EXPECT_EQ(y("0x10").hash(), y("16").hash());
    EXPECT_NE(y("!a x"), y("x"));
    EXPECT_EQ(std::hash<yaml>()(m), m.hash());
}

TEST(Yaml_Tests, Json) {
    yaml v = y("a: [1, 2.5, true, ~, x]\n? [k]\n: c\n.inf: .inf");
    EXPECT_EQ(std::string(v.to_json().to_string().view()), R"({"a":[1,2.5,true,null,"x"],"[k]":"c",".inf":null})");
    auto j = json::parse(R"({"n":18446744073709551615,"f":0.1,"s":"t","l":[{}],"z":null})").value();
    yaml from = yaml::from_json(j);
    EXPECT_EQ(from["n"].as_uint(), UINT64_MAX);
    EXPECT_EQ(from["f"].as_double(), 0.1);
    EXPECT_EQ(from.to_json(), j);
    EXPECT_EQ(std::string(from.to_string().view()), "n: 18446744073709551615\nf: 0.1\ns: t\nl:\n  - {}\nz: null\n");
}

TEST(Yaml_Tests, Boundaries) {
    yaml none;
    EXPECT_TRUE(none.is_null());
    EXPECT_EQ(none.text(), "");
    EXPECT_TRUE(yaml::parse("%FOO bar\n--- x"));   // a reserved directive is passed over
    EXPECT_EQ(none.tag(), "");
    EXPECT_EQ(none, yaml(nullptr));
    EXPECT_EQ(none, y("~"));
    EXPECT_EQ(none.size(), 0u);
    EXPECT_FALSE(none.as_bool());
    EXPECT_FALSE(none.as_string());
    // empty texts, comments alone, a document marker alone
    EXPECT_TRUE(yaml::parse("")->is_null());
    EXPECT_TRUE(yaml::parse("# only a comment\n")->is_null());
    EXPECT_EQ(yaml::parse_all("")->size(), 0u);
    EXPECT_EQ(yaml::parse_all("---")->size(), 1u);
    EXPECT_EQ(yaml::parse_all("--- \n...\n--- x")->size(), 2u);
    EXPECT_EQ(yaml::parse("\xef\xbb\xbf" "a: 1")->operator[]("a").as_int(), 1);
    // a moved value is a copy of the handle
    yaml a = y("[1]");
    yaml b = std::move(a);
    EXPECT_EQ(b.size(), 1u);
    // every prefix of a document fails or reads, never faults
    std::string text = "a: [1, {b: 'c'}]\nd: |\n  e\nf: \"g\\n\"\n- h";
    for (size_t n = 0; n <= text.size(); ++n) {
        (void)yaml::parse_all(sgcl::string(text.substr(0, n)));
    }
    // a stream, to its end
    std::string doc = "a: 1\nb: [2, 3]\n";
    for (size_t piece : {1, 3, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(doc, piece));
        auto r = yaml::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ((*r)["b"][1].as_int(), 3);
    }
    sgcl::io::reader bad(make_tracked<failing>("a: 1"));
    EXPECT_EQ(yaml::parse(bad).error().code(), errc::io);
    auto t = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 2));
        auto r = co_await yaml::async_parse(in);
        co_return r && (*r)["a"].as_int() == 1 ? 1 : -1;
    }(doc));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}
