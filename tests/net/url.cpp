//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: url and query_params. The oracle is the WHATWG URL Standard's own
// test data from the web platform tests (tools/url_oracle.go writes
// url_oracle.h): every case of urltestdata.json (parse, with and without a
// base, and every part after it), of setters_tests.json (every setter) and
// of toascii.json (the host parser), none left out. Go's net/url is asked
// where the two standards agree, the cases named in the generator: the
// examples of RFC 3986 §5.4, the parts of plain URLs, the pairs of
// queries. The cases below the oracles' are the API's own: what the
// setters refuse, the parts the standard has no getter for, the pairs.
#include "tests/types.h"
#include "sgcl/net/url.h"
#include "url_oracle.h"

#include <string>
#include <string_view>
#include <vector>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    sgcl::string str(std::string_view s) {
        return sgcl::string(s);
    }

    // The parts as the standard's API gets them
    struct Parts {
        std::string href, origin, protocol, username, password, host, hostname, port, pathname, search, hash;
    };

    Parts parts_of(const net::url& u) {
        Parts p;
        p.href = text(u.to_string());
        p.origin = text(u.origin());
        p.protocol = text(u.scheme()) + ":";
        p.username = text(u.username());
        p.password = text(u.password());
        p.hostname = text(net::detail::UrlAccess::host_as_written(u));   // WHATWG's hostname keeps an IPv6 address's brackets
        p.port = u.port() ? std::to_string(*u.port()) : "";
        p.host = text(u.host());
        p.pathname = text(u.path());
        p.search = u.query().empty() ? "" : "?" + text(u.query());
        p.hash = u.fragment().empty() ? "" : "#" + text(u.fragment());
        return p;
    }

    std::string show(std::string_view s) {
        std::string out;
        for (unsigned char c : s) {
            if (c >= 0x20 && c < 0x7f) {
                out.push_back(char(c));
            } else {
                char b[8];
                snprintf(b, sizeof b, "\\x%02x", c);
                out += b;
            }
        }
        return out;
    }

    // One setter of the standard by its name in setters_tests.json
    net::url apply(const net::url& u, std::string_view setter, std::string_view value) {
        using A = net::detail::UrlAccess;
        auto v = str(value);
        if (setter == "protocol") {
            return A::protocol(u, v).value;
        }
        if (setter == "username") {
            return A::username(u, v, false).value;
        }
        if (setter == "password") {
            return A::username(u, v, true).value;
        }
        if (setter == "host") {
            return A::host(u, v, false).value;
        }
        if (setter == "hostname") {
            return A::host(u, v, true).value;
        }
        if (setter == "port") {
            return A::port(u, v).value;
        }
        if (setter == "pathname") {
            return A::pathname(u, v).value;
        }
        if (setter == "search") {
            return A::search(u, v);
        }
        if (setter == "hash") {
            return A::hash(u, v);
        }
        if (setter == "href") {
            auto p = net::url::parse(v);
            return p ? *p : u;   // the API throws and keeps the URL
        }
        ADD_FAILURE() << "unknown setter " << setter;
        return u;
    }
}

