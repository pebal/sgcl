//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// json_schema: JSON Schema draft 2020-12. No validator is on the machine to
// hold it to, so json_schema/suite.json holds cases written from the two
// specifications keyword by keyword, in the JSON-Schema-Test-Suite's form
// (a schema, instances, valid or not); valid and validate must agree on
// each. Then the output's locations, what compile refuses, resources by $id,
// the formats as assertions, the depth, many threads on one schema, and the
// boundaries.
#include "common.h"
#include "tests/source_root.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    json j(const char* text) {
        auto v = json::parse(sgcl::string(text));
        if (!v) {
            throw std::runtime_error(std::string("bad test JSON: ") + text);
        }
        return *v;
    }

    json_schema compile(const char* text) {
        auto s = json_schema::compile(j(text));
        if (!s) {
            throw std::runtime_error(std::string("bad test schema: ") + text + ": " + std::string(s.error().message().view()));
        }
        return *s;
    }
}

TEST(JsonSchema_Tests, Suite) {
    std::ifstream f(source_root() / "tests/encoding/json_schema/suite.json", std::ios::binary);
    ASSERT_TRUE(f);
    std::stringstream ss;
    ss << f.rdbuf();
    auto suite = json::parse(sgcl::string(ss.str()));
    ASSERT_TRUE(suite) << suite.error().message();
    size_t cases = 0;
    for (const auto& group : suite->elements()) {
        std::string name(group["description"].as_string("?").view());
        auto schema = json_schema::compile(group["schema"]);
        ASSERT_TRUE(schema) << name << ": " << schema.error().message();
        for (const auto& t : group["tests"].elements()) {
            bool want = t["valid"].as_bool(false);
            bool got = schema->valid(t["data"]);
            auto errors = schema->validate(t["data"]);
            EXPECT_EQ(got, want) << name << ": " << t["data"].to_string();
            EXPECT_EQ(errors.empty(), want) << name << ": " << t["data"].to_string() << (errors.empty() ? "" : ": " + std::string(errors[0].message.view()));
            ++cases;
        }
    }
    EXPECT_GT(cases, 250u);
}

TEST(JsonSchema_Tests, Output) {
    auto s = compile(R"({"$id": "https://example.com/order.json",
        "properties": {"items": {"type": "array", "items": {"$ref": "item.json"}}, "total": {"type": "number", "minimum": 0}},
        "required": ["items"],
        "$defs": {"item": {"$id": "item.json", "properties": {"name": {"type": "string"}, "qty": {"type": "integer", "minimum": 1}},
                           "required": ["name"]}}})");
    EXPECT_EQ(s.id(), "https://example.com/order.json");
    auto errors = s.validate(j(R"({"items": [{"name": "a", "qty": 1}, {"qty": 0}, {"name": 5}], "total": -1})"));
    ASSERT_EQ(errors.size(), 4u);
    EXPECT_EQ(errors[0].instance_location, "/items/1");
    EXPECT_EQ(errors[0].keyword_location, "/properties/items/items/$ref/required");
    EXPECT_EQ(errors[0].absolute_location, "https://example.com/item.json#/required");
    EXPECT_EQ(errors[0].message, "a required property is missing: name");
    EXPECT_EQ(errors[1].instance_location, "/items/1/qty");
    EXPECT_EQ(errors[1].keyword_location, "/properties/items/items/$ref/properties/qty/minimum");
    EXPECT_EQ(errors[2].instance_location, "/items/2/name");
    EXPECT_EQ(errors[2].keyword_location, "/properties/items/items/$ref/properties/name/type");
    EXPECT_EQ(errors[2].message, "an integer where the type is \"string\"");
    EXPECT_EQ(errors[3].instance_location, "/total");
    EXPECT_EQ(errors[3].absolute_location, "https://example.com/order.json#/properties/total/minimum");
    // keys escaped in the pointers
    auto esc = compile(R"({"properties": {"a/b~c": {"type": "string"}}})").validate(j(R"({"a/b~c": 1})"));
    ASSERT_EQ(esc.size(), 1u);
    EXPECT_EQ(esc[0].instance_location, "/a~1b~0c");
    EXPECT_EQ(esc[0].keyword_location, "/properties/a~1b~0c/type");
    EXPECT_EQ(esc[0].absolute_location, "https://schema.invalid/root.json#/properties/a~1b~0c/type");
    // anyOf: every branch's errors and its own; a match leaves none
    auto any = compile(R"({"anyOf": [{"type": "string"}, {"minimum": 5}]})");
    auto ae = any.validate(j("1"));
    ASSERT_EQ(ae.size(), 3u);
    EXPECT_EQ(ae[2].keyword_location, "/anyOf");
    EXPECT_TRUE(any.validate(j("6")).empty());
    auto one = compile(R"({"oneOf": [{"minimum": 1}, {"minimum": 2}]})").validate(j("3"));
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0].message, "subschemas 0 and 1 of oneOf both match");
}

