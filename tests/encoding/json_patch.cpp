//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// json: JSON Patch (RFC 6902), JSON Merge Patch (RFC 7396), erase_path,
// path_of and diff. The examples of RFC 6902's appendix A and RFC 7396's,
// cases in the shape of the json-patch-tests suite (a document, a patch, the
// result or the error), and diff checked by patch over random pairs.
#include "common.h"

#include <functional>
#include <random>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    json j(const char* text) {
        auto v = json::parse(text);
        if (!v) {
            throw std::runtime_error(std::string("bad test JSON: ") + text);
        }
        return *v;
    }
}

TEST(JsonPatch_Tests, Rfc6902AppendixA) {
    struct {
        const char* name;
        const char* doc;
        const char* patch;
        const char* expected;   // nullptr: an error
        errc code;
    } cases[] = {
        {"A.1", R"({"foo":"bar"})", R"([{"op":"add","path":"/baz","value":"qux"}])", R"({"baz":"qux","foo":"bar"})", {}},
        {"A.2", R"({"foo":["bar","baz"]})", R"([{"op":"add","path":"/foo/1","value":"qux"}])", R"({"foo":["bar","qux","baz"]})", {}},
        {"A.3", R"({"baz":"qux","foo":"bar"})", R"([{"op":"remove","path":"/baz"}])", R"({"foo":"bar"})", {}},
        {"A.4", R"({"foo":["bar","qux","baz"]})", R"([{"op":"remove","path":"/foo/1"}])", R"({"foo":["bar","baz"]})", {}},
        {"A.5", R"({"baz":"qux","foo":"bar"})", R"([{"op":"replace","path":"/baz","value":"boo"}])", R"({"baz":"boo","foo":"bar"})", {}},
        {"A.6", R"({"foo":{"bar":"baz","waldo":"fred"},"qux":{"corge":"grault"}})", R"([{"op":"move","from":"/foo/waldo","path":"/qux/thud"}])",
         R"({"foo":{"bar":"baz"},"qux":{"corge":"grault","thud":"fred"}})", {}},
        {"A.7", R"({"foo":["all","grass","cows","eat"]})", R"([{"op":"move","from":"/foo/1","path":"/foo/3"}])", R"({"foo":["all","cows","eat","grass"]})", {}},
        {"A.8", R"({"baz":"qux","foo":["a",2,"c"]})", R"([{"op":"test","path":"/baz","value":"qux"},{"op":"test","path":"/foo/1","value":2}])",
         R"({"baz":"qux","foo":["a",2,"c"]})", {}},
        {"A.9", R"({"baz":"qux"})", R"([{"op":"test","path":"/baz","value":"bar"}])", nullptr, errc::type_mismatch},
        {"A.10", R"({"foo":"bar"})", R"([{"op":"add","path":"/child","value":{"grandchild":{}}}])", R"({"foo":"bar","child":{"grandchild":{}}})", {}},
        {"A.11", R"({"foo":"bar"})", R"([{"op":"add","path":"/baz","value":"qux","xyz":123}])", R"({"foo":"bar","baz":"qux"})", {}},
        {"A.12", R"({"foo":"bar"})", R"([{"op":"add","path":"/baz/bat","value":"qux"}])", nullptr, errc::missing_field},
        {"A.14", R"({"/":9,"~1":10})", R"([{"op":"test","path":"/~01","value":10}])", R"({"/":9,"~1":10})", {}},
        {"A.15", R"({"/":9,"~1":10})", R"([{"op":"test","path":"/~01","value":"10"}])", nullptr, errc::type_mismatch},
        {"A.16", R"({"foo":["bar"]})", R"([{"op":"add","path":"/foo/-","value":["abc","def"]}])", R"({"foo":["bar",["abc","def"]]})", {}},
    };
    for (auto& c : cases) {
        auto r = j(c.doc).patch(j(c.patch));
        if (c.expected) {
            ASSERT_TRUE(r) << c.name << ": " << r.error().message();
            EXPECT_EQ(*r, j(c.expected)) << c.name << ": " << r->to_string();
        } else {
            ASSERT_FALSE(r) << c.name;
            EXPECT_EQ(r.error().code(), c.code) << c.name << ": " << r.error().message();
            EXPECT_EQ(r.error().path(), "/0") << c.name;
        }
    }
    // A.13: a member given twice is no JSON a patch could be read from
    EXPECT_EQ(json::parse(R"([{"op":"add","path":"/baz","value":"qux","op":"remove"}])").error().code(), errc::duplicate_key);
}

