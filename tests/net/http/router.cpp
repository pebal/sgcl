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
#include "router_legacy.h"

#include <random>
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
                    values += (values.empty() ? "" : "&") + std::string(v.first.view()) + "=" + std::string(v.second.view());
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

// The lookup in views (PathSegments, no allocation of the system's) against
// the one in strings it replaced (router_legacy.h): the tables of the
// Go oracle and tables of their own, and paths made of the patterns'
// segments, wildcard values, '%' escapes (valid, broken, of a '/'), empty
// segments, trailing slashes, a segment longer than the buffer and more
// segments than are held in place; every answer the same (the kind, the
// pattern, the values, Allow, Location)
TEST(HttpRouter_Tests, TheLookupOfViewsAnswersAsTheOneOfStrings) {
    std::vector<std::vector<std::string>> tables;
    std::vector<std::string> go_table(std::begin(route_oracle::table), std::end(route_oracle::table));
    tables.push_back(go_table);
    tables.push_back({"GET /a/{x}/{y...}", "POST /a/{x}/b", "/a/", "/files/{path...}", "GET /x/{$}", "example.com/{z}", "/%41%2Fb/{q}"});
    tables.push_back({"/{a}/{b}/{c}/{d}", "GET /h/{x}", "POST /h/{x}/"});
    const std::vector<std::string> pieces = {"a", "b", "x", "files", "deep", "h", "posts", "latest", "images", "thumbnails", "index.html",
                                             "%41", "%2F", "%2", "%zz", "%", "caf%C3%A9", "{$}", "", "A%2fb"};
    const std::vector<std::string> methods = {"GET", "HEAD", "POST", "PUT"};
    const std::vector<std::string> hosts = {"", "example.com", "EXAMPLE.com:8080", "other.com", "[::1]:80", "[::1]"};
    std::mt19937 rng(20260927);
    size_t compared = 0;
    for (auto& patterns : tables) {
        RouteTable t;
        std::vector<const RoutePattern*> legacy;
        for (auto& p : patterns) {
            try {
                t.add(p);
            } catch (const std::invalid_argument&) {
                // a conflict with one before it: the table without it
            }
        }
        EXPECT_GE(t.size(), 2u);
        for (size_t i = 0; i < t.size(); ++i) {
            legacy.push_back(&t.pattern(i));
        }
        std::vector<std::string> paths = {"", "/", "//", "/a", "/a/", "/a//b", "noslash", "/x/", "/x", "/files/", "/files/a/b/c/"};
        paths.push_back("/" + std::string(700, 'q') + "%41");      // past the buffer: the managed spill
        std::string many;
        for (int k = 0; k < 40; ++k) {
            many += "/s%41" + std::to_string(k);                    // more segments than are held in place
        }
        paths.push_back(many);
        paths.push_back(many + "/");
        for (int k = 0; k < 3000; ++k) {
            std::string path;
            int n = int(rng() % 6);
            for (int s = 0; s < n; ++s) {
                path += "/" + pieces[rng() % pieces.size()];
            }
            if (rng() % 3 == 0) {
                path += "/";
            }
            paths.push_back(path);
        }
        for (auto& path : paths) {
            for (auto& method : methods) {
                const auto& host = hosts[compared % hosts.size()];
                auto now = t.find(method, host, path);
                auto was = router_legacy::find(legacy, method, host, path);
                std::string where = method + " " + host + " " + path;
                ASSERT_EQ(int(now.kind), int(was.kind)) << where;
                EXPECT_EQ(now.location, was.location) << where;
                EXPECT_EQ(now.allow, was.allow) << where;
                if (now.kind == RouteTable::Found::route) {
                    EXPECT_EQ(now.index, was.index) << where;
                    ASSERT_EQ(now.values.size(), was.values.size()) << where;
                    for (size_t v = 0; v < was.values.size(); ++v) {
                        EXPECT_EQ(std::string(now.values[v].first.view()), was.values[v].first) << where;
                        EXPECT_EQ(std::string(now.values[v].second.view()), was.values[v].second) << where;
                    }
                }
                ++compared;
            }
        }
    }
    EXPECT_GT(compared, 30000u);
}
