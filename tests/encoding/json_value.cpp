//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The value of JSON (E3): making one, reading it, new versions, JSON
// Pointer, the builder, equality and hash, the text, the collector.
#include "json_common.h"

#include <atomic>
#include <cmath>
#include <thread>

using namespace json_test;
using sgcl::encoding::errc;

namespace {
    json parsed(std::string_view s) {
        auto r = json::parse(text(s));
        EXPECT_TRUE(r) << s << (r ? "" : r.error().message().c_str());
        return r ? *r : json();
    }
}

TEST(JsonValue_Tests, Kinds) {
    EXPECT_EQ(sizeof(json), 24u);
    EXPECT_TRUE(json().is_null());
    EXPECT_TRUE(json(nullptr).is_null());
    EXPECT_EQ(json(true).as_bool(), true);
    EXPECT_EQ(json(false).type(), json::kind::boolean);
    EXPECT_EQ(json(int8_t(-5)).as_int(), -5);
    EXPECT_EQ(json(uint16_t(65535)).as_int(), 65535);
    EXPECT_EQ(json(size_t(7)).as_uint(), 7u);
    EXPECT_EQ(json(UINT64_MAX).as_uint(), UINT64_MAX);
    EXPECT_EQ(json(UINT64_MAX).as_int(), std::nullopt);
    EXPECT_EQ(json(INT64_MIN).as_int(), INT64_MIN);
    EXPECT_EQ(json(INT64_MIN).as_uint(), std::nullopt);
    EXPECT_EQ(json(2.5).as_double(), 2.5);
    EXPECT_EQ(json(2.5f).as_double(), 2.5);
    EXPECT_EQ(json("text").as_string(), sgcl::string("text"));
    EXPECT_EQ(json(sgcl::string()).as_string(), sgcl::string());
    EXPECT_TRUE(json(sgcl::string()).is_string());
    EXPECT_EQ(json::array({1, "a", nullptr}).type(), json::kind::array);
    EXPECT_EQ(json::object({{"a", 1}}).type(), json::kind::object);
    // one kind at a time
    json n(3);
    EXPECT_TRUE(n.is_number());
    EXPECT_TRUE(n.is_integer());
    EXPECT_FALSE(n.is_string());
    EXPECT_EQ(n.as_bool(), std::nullopt);
    EXPECT_EQ(n.as_string(), std::nullopt);
    EXPECT_EQ(json("3").as_int(), std::nullopt);
    EXPECT_EQ(json(true).as_int(), std::nullopt);
}

// A number as any number type, exactly or not at all; as a double rounded
TEST(JsonValue_Tests, NumbersAsTypes) {
    EXPECT_EQ(json(2.0).as_int(), 2);
    EXPECT_EQ(json(2.5).as_int(), std::nullopt);
    EXPECT_EQ(json(-2.0).as_uint(), std::nullopt);
    EXPECT_EQ(json(-0.0).as_int(), 0);
    EXPECT_EQ(json(9223372036854775808.0).as_int(), std::nullopt);
    EXPECT_EQ(json(9223372036854775808.0).as_uint(), 9223372036854775808ull);
    EXPECT_EQ(json(-9223372036854775808.0).as_int(), INT64_MIN);
    EXPECT_EQ(json(18446744073709551616.0).as_uint(), std::nullopt);
    EXPECT_EQ(json(1e300).as_int(), std::nullopt);
    EXPECT_EQ(json(int64_t(9007199254740993)).as_double(), 9007199254740992.0);
    EXPECT_FALSE(json(2.5).is_integer());
    EXPECT_TRUE(json(2.0).is_integer());
    EXPECT_TRUE(json(UINT64_MAX).is_integer());
    EXPECT_EQ(parsed("100000000000000000000000").number_text(), sgcl::string("100000000000000000000000"));
    EXPECT_FALSE(parsed("100000000000000000000000").is_integer());
    EXPECT_EQ(json(1).number_text(), std::nullopt);
}