TEST(NetUrl_Tests, ParseAgainstTheStandardsTests) {
    size_t checked = 0, wrong = 0;
    for (auto& c : url_oracle::parse_cases) {
        optional<net::url> u;
        if (c.has_base) {
            auto base = net::url::parse(str(c.base));
            if (base) {
                if (auto r = net::url::parse(str(c.input), *base)) {
                    u = *r;
                }
            }
        } else if (auto r = net::url::parse(str(c.input))) {
            u = *r;
        }
        ++checked;
        if (c.failure) {
            if (u) {
                ++wrong;
                ADD_FAILURE() << "parsed what is to fail: " << show(c.input) << " base " << show(c.base) << " -> " << text(u->to_string());
            }
            continue;
        }
        if (!u) {
            ++wrong;
            ADD_FAILURE() << "failed to parse: " << show(c.input) << " base " << show(c.base);
            continue;
        }
        auto p = parts_of(*u);
        bool ok = p.href == c.href && p.protocol == c.protocol && p.username == c.username && p.password == c.password
               && p.host == c.host && p.hostname == c.hostname && p.port == c.port && p.pathname == c.pathname
               && p.search == c.search && p.hash == c.hash && (!c.has_origin || p.origin == c.origin);
        if (!ok) {
            ++wrong;
            ADD_FAILURE() << show(c.input) << " base " << show(c.base) << "\n  href " << show(p.href) << " want " << show(c.href)
                          << "\n  origin " << p.origin << " want " << c.origin << "\n  host " << p.host << " want " << c.host
                          << "\n  path " << show(p.pathname) << " want " << show(c.pathname) << "\n  search " << show(p.search)
                          << " want " << show(c.search) << "\n  hash " << show(p.hash) << " want " << show(c.hash);
        }
    }
    EXPECT_EQ(wrong, 0u);
    EXPECT_GT(checked, 850u);
}