TEST(JsonPatch_Tests, TheSuitesShape) {
    struct {
        const char* doc;
        const char* patch;
        const char* expected;
        errc code;
    } cases[] = {
        // the root
        {R"({})", R"([{"op":"add","path":"","value":[1]}])", "[1]", {}},
        {R"([1])", R"([{"op":"replace","path":"","value":{"a":1}}])", R"({"a":1})", {}},
        {R"({"a":1})", R"([{"op":"remove","path":""}])", nullptr, errc::syntax},
        {R"({"a":1})", R"([{"op":"test","path":"","value":{"a":1.0}}])", R"({"a":1})", {}},
        // empty keys and the escapes
        {R"({})", R"([{"op":"add","path":"/","value":1}])", R"({"":1})", {}},
        {R"({"":{"":2}})", R"([{"op":"replace","path":"//","value":3}])", R"({"":{"":3}})", {}},
        {R"({"a/b":1,"m~n":2})", R"([{"op":"remove","path":"/a~1b"},{"op":"remove","path":"/m~0n"}])", "{}", {}},
        {R"({})", R"([{"op":"add","path":"/~2","value":1}])", nullptr, errc::syntax},
        // add replaces a member, inserts into an array
        {R"({"a":1})", R"([{"op":"add","path":"/a","value":2}])", R"({"a":2})", {}},
        {R"([1,2])", R"([{"op":"add","path":"/0","value":0},{"op":"add","path":"/3","value":3}])", "[0,1,2,3]", {}},
        {R"([1,2])", R"([{"op":"add","path":"/3","value":3}])", nullptr, errc::out_of_range},
        {R"([1,2])", R"([{"op":"add","path":"/01","value":3}])", nullptr, errc::out_of_range},
        {R"([1,2])", R"([{"op":"add","path":"/-1","value":3}])", nullptr, errc::out_of_range},
        {R"([1,2])", R"([{"op":"add","path":"/x","value":3}])", nullptr, errc::out_of_range},
        {R"({"a":1})", R"([{"op":"add","path":"/a/b","value":3}])", nullptr, errc::missing_field},
        // remove and replace need their target
        {R"({"a":1})", R"([{"op":"remove","path":"/b"}])", nullptr, errc::missing_field},
        {R"([1])", R"([{"op":"remove","path":"/1"}])", nullptr, errc::out_of_range},
        {R"([1])", R"([{"op":"remove","path":"/-"}])", nullptr, errc::out_of_range},
        {R"({"a":1})", R"([{"op":"replace","path":"/b","value":2}])", nullptr, errc::missing_field},
        {R"([1])", R"([{"op":"replace","path":"/0","value":null}])", "[null]", {}},
        // copy and move
        {R"({"a":{"b":1}})", R"([{"op":"copy","from":"/a","path":"/c"}])", R"({"a":{"b":1},"c":{"b":1}})", {}},
        {R"({"a":[1,2]})", R"([{"op":"copy","from":"/a/0","path":"/a/-"}])", R"({"a":[1,2,1]})", {}},
        {R"({"a":1})", R"([{"op":"copy","from":"/b","path":"/c"}])", nullptr, errc::missing_field},
        {R"({"a":{"b":1}})", R"([{"op":"move","from":"/a","path":"/a/c"}])", nullptr, errc::syntax},
        {R"({"a":1})", R"([{"op":"move","from":"/a","path":"/a"}])", R"({"a":1})", {}},
        {R"({"a":1,"b":2})", R"([{"op":"move","from":"/a","path":"/b"}])", R"({"b":1})", {}},
        {R"([1,2,3])", R"([{"op":"move","from":"/0","path":"/-"}])", "[2,3,1]", {}},
        // test: numbers by value, objects in any order
        {R"({"a":[1,{"x":1,"y":2}]})", R"([{"op":"test","path":"/a","value":[1.0,{"y":2,"x":1}]}])", R"({"a":[1,{"x":1,"y":2}]})", {}},
        {R"({"a":null})", R"([{"op":"test","path":"/a","value":null}])", R"({"a":null})", {}},
        {R"({})", R"([{"op":"test","path":"/a","value":null}])", nullptr, errc::missing_field},
        {R"({"a":"1"})", R"([{"op":"test","path":"/a","value":1}])", nullptr, errc::type_mismatch},
        // malformed operations
        {R"({})", R"({"op":"add"})", nullptr, errc::syntax},
        {R"({})", R"([1])", nullptr, errc::syntax},
        {R"({})", R"([{"path":"/a","value":1}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"add","value":1}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"add","path":"a","value":1}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"add","path":"/a"}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"replace","path":"/a"}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"test","path":"/a"}])", nullptr, errc::syntax},
        {R"({"a":1})", R"([{"op":"move","path":"/b"}])", nullptr, errc::syntax},
        {R"({"a":1})", R"([{"op":"copy","path":"/b","from":1}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":"spam","path":"/a"}])", nullptr, errc::syntax},
        {R"({})", R"([{"op":1,"path":"/a"}])", nullptr, errc::syntax},
        {R"({})", "[]", "{}", {}},
        // a value inside a number, a string, a boolean
        {R"({"a":5})", R"([{"op":"add","path":"/a/b/c","value":1}])", nullptr, errc::missing_field},
        {R"({"a":"s"})", R"([{"op":"add","path":"/a/0","value":1}])", nullptr, errc::missing_field},
    };
    for (auto& c : cases) {
        auto r = j(c.doc).patch(j(c.patch));
        if (c.expected) {
            ASSERT_TRUE(r) << c.doc << " " << c.patch << ": " << r.error().message();
            EXPECT_EQ(*r, j(c.expected)) << c.doc << " " << c.patch << ": " << r->to_string();
        } else {
            ASSERT_FALSE(r) << c.doc << " " << c.patch << " -> " << r->to_string();
            EXPECT_EQ(r.error().code(), c.code) << c.doc << " " << c.patch << ": " << r.error().message();
        }
    }
}