// A key that is not there, an index past the end, a member asked of an
// array: null, so a chain never fails half-way
TEST(JsonValue_Tests, Lookups) {
    auto doc = parsed(R"({"user": {"name": "Ala", "tags": ["a", "b"]}, "count": 3, "": "empty key"})");
    EXPECT_EQ(doc["user"]["name"].as_string(), sgcl::string("Ala"));
    EXPECT_EQ(doc["user"]["tags"][1].as_string(), sgcl::string("b"));
    EXPECT_TRUE(doc["user"]["tags"][2].is_null());
    EXPECT_TRUE(doc["nobody"]["name"]["deeper"].is_null());
    EXPECT_TRUE(doc["count"]["x"].is_null());
    EXPECT_TRUE(doc[0].is_null());
    EXPECT_TRUE(doc["user"]["tags"]["0"].is_null());
    EXPECT_EQ(doc[""].as_string(), sgcl::string("empty key"));
    EXPECT_EQ(doc[sgcl::string("count")].as_int(), 3);
    EXPECT_TRUE(doc.contains("user"));
    EXPECT_TRUE(doc.contains(""));
    EXPECT_FALSE(doc.contains("tags"));
    EXPECT_EQ(doc.size(), 3u);
    EXPECT_EQ(doc["user"]["tags"].size(), 2u);
    EXPECT_EQ(doc["count"].size(), 0u);
    EXPECT_TRUE(json(1).empty());
    EXPECT_TRUE(json::array({}).empty());
    // the members in the order of the input, bound as [key, value]
    std::string keys;
    for (auto& [key, value] : doc.members()) {
        keys += std::string(key.view()) + ";";
    }
    EXPECT_EQ(keys, "user;count;;");
    std::string tags;
    for (auto& t : doc["user"]["tags"].elements()) {
        tags += t.as_string()->view();
    }
    EXPECT_EQ(tags, "ab");
    EXPECT_TRUE(doc["count"].elements().empty());
    EXPECT_TRUE(doc["user"]["tags"].members().empty());
}

// Past 16 members an object is found by a keyed hash: the same answers,
// through set and erase too
TEST(JsonValue_Tests, LargeObjects) {
    for (int n : {15, 16, 17, 100, 5000}) {
        json::builder b;
        for (int i = 0; i < n; ++i) {
            b.set(sgcl::string("key" + std::to_string(i)), i);
        }
        json o = b.build();
        ASSERT_EQ(o.size(), size_t(n));
        for (int i = 0; i < n; ++i) {
            EXPECT_EQ(o[sgcl::string("key" + std::to_string(i))].as_int(), i) << n;
        }
        EXPECT_FALSE(o.contains("key-1"));
        EXPECT_TRUE(o["missing"].is_null());
        EXPECT_EQ(o.members()[size_t(n - 1)].key, sgcl::string("key" + std::to_string(n - 1)));
        json changed = o.set("key0", "zero").set("new", true);
        EXPECT_EQ(changed["key0"].as_string(), sgcl::string("zero"));
        EXPECT_EQ(changed["new"].as_bool(), true);
        EXPECT_EQ(changed.size(), size_t(n + 1));
        EXPECT_EQ(o["key0"].as_int(), 0);
        json fewer = changed.erase(sgcl::string("key" + std::to_string(n / 2)));
        EXPECT_EQ(fewer.size(), size_t(n));
        EXPECT_FALSE(fewer.contains(sgcl::string("key" + std::to_string(n / 2))));
        EXPECT_EQ(fewer["new"].as_bool(), true);
        // parsed and written back, the order kept
        auto again = parsed(std::string(o.to_string().view()));
        EXPECT_EQ(again.to_string(), o.to_string());
        EXPECT_EQ(again, o);
    }
}