TEST(NetUrl_Tests, SettersAgainstTheStandardsTests) {
    size_t checked = 0;
    for (auto& c : url_oracle::setter_cases) {
        auto u = net::url::parse(str(c.href));
        ASSERT_TRUE(u) << show(c.href);
        auto after = apply(*u, c.setter, c.value);
        auto p = parts_of(after);
        const std::string* got[10] = {&p.href, &p.protocol, &p.username, &p.password, &p.host, &p.hostname, &p.port, &p.pathname, &p.search, &p.hash};
        static const char* names[10] = {"href", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash"};
        for (int i = 0; i < 10; ++i) {
            if (c.named[i]) {
                EXPECT_EQ(*got[i], std::string(c.expected[i])) << c.setter << " = " << show(c.value) << " on " << show(c.href) << ": " << names[i];
            }
        }
        ++checked;
    }
    EXPECT_GT(checked, 250u);
}

TEST(NetUrl_Tests, HostsAgainstTheStandardsTests) {
    for (auto& c : url_oracle::toascii_cases) {
        auto input = std::string(c.input);
        auto u = net::url::parse(str("https://" + input + "/x"));
        if (c.failure) {
            EXPECT_FALSE(u) << show(input);
        } else {
            ASSERT_TRUE(u) << show(input);
            EXPECT_EQ(text(u->host()), std::string(c.output)) << show(input);
            EXPECT_EQ(text(u->path()), "/x");
            EXPECT_EQ(text(u->to_string()), "https://" + std::string(c.output) + "/x");
        }
        // the host and hostname setters on https://x/x: the new host, or x kept
        auto x = *net::url::parse(str("https://x/x"));
        auto by_hostname = x.with_hostname(str(input));
        auto by_host = net::detail::UrlAccess::host(x, str(input), false).value;
        if (c.failure) {
            EXPECT_FALSE(by_hostname) << show(input);
            EXPECT_EQ(text(net::detail::UrlAccess::host_as_written(by_host)), "x") << show(input);
        } else {
            ASSERT_TRUE(by_hostname) << show(input);
            EXPECT_EQ(text(net::detail::UrlAccess::host_as_written(*by_hostname)), std::string(c.output));
            EXPECT_EQ(text(net::detail::UrlAccess::host_as_written(by_host)), std::string(c.output));
        }
    }
}

TEST(NetUrl_Tests, ResolveAgainstGo) {
    auto base = net::url::parse(str(url_oracle::resolve_base));
    ASSERT_TRUE(base);
    // the one example the two standards part on: the URL Standard gives
    // a special URL with an empty path the path "/", RFC 3986 leaves it
    // empty
    size_t named = 0;
    for (auto& c : url_oracle::resolve_cases) {
        auto r = base->resolve(str(c.reference));
        ASSERT_TRUE(r) << c.reference;
        if (c.reference == "//g") {
            EXPECT_EQ(c.go, "http://g");
            EXPECT_EQ(text(r->to_string()), "http://g/");
            ++named;
            continue;
        }
        EXPECT_EQ(text(r->to_string()), std::string(c.go)) << "reference " << c.reference;
    }
    EXPECT_EQ(named, 1u);
}

// The one-pass path of url::parse against the parser it shortens: URLs
// put together from parts, the plain and the odd of each (a case, a third
// slash, a backslash, credentials, IPv4 in its forms, IPv6 and what follows
// it, ports, dot segments written every way, bytes escaped and not, UTF-8,
// a tab), every part in turn over a plain URL and then 40 000 mixes, each
// part of them plain half the time. Where
// the path takes a text it must give the parser's URL, part for part;
// what it leaves, the parser alone answers.
TEST(NetUrl_Tests, TheOnePassPathAgreesWithTheParser) {
    using A = net::detail::UrlAccess;
    const std::vector<std::vector<std::string>> dims = {
        {"http", "HTTPS", "Ws", "wss", "ftp", "FTP", "file", "foo", "h2", "httpss"},
        {"://", ":///", ":/", ":\\\\", "://\\", ":"},
        {"", "u@", "u:p@", "u:@", ":p@", "@", "a:b:c@", "a b@", "u@v@", "%41@", "\xC3\xA9@", "u/p@", "U:P@"},
        {"example.com", "EXAMPLE.Com", "a", "1.2.3.4", "0x7f.1", "127.1", "999.1.1.1", "1.2.3.4.5", "[::1]", "[2001:DB8::1]",
         "[::1]x", "[::1]]", "[::", "[1.2.3.4]", "exa%41mple.com", "b\xC3\xBC" "cher.de", "a^b", "", "com.", ".", "a..", "0x", "1.0x",
         "cafe", "1.2.3.0xg", "1.2.3.09", "x.0X1", "a:b", "a\tb", "-", "a_b", "a~b"},
        {"", ":", ":80", ":0080", ":443", ":8080", ":65535", ":65536", ":x", ":21", ":0", ":1a"},
        {"", "/", "/a/b", "/a/../b", "/./a", "/a/..", "/a/.", "/%2e/%2E%2e/x", "/a b/\"<>`{}", "/\xC3\xA9", "/a\\b", "/.%2e", "/..",
         "/a//b/", "/..%2f", "/%2e.", "/a/./", "/a/%2E", "/^|", "/a?b"},
        {"", "?", "?a=1&b='x'", "?\" <>", "?\xC3\xA9", "?a#b", "?%zz"},
        {"", "#", "#a b`", "#\xC3\xA9#x", "#<\">"},
    };
    size_t taken = 0, total = 0;
    auto check = [&](const std::vector<size_t>& pick) {
        std::string t;
        for (size_t d = 0; d < dims.size(); ++d) {
            t += dims[d][pick[d]];
        }
        ++total;
        auto fast = A::parse_in_one_pass(str(t));
        if (!fast) {
            return;
        }
        ++taken;
        auto slow = A::parse_by_parser(str(t));
        if (!slow) {
            ADD_FAILURE() << "the one-pass path parsed what the parser refuses: " << show(t) << " -> " << text(fast->to_string());
            return;
        }
        auto f = parts_of(*fast), p = parts_of(*slow);
        if (f.href != p.href || f.origin != p.origin || f.username != p.username || f.password != p.password || f.host != p.host
            || f.pathname != p.pathname || f.search != p.search || f.hash != p.hash || fast->has_query() != slow->has_query()
            || fast->has_fragment() != slow->has_fragment() || fast->host_address() != slow->host_address()) {
            ADD_FAILURE() << show(t) << "\n  one pass " << show(f.href) << " host " << show(f.host) << " path " << show(f.pathname)
                          << "\n  parser   " << show(p.href) << " host " << show(p.host) << " path " << show(p.pathname);
        }
    };
    const std::vector<size_t> plain = {0, 0, 0, 0, 0, 2, 0, 0};
    for (size_t d = 0; d < dims.size(); ++d) {
        for (size_t v = 0; v < dims[d].size(); ++v) {
            auto pick = plain;
            pick[d] = v;
            check(pick);
        }
    }
    uint64_t seed = 0x9E3779B97F4A7C15u;
    for (int n = 0; n < 40000; ++n) {
        std::vector<size_t> pick(dims.size());
        for (size_t d = 0; d < dims.size(); ++d) {
            seed = seed * 6364136223846793005u + 1442695040888963407u;
            pick[d] = (seed >> 63) ? plain[d] : size_t(seed >> 33) % dims[d].size();   // half of them plain
        }
        check(pick);
    }
    EXPECT_GT(taken, total / 10) << taken << " of " << total;
}

TEST(NetUrl_Tests, PlainUrlsAgainstGo) {
    for (auto& c : url_oracle::plain_cases) {
        auto u = net::url::parse(str(c.input));
        ASSERT_TRUE(u) << c.input;
        EXPECT_EQ(text(u->scheme()), c.scheme);
        EXPECT_EQ(text(u->username()), c.username);
        EXPECT_EQ(text(u->password()), c.password);
        EXPECT_EQ(text(u->hostname()), c.hostname) << c.input;
        EXPECT_EQ(u->port() ? std::to_string(*u->port()) : std::string(), std::string(c.port)) << c.input;
        EXPECT_EQ(text(u->path()), c.path) << c.input;
        EXPECT_EQ(text(u->query()), c.query) << c.input;
        EXPECT_EQ(text(u->fragment()), c.fragment) << c.input;
    }
}

TEST(NetUrl_Tests, QueriesAgainstGo) {
    for (auto& c : url_oracle::query_cases) {
        auto q = net::query_params::parse(str(c.input));
        // Go groups the values by name; the pairs here are in their
        // order, so the comparison is of each name's values in order
        std::vector<std::string> got;
        std::vector<std::string> names;
        for (auto& p : q) {
            if (std::find(names.begin(), names.end(), text(p.first)) == names.end()) {
                names.push_back(text(p.first));
            }
        }
        for (auto& n : names) {
            for (auto& v : q.get_all(str(n))) {
                got.push_back(n);
                got.push_back(text(v));
            }
        }
        std::vector<std::string> want;
        for (int i = 0; i < c.count; ++i) {
            want.push_back(std::string(c.pairs[i]));
        }
        EXPECT_EQ(got, want) << show(c.input);
    }
}

TEST(NetUrl_Tests, TheSketchsExample) {
    auto u = net::url::parse("https://b\xC3\xBC" "cher.de/a/../szukaj?q=kot&page=2#wyniki");
    ASSERT_TRUE(u);
    EXPECT_EQ(u->host(), "xn--bcher-kva.de");
    EXPECT_EQ(u->path(), "/szukaj");
    EXPECT_EQ(u->query_params().get("q"), "kot");
    auto next = u->with_query(u->query_params().set("page", "3"));
    EXPECT_EQ(next.to_string(), "https://xn--bcher-kva.de/szukaj?q=kot&page=3#wyniki");
    auto logo = u->resolve("/img/logo.png");
    ASSERT_TRUE(logo);
    EXPECT_EQ(logo->to_string(), "https://xn--bcher-kva.de/img/logo.png");
    EXPECT_EQ(u->origin(), "https://xn--bcher-kva.de");
    EXPECT_EQ(u->request_target(), "/szukaj?q=kot&page=2");
}

TEST(NetUrl_Tests, PartsWithoutAGetterInTheStandard) {
    auto u = *net::url::parse("http://[::1]:8080/a?b#c");
    EXPECT_EQ(u.host(), "[::1]:8080");                     // with the port, as WHATWG's host and Go's URL.Host
    EXPECT_EQ(u.hostname(), "::1");                        // without it, and without the brackets, as Go's Hostname()
    ASSERT_TRUE(u.host_address());
    EXPECT_EQ(*u.host_address(), net::ip_address::loopback_v6());
    EXPECT_EQ(u.effective_port(), 8080);
    EXPECT_TRUE(u.has_query());
    EXPECT_TRUE(u.has_fragment());
    EXPECT_EQ(u.request_target(), "/a?b");

    auto v = *net::url::parse("https://0x7f.1/");
    EXPECT_EQ(v.host(), "127.0.0.1");
    EXPECT_EQ(*v.host_address(), net::ip_address::v4(127, 0, 0, 1));
    EXPECT_FALSE(v.port());
    EXPECT_EQ(v.effective_port(), 443);

    auto w = *net::url::parse("sc://1.2.3.4/");        // an opaque host, not an address
    EXPECT_FALSE(w.host_address());
    EXPECT_EQ(w.effective_port(), 0);
    EXPECT_FALSE(w.is_special());

    auto empty_query = *net::url::parse("http://x/?");
    EXPECT_TRUE(empty_query.has_query());
    EXPECT_EQ(empty_query.query(), "");
    EXPECT_EQ(empty_query.request_target(), "/?");
    EXPECT_EQ(empty_query.to_string(), "http://x/?");

    auto mail = *net::url::parse("mailto:someone@example.com");
    EXPECT_TRUE(mail.has_opaque_path());
    EXPECT_FALSE(mail.has_host());
    EXPECT_EQ(mail.path(), "someone@example.com");
    EXPECT_EQ(mail.origin(), "null");

    EXPECT_FALSE(net::url::parse("/relative"));
    EXPECT_FALSE(net::url::parse("http://exa mple.com/"));
    EXPECT_FALSE(net::url::parse("http://example.com:65536/"));
}

TEST(NetUrl_Tests, TheSettersRefuse) {
    auto u = *net::url::parse("http://example.com/a");
    EXPECT_FALSE(u.with_scheme("mailto"));                  // special to not special
    EXPECT_FALSE(u.with_scheme("1http"));                   // not a scheme
    EXPECT_EQ(u.with_scheme("https")->to_string(), "https://example.com/a");
    EXPECT_EQ(u.with_scheme("https:")->to_string(), "https://example.com/a");
    EXPECT_FALSE(u.with_hostname("example.com:8080"));      // the hostname setter takes no port
    EXPECT_EQ(u.with_host("example.com:8080")->host(), "example.com:8080");   // the host setter, host()'s pair, takes one
    EXPECT_EQ(u.with_host("example.com:8080")->port(), 8080);
    auto moved = *net::url::parse("http://other.org:9000/x");
    EXPECT_EQ(u.with_host(moved.host())->to_string(), "http://other.org:9000/a");   // what host() gives, with_host takes
    EXPECT_EQ(moved.with_host("a:")->to_string(), "http://a:9000/x");   // an empty port after ':' keeps the port (WHATWG, setters_tests)
    EXPECT_FALSE(moved.with_hostname("a:"));
    EXPECT_FALSE(u.with_host("exa mple.com"));
    EXPECT_FALSE(u.with_host(""));                          // a special URL has a host
    EXPECT_EQ(u.with_host("B\xC3\x9C" "CHER.de")->host(), "xn--bcher-kva.de");
    EXPECT_EQ(u.with_port(8080)->to_string(), "http://example.com:8080/a");
    EXPECT_EQ(u.with_port(80)->to_string(), "http://example.com/a");  // the default is no port
    EXPECT_EQ(u.with_port(8080)->with_port(nullopt)->to_string(), "http://example.com/a");
    EXPECT_EQ(u.with_path("x y/../z")->path(), "/z");
    EXPECT_EQ(u.with_path("b c")->path(), "/b%20c");
    EXPECT_EQ(u.with_username("a b")->to_string(), "http://a%20b@example.com/a");
    EXPECT_EQ(u.with_username("u")->with_password("p:w")->to_string(), "http://u:p%3Aw@example.com/a");

    auto file = *net::url::parse("file:///tmp/x");
    EXPECT_FALSE(file.with_port(8080));                     // file has no port
    EXPECT_FALSE(file.with_username("u"));
    EXPECT_EQ(file.with_host("server")->to_string(), "file://server/tmp/x");
    EXPECT_EQ(file.with_host("localhost")->to_string(), "file:///tmp/x");

    auto mail = *net::url::parse("mailto:x@example.com");
    EXPECT_FALSE(mail.with_host("example.com"));            // an opaque path
    EXPECT_FALSE(mail.with_path("y"));
    EXPECT_FALSE(mail.with_port(25));

    // the query and the fragment always apply
    EXPECT_EQ(u.with_query("?a=1 2").to_string(), "http://example.com/a?a=1%202");
    EXPECT_EQ(u.with_query("a").with_query("").to_string(), "http://example.com/a");
    EXPECT_EQ(u.with_fragment("#top").to_string(), "http://example.com/a#top");
    EXPECT_EQ(u.with_fragment("top").without_fragment().to_string(), "http://example.com/a");
    EXPECT_EQ(u.with_query(net::query_params()).to_string(), "http://example.com/a");
    EXPECT_EQ(u.with_query(net::query_params().add("a b", "c&d")).to_string(), "http://example.com/a?a+b=c%26d");
}

TEST(NetUrl_Tests, QueryParams) {
    auto q = net::query_params::parse("?a=1&b=x+y&a=2&c&=e&%zz=%41");
    EXPECT_EQ(q.size(), 6u);
    EXPECT_EQ(q.get("a"), "1");
    EXPECT_EQ(q.get("b"), "x y");
    EXPECT_EQ(q.get("c"), "");
    EXPECT_TRUE(q.contains("c"));
    EXPECT_FALSE(q.contains("d"));
    EXPECT_EQ(q.get(""), "e");
    EXPECT_EQ(q.get("%zz"), "A");                           // a broken escape stays as written
    auto all = q.get_all("a");
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0], "1");
    EXPECT_EQ(all[1], "2");

    q.set("a", "3");
    EXPECT_EQ(q.to_string(), "a=3&b=x+y&c=&=e&%25zz=A");
    q.erase("b").add("\xC3\xA9", "~*");
    EXPECT_EQ(q.to_string(), "a=3&c=&=e&%25zz=A&%C3%A9=%7E*");
    q.set("new", "1");
    EXPECT_EQ(q.get("new"), "1");
    EXPECT_EQ(net::query_params().to_string(), "");

    // bytes that are not UTF-8 after the unescaping become U+FFFD, one
    // for each longest broken start (the Encoding Standard)
    EXPECT_EQ(net::query_params::parse("x=%FE%FF").get("x"), "\xEF\xBF\xBD\xEF\xBF\xBD");
    EXPECT_EQ(net::query_params::parse("x=%F0%9F%98").get("x"), "\xEF\xBF\xBD");
    EXPECT_EQ(net::query_params::parse("x=%C2x").get("x"), "\xEF\xBF\xBD" "x");
    EXPECT_EQ(net::query_params::parse("x=%F0%9F%98%80").get("x"), "\xF0\x9F\x98\x80");
    // and round the other way
    auto back = net::query_params::parse(q.to_string());
    EXPECT_EQ(back, q);
}

TEST(NetUrl_Tests, TextThatIsNotUtf8) {
    // the standard's input is code points; a byte that is not UTF-8 is
    // taken as U+FFFD, which a path escapes and a host refuses
    auto u = net::url::parse("http://example.com/\xFF");
    ASSERT_TRUE(u);
    EXPECT_EQ(u->path(), "/%EF%BF%BD");
    EXPECT_FALSE(net::url::parse("http://ex\xFF" "ample.com/"));
}

TEST(NetUrl_Tests, ValueSemantics) {
    auto a = *net::url::parse("http://example.com/a");
    auto b = *net::url::parse("HTTP://EXAMPLE.com:80/./a");
    EXPECT_EQ(a, b);
    EXPECT_EQ(std::hash<net::url>()(a), std::hash<net::url>()(b));
    auto c = *net::url::parse("http://example.com/b");
    EXPECT_LT(a, c);
    EXPECT_NE(a, c);
    auto copy = a;
    EXPECT_EQ(copy.to_string(), "http://example.com/a");
}
