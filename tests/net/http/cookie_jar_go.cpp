//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::cookie_jar against Go's net/http/cookiejar as the oracle
// (go_cookiejar/main.go, built here; skipped where there is no go): the
// same script of Set-Cookie values from URLs and Cookie fields asked for
// URLs, the answers compared line by line. Go's jar runs with a nil
// PublicSuffixList (x/net/publicsuffix is not in the standard library), so
// the script keeps to domains no suffix rule touches, and to what both jars
// decide alike: no Secure from http and no cookie of a Secure one's name
// from http, no prefixes, no SameSite=None (RFC 6265bis's rules, which Go's
// jar does not have), and no host-only and
// domain cookie of one name, path and host (RFC 6265bis counts the host-only
// flag in a cookie's identity, Go does not). Domains, default and given
// paths, path matching, the order, replacement, Max-Age, Expires, deletion,
// Secure over https, addresses.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    const std::string& oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_cookiejar/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_cookiejar";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    struct Op {
        bool set = false;
        std::string url;
        std::string field;
    };

    // The script: operations drawn from hosts, paths and attributes on which
    // the two jars agree
    std::vector<Op> script(unsigned seed, int count) {
        std::mt19937 rng(seed);
        auto pick = [&](auto& list) -> const std::string& {
            return list[rng() % list.size()];
        };
        static const std::vector<std::string> hosts = {"example.com", "www.example.com", "a.b.example.com", "b.example.com",
                                                       "other.example.com", "example.org", "www.example.org", "10.0.0.1"};
        static const std::vector<std::string> paths = {"/", "/a", "/a/", "/a/b", "/a/b/c", "/ab", "/a/bc/d", "/x/y"};
        static const std::vector<std::string> domains = {"example.com", ".example.com", "www.example.com", "b.example.com",
                                                         "example.org", "EXAMPLE.com", "10.0.0.1", "nope.net"};
        static const std::vector<std::string> attr_paths = {"/", "/a", "/a/", "/a/b", "relative", "/ab"};
        static const std::vector<std::string> ages = {"Max-Age=0", "Max-Age=-1", "Max-Age=3600", "Expires=Thu, 01 Jan 1970 00:00:00 GMT",
                                                      "Expires=Fri, 01 Jan 2100 00:00:00 GMT", "Max-Age=60; Expires=Thu, 01 Jan 1970 00:00:00 GMT"};
        std::vector<Op> ops;
        int value = 0;
        for (int i : range(count)) {
            (void)i;
            Op op;
            bool https = rng() % 4 == 0;
            op.url = (https ? "https://" : "http://") + pick(hosts) + pick(paths);
            if (rng() % 3 != 0) {
                op.set = true;
                bool domain = rng() % 2 == 0;
                bool secure = https && rng() % 3 == 0;
                // the host-only flag in the name: no host-only and domain
                // cookie of one name; Secure too: an insecure origin never
                // meets a Secure cookie of its name (RFC 6265bis leaves those alone)
                std::string name = std::string(domain ? "d" : "h") + (secure ? "s" : "") + char('a' + rng() % 3);
                op.field = name + "=v" + std::to_string(++value);
                if (domain) {
                    op.field += "; Domain=" + pick(domains);
                }
                if (rng() % 2) {
                    op.field += "; Path=" + pick(attr_paths);
                }
                if (rng() % 4 == 0) {
                    op.field += "; " + pick(ages);
                }
                if (secure) {
                    op.field += "; Secure";
                }
                if (rng() % 5 == 0) {
                    op.field += "; HttpOnly";
                }
            }
            ops.push_back(op);
        }
        return ops;
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }
}

TEST(HttpCookieJarGo_Tests, TheSameScriptGivesTheSameCookies) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    size_t compared = 0, nonempty = 0;
    for (unsigned seed : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
        auto ops = script(seed, 1500);
        auto path = std::filesystem::temp_directory_path() / ("sgcl_cookie_jar_go_" + std::to_string(::getpid()) + ".txt");
        {
            std::ofstream f(path);
            for (auto& op : ops) {
                f << (op.set ? "set\t" : "get\t") << op.url << (op.set ? "\t" + op.field : "") << "\n";
            }
        }
        std::string theirs = run("'" + oracle() + "' < '" + path.string() + "'");
        std::filesystem::remove(path);
        net::http::cookie_jar jar;
        size_t at = 0;
        int line = 0;
        for (auto& op : ops) {
            ++line;
            net::url u(sgcl::string(op.url));
            if (op.set) {
                jar.set_cookies(u, {net::http::cookie(sgcl::string(op.field))});
                continue;
            }
            size_t end = theirs.find('\n', at);
            ASSERT_NE(end, std::string::npos) << "seed " << seed << ": Go's answers ended at line " << line;
            std::string want = theirs.substr(at, end - at);
            at = end + 1;
            std::string got = text(jar.header(u));
            ASSERT_EQ(got, want) << "seed " << seed << ", line " << line << ": get " << op.url;
            ++compared;
            nonempty += !want.empty();
        }
    }
    EXPECT_GT(compared, 3000u);
    EXPECT_GT(nonempty, compared / 2);
}