TEST(JsonValue_Tests, NewVersions) {
    auto doc = parsed(R"({"a": 1, "b": [1, 2]})");
    auto before = doc.to_string();
    auto set = doc.set("a", 2);
    auto added = doc.set("c", "x");
    auto erased = doc.erase("a");
    auto same = doc.erase("nothing");
    auto pushed = doc["b"].push_back(3);
    auto replaced = doc["b"].set(0, "first");
    auto past = doc["b"].set(5, "x");
    EXPECT_EQ(doc.to_string(), before);   // never changed
    EXPECT_EQ(set.to_string(), R"({"a":2,"b":[1,2]})");
    EXPECT_EQ(added.to_string(), R"({"a":1,"b":[1,2],"c":"x"})");
    EXPECT_EQ(erased.to_string(), R"({"b":[1,2]})");
    EXPECT_EQ(same, doc);
    EXPECT_EQ(pushed.to_string(), "[1,2,3]");
    EXPECT_EQ(replaced.to_string(), R"(["first",2])");
    EXPECT_EQ(past, doc["b"]);
    // on a value of another kind: an object of one member, an array of one element
    EXPECT_EQ(json(5).set("k", 1).to_string(), R"({"k":1})");
    EXPECT_EQ(json().push_back(1).to_string(), "[1]");
    EXPECT_EQ(json("s").set(0, 1), json("s"));
    EXPECT_EQ(json::array({}).push_back(json::object({})).to_string(), "[{}]");
}

// RFC 6901, section 5: the document and every pointer of the example
TEST(JsonValue_Tests, JsonPointerRfc6901) {
    auto doc = parsed(R"({"foo": ["bar", "baz"], "": 0, "a/b": 1, "c%d": 2, "e^f": 3, "g|h": 4, "i\\j": 5, "k\"l": 6, " ": 7, "m~n": 8})");
    EXPECT_EQ(doc.at_path(""), doc);
    EXPECT_EQ(doc.at_path("/foo")->to_string(), R"(["bar","baz"])");
    EXPECT_EQ(doc.at_path("/foo/0")->as_string(), sgcl::string("bar"));
    EXPECT_EQ(doc.at_path("/")->as_int(), 0);
    EXPECT_EQ(doc.at_path("/a~1b")->as_int(), 1);
    EXPECT_EQ(doc.at_path("/c%d")->as_int(), 2);
    EXPECT_EQ(doc.at_path("/e^f")->as_int(), 3);
    EXPECT_EQ(doc.at_path("/g|h")->as_int(), 4);
    EXPECT_EQ(doc.at_path("/i\\j")->as_int(), 5);
    EXPECT_EQ(doc.at_path("/k\"l")->as_int(), 6);
    EXPECT_EQ(doc.at_path("/ ")->as_int(), 7);
    EXPECT_EQ(doc.at_path("/m~0n")->as_int(), 8);
    // what is not there, and what is not a pointer
    EXPECT_EQ(doc.at_path("/foo/2"), std::nullopt);
    EXPECT_EQ(doc.at_path("/foo/01"), std::nullopt);
    EXPECT_EQ(doc.at_path("/foo/-"), std::nullopt);
    EXPECT_EQ(doc.at_path("/foo/0/x"), std::nullopt);
    EXPECT_EQ(doc.at_path("/nothing"), std::nullopt);
    EXPECT_EQ(doc.at_path("foo"), std::nullopt);
    EXPECT_EQ(doc.at_path("/m~2n"), std::nullopt);
    EXPECT_EQ(doc.at_path("/m~"), std::nullopt);
}

TEST(JsonValue_Tests, SetPath) {
    auto doc = parsed(R"({"user": {"name": "Ala", "tags": ["a"]}, "n": 1})");
    EXPECT_EQ(doc.set_path("/user/name", "Ola").to_string(), R"({"user":{"name":"Ola","tags":["a"]},"n":1})");
    EXPECT_EQ(doc.set_path("/user/age", 30).to_string(), R"({"user":{"name":"Ala","tags":["a"],"age":30},"n":1})");
    EXPECT_EQ(doc.set_path("/user/tags/-", "b").to_string(), R"({"user":{"name":"Ala","tags":["a","b"]},"n":1})");
    EXPECT_EQ(doc.set_path("/user/tags/1", "b").to_string(), R"({"user":{"name":"Ala","tags":["a","b"]},"n":1})");
    EXPECT_EQ(doc.set_path("/user/tags/0", "z").to_string(), R"({"user":{"name":"Ala","tags":["z"]},"n":1})");
    EXPECT_EQ(doc.set_path("/a/b/c", true).to_string(), R"({"user":{"name":"Ala","tags":["a"]},"n":1,"a":{"b":{"c":true}}})");
    EXPECT_EQ(json().set_path("/a", 1).to_string(), R"({"a":1})");
    EXPECT_EQ(doc.set_path("", 5), json(5));
    EXPECT_EQ(doc.set_path("/a~1b", 1)["a/b"].as_int(), 1);
    // what cannot be done gives the value unchanged
    EXPECT_EQ(doc.set_path("/user/tags/5", "x"), doc);
    EXPECT_EQ(doc.set_path("/user/tags/01", "x"), doc);
    EXPECT_EQ(doc.set_path("/n/x", "x"), doc);
    EXPECT_EQ(doc.set_path("/user/name/x", "x"), doc);
    EXPECT_EQ(doc.set_path("user", "x"), doc);
    EXPECT_EQ(doc.to_string(), R"({"user":{"name":"Ala","tags":["a"]},"n":1})");
    // a thousand levels deep, without a stack of calls
    std::string pointer;
    for (int i = 0; i < 1000; ++i) {
        pointer += "/k";
    }
    auto deep = json().set_path(sgcl::string(pointer), 1);
    EXPECT_EQ(deep.at_path(sgcl::string(pointer))->as_int(), 1);
}