TEST(JsonPatch_Tests, AllOrNothing) {
    // the second operation fails: the document is as it was, the error names
    // the operation
    json doc = j(R"({"a":1})");
    auto r = doc.patch(j(R"([{"op":"add","path":"/b","value":2},{"op":"remove","path":"/c"},{"op":"add","path":"/d","value":4}])"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().path(), "/1");
    EXPECT_EQ(r.error().message(), "/1: remove: no value at /c");
    EXPECT_EQ(doc, j(R"({"a":1})"));
    // the patch of a document applied to a document made of it
    json big = json::object({});
    sgcl::vector<json> ops;
    for (int i = 0; i < 2000; ++i) {
        ops.push_back(json::object({{"op", "add"}, {"path", sgcl::string("/k" + std::to_string(i))}, {"value", i}}));
    }
    auto many = big.patch(json::array(ops));
    ASSERT_TRUE(many);
    EXPECT_EQ(many->size(), 2000u);
    EXPECT_EQ(many->at_path("/k1999")->as_int(), 1999);
}

TEST(JsonPatch_Tests, MergePatchRfc7396) {
    struct {
        const char* target;
        const char* patch;
        const char* result;
    } cases[] = {
        {R"({"a":"b"})", R"({"a":"c"})", R"({"a":"c"})"},
        {R"({"a":"b"})", R"({"b":"c"})", R"({"a":"b","b":"c"})"},
        {R"({"a":"b"})", R"({"a":null})", R"({})"},
        {R"({"a":"b","b":"c"})", R"({"a":null})", R"({"b":"c"})"},
        {R"({"a":["b"]})", R"({"a":"c"})", R"({"a":"c"})"},
        {R"({"a":"c"})", R"({"a":["b"]})", R"({"a":["b"]})"},
        {R"({"a":{"b":"c"}})", R"({"a":{"b":"d","c":null}})", R"({"a":{"b":"d"}})"},
        {R"({"a":[{"b":"c"}]})", R"({"a":[1]})", R"({"a":[1]})"},
        {R"(["a","b"])", R"(["c","d"])", R"(["c","d"])"},
        {R"({"a":"b"})", R"(["c"])", R"(["c"])"},
        {R"({"a":"foo"})", R"(null)", R"(null)"},
        {R"({"a":"foo"})", R"("bar")", R"("bar")"},
        {R"({"e":null})", R"({"a":1})", R"({"e":null,"a":1})"},
        {R"([1,2])", R"({"a":"b","c":null})", R"({"a":"b"})"},
        {R"({})", R"({"a":{"bb":{"ccc":null}}})", R"({"a":{"bb":{}}})"},
        // RFC 7396 §3's example
        {R"({"title":"Goodbye!","author":{"givenName":"John","familyName":"Doe"},"tags":["example","sample"],"content":"This will be unchanged"})",
         R"({"title":"Hello!","phoneNumber":"+01-123-456-7890","author":{"familyName":null},"tags":["example"]})",
         R"({"title":"Hello!","author":{"givenName":"John"},"tags":["example"],"content":"This will be unchanged","phoneNumber":"+01-123-456-7890"})"},
    };
    for (auto& c : cases) {
        json r = j(c.target).merge_patch(j(c.patch));
        EXPECT_EQ(r, j(c.result)) << c.target << " + " << c.patch << " = " << r.to_string();
    }
    // the order of the members kept, a merged member in its place
    EXPECT_EQ(j(R"({"a":{"x":1},"b":2})").merge_patch(j(R"({"a":{"y":2}})")).to_string(), R"({"a":{"x":1,"y":2},"b":2})");
    // deep, without recursion
    std::string deep_patch, deep_target;
    for (int i = 0; i < 400; ++i) {
        deep_patch += "{\"k\":";
        deep_target += "{\"k\":";
    }
    deep_patch += "1";
    deep_target += "0";
    for (int i = 0; i < 400; ++i) {
        deep_patch += "}";
        deep_target += "}";
    }
    EXPECT_EQ(j(deep_target.c_str()).merge_patch(j(deep_patch.c_str())), j(deep_patch.c_str()));
}

TEST(JsonPatch_Tests, ErasePathAndPathOf) {
    json doc = j(R"({"a":{"b":[1,2,3]},"c/d":4})");
    EXPECT_EQ(doc.erase_path("/a/b/1"), j(R"({"a":{"b":[1,3]},"c/d":4})"));
    EXPECT_EQ(doc.erase_path("/c~1d"), j(R"({"a":{"b":[1,2,3]}})"));
    EXPECT_EQ(doc.erase_path("/x"), doc);
    EXPECT_EQ(doc.erase_path("/a/b/9"), doc);
    EXPECT_EQ(doc.erase_path(""), doc);
    EXPECT_EQ(doc.erase_path("bad"), doc);
    EXPECT_EQ(json::path_of({"a/b", "~", "0"}), "/a~1b/~0/0");
    EXPECT_EQ(json::path_of({}), "");
    EXPECT_EQ(json::path_of({""}), "/");
    EXPECT_EQ(doc.at_path(json::path_of({"c/d"}))->as_int(), 4);
}

TEST(JsonPatch_Tests, DiffAppliedGivesTheOther) {
    EXPECT_EQ(json::diff(j(R"({"a":1})"), j(R"({"a":1})")).to_string(), "[]");
    EXPECT_EQ(json::diff(j(R"({"a":1,"b":2})"), j(R"({"a":3,"c":4})")).to_string(),
              R"([{"op":"remove","path":"/b"},{"op":"add","path":"/c","value":4},{"op":"replace","path":"/a","value":3}])");
    EXPECT_EQ(json::diff(j("[1,2,3]"), j("[1,5]")).to_string(), R"([{"op":"remove","path":"/2"},{"op":"replace","path":"/1","value":5}])");
    EXPECT_EQ(json::diff(j("1"), j(R"("x")")).to_string(), R"([{"op":"replace","path":"","value":"x"}])");
    // random pairs
    std::mt19937_64 rng(20261006);
    std::function<json(int)> make = [&](int depth) -> json {
        int k = int(rng() % (depth > 3 ? 4 : 6));
        switch (k) {
            case 0: return json();
            case 1: return json(int64_t(rng() % 5));
            case 2: return json(sgcl::string(std::string(1, char('a' + rng() % 3))));
            case 3: return json(bool(rng() & 1));
            case 4: {
                sgcl::vector<json> es;
                for (int i = int(rng() % 4); i > 0; --i) {
                    es.push_back(make(depth + 1));
                }
                return json::array(es);
            }
            default: {
                json::builder b;
                int n = int(rng() % 4);
                for (int i = 0; i < n; ++i) {
                    b.set(sgcl::string(std::string(1, char('p' + rng() % 4))), make(depth + 1));
                }
                return n ? b.build() : json::object({});
            }
        }
    };
    for (int i = 0; i < 3000; ++i) {
        json a = make(0), b = make(0);
        json d = json::diff(a, b);
        auto r = a.patch(d);
        ASSERT_TRUE(r) << a.to_string() << " " << b.to_string() << " " << d.to_string() << ": " << r.error().message();
        ASSERT_EQ(*r, b) << a.to_string() << " -> " << b.to_string() << " by " << d.to_string();
    }
}
