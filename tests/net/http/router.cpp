//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the routes. The oracle is Go's ServeMux (tools/route_oracle.go
// writes route_oracle.h): pairs of patterns, whether the second conflicts
// with the first; a table of patterns and requests against it, with and
// without its catch-alls, each answered by the pattern that serves it (and
// the values of its wildcards), a 404, a 405 with Allow, or a 307 with
// Location. Then the patterns that do not parse.
#include "tests/types.h"
#include "sgcl/net/http/detail/router.h"
#include "route_oracle.h"

#include <string>
#include <vector>

using namespace sgcl::net::http::detail;

namespace {
    void check(const RouteTable& t, const route_oracle::Request& q) {
        auto f = t.find(q.method, q.host, q.path);
        std::string where = std::string(q.method) + " " + q.host + q.path;
        switch (q.status) {
            case 200: {
                ASSERT_EQ(f.kind, RouteTable::Found::route) << where;
                EXPECT_EQ(t.pattern(f.index).text, q.pattern) << where;
                std::string values;
                for (auto& v : f.values) {
                    values += (values.empty() ? "" : "&") + v.first + "=" + v.second;
                }
                EXPECT_EQ(values, q.values) << where;
                break;
            }
            case 307:
                ASSERT_EQ(f.kind, RouteTable::Found::redirect) << where;
                EXPECT_EQ(f.location, q.location) << where;
                break;
            case 405:
                ASSERT_EQ(f.kind, RouteTable::Found::method_not_allowed) << where;
                EXPECT_EQ(f.allow, q.allow) << where;
                break;
            case 404:
                EXPECT_EQ(f.kind, RouteTable::Found::not_found) << where;
                break;
            default:
                ADD_FAILURE() << "a status the test does not know: " << q.status;
        }
    }
}

TEST(HttpRouter_Tests, ConflictsAgainstGo) {
    for (auto& p : route_oracle::pairs) {
        RouteTable t;
        t.add(p.first);
        bool threw = false;
        try {
            t.add(p.second);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        EXPECT_EQ(threw, p.conflict) << p.first << " then " << p.second;
    }
}

TEST(HttpRouter_Tests, RequestsAgainstGo) {
    RouteTable all, some;
    for (auto p : route_oracle::table) {
        all.add(p);
        std::string s(p);
        if (s != "/" && s.rfind("example.com/", 0) != 0) {
            some.add(p);
        }
    }
    for (auto& q : route_oracle::requests) {
        check(all, q);
    }
    for (auto& q : route_oracle::requests_without_catch_alls) {
        check(some, q);
    }
}

TEST(HttpRouter_Tests, PatternsThatDoNotParse) {
    for (auto bad : {"", "GET", "GET  ", "G(T /", "/a//b", "/{", "/{}", "/{x", "/a{x}", "/{x}b", "/{1x}", "/{x-y}", "/{x...}/a",
                     "/{$}/a", "/a/{$}b", "/{x}/{x}", "/a/{$}/", "example.com"}) {
        RouteTable t;
        EXPECT_THROW(t.add(bad), std::invalid_argument) << bad;
    }
    RouteTable t;
    t.add("GET\t /a/{x}/{y...}");
    EXPECT_EQ(t.pattern(0).method, "GET");
    t.add("Example.COM/x");
    EXPECT_EQ(t.pattern(1).host, "example.com");
    auto f = t.find("GET", "EXAMPLE.com:80", "/x");
    EXPECT_EQ(f.kind, RouteTable::Found::route);
    EXPECT_EQ(f.index, 1u);
}