TEST(JsonValue_Tests, Builder) {
    json::builder squares;
    for (auto i : sgcl::range(5)) {
        squares.push_back(i * i);
    }
    EXPECT_EQ(squares.size(), 5u);
    EXPECT_EQ(squares.build().to_string(), "[0,1,4,9,16]");
    EXPECT_EQ(squares.size(), 0u);
    EXPECT_EQ(squares.build().to_string(), "[]");   // an empty builder builds []
    json::builder o;
    o.set("a", 1).set("b", 2).set("a", 3);
    EXPECT_EQ(o.build().to_string(), R"({"b":2,"a":3})");   // the last value wins
    // the builder is empty again and may build the other kind
    o.push_back(1);
    EXPECT_EQ(o.build().to_string(), "[1]");
    json::builder mixed;
    mixed.push_back(1);
    EXPECT_THROW(mixed.set("a", 1), std::logic_error);
    json::builder mixed2;
    mixed2.set("a", 1);
    EXPECT_THROW(mixed2.push_back(1), std::logic_error);
    // array from a range, object from a list with a key twice
    std::vector<int> v = {1, 2, 3};
    EXPECT_EQ(json::array(v).to_string(), "[1,2,3]");
    EXPECT_EQ(json::array(sgcl::vector<sgcl::string>{"x", "y"}).to_string(), R"(["x","y"])");
    EXPECT_EQ(json::object({{"k", 1}, {"k", 2}}).to_string(), R"({"k":2})");
    EXPECT_EQ(json::object({{"count", 10}, {"squares", json::array({1, 4})}}).to_string(), R"({"count":10,"squares":[1,4]})");
}

// Equal by value: numbers of every kind, objects in any order; equal
// values hash alike
TEST(JsonValue_Tests, EqualityAndHash) {
    auto eq = [](const json& a, const json& b) {
        EXPECT_EQ(a, b) << a.to_string().view() << " " << b.to_string().view();
        EXPECT_EQ(a.hash(), b.hash()) << a.to_string().view() << " " << b.to_string().view();
    };
    eq(json(1), json(1.0));
    eq(json(0), json(-0.0));
    eq(json(uint64_t(5)), json(5));
    eq(json(UINT64_MAX), json(18446744073709551615.0));   // as doubles
    eq(parsed("18446744073709551616"), json(18446744073709551616.0));
    eq(parsed("1e2"), json(100));
    eq(parsed("[1, {\"a\": [true, null], \"b\": \"x\"}]"), parsed("[1.0, {\"b\": \"x\", \"a\": [true, null]}]"));
    eq(parsed("{}"), json::object({}));
    eq(json("ż"), parsed("\"\\u017c\""));
    json::options keep;
    keep.keep_number_text = true;
    eq(json::parse(text("1.50"), keep).value(), json::parse(text("15e-1"), keep).value());
    eq(json::parse(text("2.0"), keep).value(), json(2));
    EXPECT_NE(json(1), json(2));
    EXPECT_NE(json(1), json("1"));
    EXPECT_NE(json(), json(false));
    EXPECT_NE(json(0), json(false));
    EXPECT_NE(parsed("[1, 2]"), parsed("[2, 1]"));
    EXPECT_NE(parsed("{\"a\": 1}"), parsed("{\"a\": 1, \"b\": 2}"));
    EXPECT_NE(parsed("{\"a\": 1}"), parsed("{\"b\": 1}"));
    EXPECT_NE(json(int64_t(9007199254740993)), json(int64_t(9007199254740992)));   // two integers: exactly
    EXPECT_NE(json::parse(text("1.0000000000000000000001"), keep).value(), json::parse(text("1"), keep).value());
    // a hundred thousand arrays deep, built by hand: compared, hashed and
    // written without a stack of calls
    json a = json::array({});
    json b = json::array({});
    for (int i = 0; i < 100000; ++i) {
        a = json::array({a});
        b = json::array({b});
    }
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.hash(), b.hash());
    EXPECT_EQ(a.to_string().size(), 200002u);
}