TEST(JsonSchema_Tests, CompileErrors) {
    for (const char* bad : {R"("string")", R"(1)", R"({"type": "text"})", R"({"type": []})", R"({"minLength": -1})", R"({"minLength": 1.5})",
                            R"({"required": [1]})", R"({"properties": []})", R"({"properties": {"a": 1}})", R"({"allOf": []})", R"({"items": 3})",
                            R"({"multipleOf": 0})", R"({"enum": 1})", R"({"$ref": "#/$defs/missing"})", R"({"$ref": "https://other.example/x.json"})",
                            R"({"$ref": "#nowhere"})", R"({"$schema": "http://json-schema.org/draft-07/schema#"})", R"({"$id": 1})",
                            R"({"$anchor": "1x"})", R"({"pattern": "(a)\\1"})", R"j({"pattern": "(?=a)"})j", R"({"patternProperties": {"(": {}}})",
                            R"({"dependentRequired": {"a": "b"}})", R"({"format": 1})", R"({"$id": "x.json#frag"})"}) {
        auto s = json_schema::compile(j(bad));
        EXPECT_FALSE(s) << bad;
        if (!s) {
            EXPECT_EQ(s.error().offset(), 0u) << bad;
        }
    }
    EXPECT_EQ(json_schema::compile(j(R"j({"pattern": "(?=a)"})j")).error().code(), errc::unsupported_value);
    EXPECT_EQ(json_schema::compile(j(R"({"$ref": "#/x"})")).error().code(), errc::missing_field);
    EXPECT_THROW(json_schema(j("1")), sgcl::bad_expected_access<error>);
    EXPECT_FALSE(json_schema::parse("{"));
    EXPECT_TRUE(json_schema::parse(R"({"type": "string"})")->valid(j("\"x\"")));
}

TEST(JsonSchema_Tests, Resources) {
    sgcl::vector<json> resources;
    resources.push_back(j(R"({"$id": "https://example.com/address.json", "type": "object", "required": ["city"],
                              "properties": {"city": {"type": "string"}, "geo": {"$ref": "geo.json"}}})"));
    resources.push_back(j(R"({"$id": "https://example.com/geo.json", "type": "array", "prefixItems": [{"type": "number"}, {"type": "number"}], "items": false})"));
    auto s = json_schema::compile(j(R"({"$id": "https://example.com/person.json", "properties": {"home": {"$ref": "address.json"}}})"), resources);
    ASSERT_TRUE(s) << s.error().message();
    EXPECT_TRUE(s->valid(j(R"({"home": {"city": "Kraków", "geo": [50.06, 19.94]}})")));
    EXPECT_FALSE(s->valid(j(R"({"home": {"geo": [50.06, 19.94]}})")));
    EXPECT_FALSE(s->valid(j(R"({"home": {"city": "x", "geo": [1, 2, 3]}})")));
    auto e = s->validate(j(R"({"home": {"city": 1}})"));
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].absolute_location, "https://example.com/address.json#/properties/city/type");
    EXPECT_FALSE(json_schema::compile(j(R"({"$ref": "address.json"})"), resources));   // relative to the default base: no such resource
    sgcl::vector<json> no_id;
    no_id.push_back(j(R"({"type": "string"})"));
    EXPECT_FALSE(json_schema::compile(j("true"), no_id));
}

TEST(JsonSchema_Tests, Formats) {
    json_schema::options o;
    o.format_assertion = true;
    struct {
        const char* format;
        std::vector<const char*> good, bad;
    } cases[] = {
        {"date-time", {"2026-10-06T09:30:00Z", "2026-10-06t09:30:00.123+02:00", "1990-12-31T23:59:60Z", "1990-12-31T15:59:60-08:00"},
         {"2026-10-06 09:30:00Z", "2026-10-06T09:30:00", "2026-02-30T00:00:00Z", "2026-10-06T24:00:00Z", "1990-12-31T22:59:60Z"}},
        {"date", {"2024-02-29", "2026-12-31"}, {"2026-02-29", "2026-1-01", "20261006"}},
        {"time", {"09:30:00Z", "23:59:59.5+14:00"}, {"09:30:00", "9:30:00Z", "09:30:00+24:00"}},
        {"duration", {"P1D", "PT1H30M", "P1Y2M3DT4H5M6S", "P2W"}, {"P", "PT", "P1H", "P1W2D", "PT1S2M", "1D"}},
        {"email", {"jan@example.com", "a.b+c@sub.example.org", "x@[127.0.0.1]"}, {"jan", "@example.com", "a..b@example.com", "jan@-bad.com"}},
        {"hostname", {"example.com", "a-b.c", "localhost"}, {"-a.com", "a_b.com", "", "a..b"}},
        {"ipv4", {"192.168.0.1", "0.0.0.0"}, {"256.1.1.1", "01.1.1.1", "1.1.1", "1.1.1.1.1"}},
        {"ipv6", {"::1", "2001:db8::8a2e:370:7334", "::ffff:192.168.0.1", "1:2:3:4:5:6:7:8"}, {"1:2:3:4:5:6:7:8:9", "1::2::3", ":1", "12345::"}},
        {"uri", {"https://example.com/a?b#c", "urn:isbn:0451450523"}, {"/relative", "https://exa mple.com", "https://example.com/#a#b"}},
        {"uri-reference", {"/relative", "#frag", ""}, {"has space", "\\back"}},
        {"uuid", {"123e4567-e89b-12d3-a456-426614174000"}, {"123e4567e89b12d3a456426614174000", "123e4567-e89b-12d3-a456-42661417400g"}},
        {"json-pointer", {"", "/a/b~0~1"}, {"a", "/a~2"}},
        {"relative-json-pointer", {"0", "1/a", "2#"}, {"-1", "01", "/a"}},
        {"regex", {"^a+$", "[a-z]"}, {"(", "[a"}},
    };
    for (auto& c : cases) {
        auto s = json_schema::compile(j((std::string(R"({"format": ")") + c.format + "\"}").c_str()), o).value();
        auto lax = json_schema::compile(j((std::string(R"({"format": ")") + c.format + "\"}").c_str())).value();
        for (const char* g : c.good) {
            EXPECT_TRUE(s.valid(json(sgcl::string(g)))) << c.format << " " << g;
        }
        for (const char* b : c.bad) {
            EXPECT_FALSE(s.valid(json(sgcl::string(b)))) << c.format << " " << b;
            EXPECT_TRUE(lax.valid(json(sgcl::string(b)))) << c.format << " " << b;
        }
        EXPECT_TRUE(s.valid(json(1))) << c.format;   // only strings
    }
    EXPECT_TRUE(json_schema::compile(j(R"({"format": "x-unknown"})"), o)->valid(json(sgcl::string("anything"))));
}

TEST(JsonSchema_Tests, Depth) {
    json_schema::options o;
    o.max_depth = 3;
    auto s = json_schema::compile(j(R"({"items": {"items": {"items": {"type": "integer"}}}})"), o).value();
    EXPECT_TRUE(s.valid(j("[[[1]]]")));
    auto deep = json_schema::compile(j(R"({"items": {"items": {"items": {"items": {"type": "integer"}}}}})"), o).value();
    EXPECT_FALSE(deep.valid(j("[[[[1]]]]")));
    auto e = deep.validate(j("[[[[1]]]]"));
    ASSERT_FALSE(e.empty());
    // a recursive schema walks as deep as the instance, within max_depth
    auto tree = compile(R"({"type": "object", "properties": {"child": {"$ref": "#"}}})");
    std::string text = "{}";
    for (int i = 0; i < 200; ++i) {
        text = "{\"child\": " + text + "}";
    }
    EXPECT_TRUE(tree.valid(j(text.c_str())));
}

TEST(JsonSchema_Tests, ManyThreads) {
    auto s = compile(R"({"type": "array", "items": {"type": "object", "properties": {"n": {"type": "integer", "multipleOf": 3}}, "required": ["n"]}, "uniqueItems": true})");
    std::string good = "[", bad = "[";
    for (int i = 0; i < 300; ++i) {
        good += (i ? "," : "") + std::string("{\"n\": ") + std::to_string(i * 3) + "}";
        bad += (i ? "," : "") + std::string("{\"n\": ") + std::to_string(i * 3 + (i == 150)) + "}";
    }
    good += "]";
    bad += "]";
    sgcl::vector<sgcl::async::task<int>> tasks;
    for (int t = 0; t < 8; ++t) {
        tasks.push_back(sgcl::async::spawn([](json_schema s, std::string g, std::string b) -> sgcl::async::task<int> {
            int ok = 0;
            auto gj = json::parse(sgcl::string(g)).value();
            auto bj = json::parse(sgcl::string(b)).value();
            for (int k = 0; k < 20; ++k) {
                ok += s.valid(gj) && !s.valid(bj) && s.validate(bj).size() == 1;
                co_await sgcl::async::yield();
            }
            co_return ok;
        }(s, good, bad)));
    }
    for (auto& t : tasks) {
        EXPECT_EQ(t.wait(), 20);
    }
    sgcl::async::scheduler::stop();
}

TEST(JsonSchema_Tests, Boundaries) {
    json_schema any;
    EXPECT_TRUE(any.valid(j("null")));
    EXPECT_TRUE(any.validate(j(R"({"a": [1]})")).empty());
    EXPECT_EQ(any.id(), "https://schema.invalid/root.json");
    json_schema copy = any;
    json_schema moved = std::move(copy);
    EXPECT_TRUE(moved.valid(j("1")));
    auto none = compile("false");
    EXPECT_FALSE(none.valid(j("1")));
    ASSERT_EQ(none.validate(j("1")).size(), 1u);
    EXPECT_EQ(none.validate(j("1"))[0].keyword_location, "");
    EXPECT_TRUE(compile("{}").valid(j("[]")));
    // load
    std::string path = (std::filesystem::temp_directory_path() / ("sgcl_json_schema_" + std::to_string(::getpid()) + ".json")).string();
    {
        std::ofstream out(path);
        out << R"({"type": "integer"})";
    }
    auto loaded = json_schema::load(sgcl::string(path));
    ASSERT_TRUE(loaded);
    EXPECT_TRUE(loaded->valid(j("3")));
    std::filesystem::remove(path);
    EXPECT_EQ(json_schema::load(sgcl::string(path)).error().code(), errc::io);
}