TEST(JsonValue_Tests, Text) {
    auto doc = parsed(R"({"a": [1, 2.5, -0.0, 1e21, 1e-7, 123456789012345678901234567890], "b": {"c": {}, "d": []}, "e": "<ż>\u0001", "f": null, "g": [true, false]})");
    EXPECT_EQ(doc.to_string(), R"({"a":[1,2.5,-0,1e+21,1e-7,123456789012345678901234567890],"b":{"c":{},"d":[]},"e":"<ż>\u0001","f":null,"g":[true,false]})");
    EXPECT_EQ(doc.to_string({0, true}), R"({"a":[1,2.5,-0,1e+21,1e-7,123456789012345678901234567890],"b":{"c":{},"d":[]},"e":"\u003cż\u003e\u0001","f":null,"g":[true,false]})");
    EXPECT_EQ(doc["b"].to_string(json::pretty), "{\n  \"c\": {},\n  \"d\": []\n}");
    EXPECT_EQ(parsed("[[1]]").to_string({4}), "[\n    [\n        1\n    ]\n]");
    EXPECT_EQ(json().to_string(), "null");
    EXPECT_EQ(json(0.1f).to_string(), "0.10000000149011612");   // a float in a json is its double
}

// The keys of one parse are made once: a thousand objects with the same
// fields share each key's string
TEST(JsonValue_Tests, KeysShared) {
    std::string t = "[";
    for (int i = 0; i < 1000; ++i) {
        t += (i ? "," : "") + std::string("{\"id\": ") + std::to_string(i) + ", \"name\": \"n\"}";
    }
    auto doc = parsed(t + "]");
    const void* id = doc[0].members()[0].key.object();
    for (size_t i = 1; i < 1000; ++i) {
        EXPECT_EQ(doc[i].members()[0].key.object(), id);
    }
}

// A large tree parsed while the collector runs, and read from threads
// while it runs again: immutable, shared with no lock
TEST(JsonValue_Tests, SurvivesTheCollector) {
    std::string t = "[";
    for (int i = 0; i < 20000; ++i) {
        t += (i ? "," : "") + std::string("{\"i\": ") + std::to_string(i) + ", \"s\": \"" + std::to_string(i * 7) + "\", \"a\": [" + std::to_string(i) + ", 1.5, {\"x\": null}]}";
    }
    t += "]";
    std::atomic<bool> stop = false;
    std::thread collector([&] {
        while (!stop) {
            sgcl::collector::force_collect(true);
        }
    });
    json doc;
    for (int round = 0; round < 3; ++round) {
        doc = parsed(t);
    }
    std::atomic<int> wrong = 0;
    std::vector<std::thread> readers;
    for (int k = 0; k < 4; ++k) {
        readers.emplace_back([&, k] {
            for (int i = k; i < 20000; i += 4) {
                auto& e = doc[size_t(i)];
                if (e["i"].as_int() != i || e["s"].as_string() != sgcl::string(std::to_string(i * 7)) || e["a"][0].as_int() != i || !e["a"][2]["x"].is_null()) {
                    ++wrong;
                }
            }
        });
    }
    for (auto& r : readers) {
        r.join();
    }
    stop = true;
    collector.join();
    EXPECT_EQ(wrong, 0);
    EXPECT_EQ(doc.to_string().view(), parsed(t).to_string().view());
}
