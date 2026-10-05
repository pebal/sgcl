//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net's values at their boundaries (DESIGN 408): ip_address, ip_network,
// endpoint, url and query_params. The parsers' edges are the oracles'
// (ip.cpp: Go's net/netip, the ports 0 and 65535, the prefixes /0 and
// /128, the zone's fifteen bytes; url.cpp: the WHATWG tests, the 512 MiB
// limit); here what they cannot ask: the longest texts written into their
// fixed buffers, a value moved from, an argument that is a part of the
// object itself.
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <string>
#include <utility>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }
}

// The longest text of each value fills its fixed buffer exactly (MaxText):
// eight groups of four digits, a zone of fifteen bytes, the port 65535, the
// prefix /128; to_string, write_text and the formatter agree, and the text
// parses back (the sanitizer build reads every byte)
TEST(NetBounds_Tests, TheLongestTextsFillTheirBuffers) {
    auto a = net::ip_address("1111:2222:3333:4444:5555:6666:7777:8888%123456789012345");
    EXPECT_EQ(a.to_string().size(), 55u);
    net::endpoint e(a, 65535);
    EXPECT_EQ(text(e.to_string()), "[1111:2222:3333:4444:5555:6666:7777:8888%123456789012345]:65535");
    EXPECT_EQ(e.to_string().size(), net::endpoint::MaxText);
    char buf[net::endpoint::MaxText];
    EXPECT_EQ(e.write_text(buf), net::endpoint::MaxText);
    EXPECT_EQ(net::endpoint::parse(e.to_string()), e);
    net::ip_network n(net::ip_address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"), 128);
    EXPECT_EQ(text(n.to_string()), "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128");
    EXPECT_EQ(net::ip_network::parse(n.to_string()), n);
    EXPECT_EQ(text(txt::format("{}", e)), text(e.to_string()));
    EXPECT_EQ(text(txt::format("{:>66}", e)), "   " + text(e.to_string()));
    EXPECT_EQ(text(txt::format("{:*<45}", n)), text(n.to_string()) + "**");
    EXPECT_EQ(text(txt::format("{:^14}", net::ip_address())), "  invalid IP  ");
    // the ends of a port and of the numbers in a text
    EXPECT_EQ(text(net::endpoint(net::ip_address::any_v4(), 0).to_string()), "0.0.0.0:0");
    EXPECT_EQ(net::endpoint::parse("0.0.0.0:00000000065535")->port(), 65535);   // leading zeros, as the page says
    EXPECT_FALSE(net::endpoint::parse("1.2.3.4:4294967376"));                     // 2^32 + 80: no wrap to 80
    EXPECT_FALSE(net::ip_network::parse("10.0.0.0/4294967304"));                 // 2^32 + 8
    EXPECT_FALSE(net::ip_address::parse("4294967297.0.0.1"));
    EXPECT_FALSE(net::ip_address::parse("1:2:3:4:5:6:7:10000"));
}

// An address's own zone given back, a network made of its own address, an
// endpoint of its own address: values, nothing shared
TEST(NetBounds_Tests, ValuesOfThemselves) {
    auto a = net::ip_address("fe80::1%lo0");
    EXPECT_EQ(a.with_zone(a.zone()), a);
    net::ip_network n(net::ip_address("10.1.2.3"), 8);
    n = net::ip_network(n.masked().address(), n.bits());
    EXPECT_EQ(text(n.to_string()), "10.0.0.0/8");
    EXPECT_TRUE(n.overlaps(n));
    EXPECT_TRUE(n.contains(n.address()));
    net::ip_network all(net::ip_address::any_v6(), 0);
    EXPECT_TRUE(all.contains(net::ip_address("ffff::1")));
    EXPECT_TRUE(all.overlaps(net::ip_network(net::ip_address("::1"), 128)));
    EXPECT_FALSE(all.contains(net::ip_address::any_v4()));                       // another kind
    EXPECT_EQ(net::ip_network(net::ip_address("255.255.255.255"), 0).masked(), net::ip_network(net::ip_address::any_v4(), 0));
    EXPECT_EQ(net::ip_network(net::ip_address("255.255.255.255"), 32).masked().address(), net::ip_address("255.255.255.255"));
    // the ends of the kinds: next and prev keep the zone, and past the end is the empty address
    EXPECT_EQ(net::ip_address("fe80::ffff%lo0").next(), net::ip_address("fe80::1:0%lo0"));
    EXPECT_FALSE(net::ip_address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff%lo0").next().is_valid());
    EXPECT_FALSE(net::ip_address("::%lo0").prev().is_valid());
}

// A URL moved from is still the URL: its text is a string, whose move
// copies the word (as tracked_ptr's), and the offsets are numbers
TEST(NetBounds_Tests, AUrlMovedFromIsTheUrl) {
    auto u = net::url("https://user:pw@example.com:8443/a/b?q=1#f");
    auto moved = std::move(u);
    EXPECT_EQ(u, moved);
    EXPECT_EQ(text(u.to_string()), "https://user:pw@example.com:8443/a/b?q=1#f");
    EXPECT_EQ(u.port(), 8443);
    EXPECT_EQ(text(u.path()), "/a/b");
    EXPECT_EQ(text(u.query_params().get("q")), "1");
    auto other = net::url("http://x/");
    other = std::move(u);
    EXPECT_EQ(other, moved);
    EXPECT_EQ(u, moved);
}

// Each part of a URL given back to its setter, the URL to its own
// reference and as its own base: the same URL
TEST(NetBounds_Tests, AUrlOfItsOwnParts) {
    auto u = net::url("https://user:pw@example.com:8443/a/b%20c?q=1&r=x+y#frag");
    EXPECT_EQ(*u.with_scheme(u.scheme()), u);
    EXPECT_EQ(*u.with_username(u.username()), u);
    EXPECT_EQ(*u.with_password(u.password()), u);
    EXPECT_EQ(*u.with_host(u.host()), u);
    EXPECT_EQ(*u.with_hostname(u.hostname()), u);
    EXPECT_EQ(*u.with_port(u.port()), u);
    EXPECT_EQ(*u.with_path(u.path()), u);
    EXPECT_EQ(*u.with_query(u.query()), u);
    EXPECT_EQ(*u.with_query(u.query_params()), u);
    EXPECT_EQ(*u.with_fragment(u.fragment()), u);
    EXPECT_EQ(*u.resolve(u.to_string()), u);
    EXPECT_EQ(*net::url::parse(u.to_string(), u), u);
    EXPECT_EQ(*u.resolve(""), u.without_fragment());                             // the empty reference: the base without its fragment
    // the ends of the port: 0 and 65535 kept, 65536 refused
    EXPECT_EQ(text(u.with_port(0)->to_string()), "https://user:pw@example.com:0/a/b%20c?q=1&r=x+y#frag");
    EXPECT_EQ(u.with_port(0)->effective_port(), 0);
    EXPECT_EQ(u.with_port(65535)->port(), 65535);
    EXPECT_EQ(net::url("http://x:0/").port(), 0);
    EXPECT_EQ(net::url("http://x:00065535/").port(), 65535);
    EXPECT_FALSE(net::url::parse("http://x:4294967376/"));                        // 2^32 + 80
    EXPECT_EQ(net::url("http://x/").effective_port(), 80);
    EXPECT_FALSE(net::url::parse(""));
    EXPECT_EQ(*net::url::parse("", u), u.without_fragment());
}

// A list of pairs moved from is empty, its written length too: to_string
// writes "", and a pair added after is the whole list (the length was kept
// before, and to_string wrote bytes that no pair gave)
TEST(NetBounds_Tests, PairsMovedFromAreEmpty) {
    auto q = net::query_params("a=1&bb=22");
    auto moved = std::move(q);
    EXPECT_EQ(text(moved.to_string()), "a=1&bb=22");
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(text(q.to_string()), "");
    EXPECT_EQ(net::detail::UrlAccess::written_size(q), 0u);
    ASSERT_TRUE(q.add("x", "1"));
    EXPECT_EQ(text(q.to_string()), "x=1");
    auto target = net::query_params("c=3");
    target = std::move(moved);
    EXPECT_EQ(text(target.to_string()), "a=1&bb=22");
    EXPECT_TRUE(moved.empty());
    EXPECT_EQ(text(moved.to_string()), "");
    ASSERT_TRUE(moved.set("y", "2"));
    EXPECT_EQ(text(moved.to_string()), "y=2");
    auto& self = target;
    target = std::move(self);
    EXPECT_EQ(text(target.to_string()), "a=1&bb=22");
    EXPECT_EQ(net::detail::UrlAccess::written_size(target), 9u);
    target = target;
    EXPECT_EQ(text(target.to_string()), "a=1&bb=22");
}

// A name or a value that is a pair of the list itself, the list changed
// under it: add, set and erase read it before they change the list
TEST(NetBounds_Tests, PairsOfThemselves) {
    auto q = net::query_params("a=1&b=2&a=3");
    ASSERT_TRUE(q.add(q.begin()->first, q.begin()->second));
    EXPECT_EQ(text(q.to_string()), "a=1&b=2&a=3&a=1");
    ASSERT_TRUE(q.set(q.begin()->first, (q.begin() + 1)->second));
    EXPECT_EQ(text(q.to_string()), "a=2&b=2");
    ASSERT_TRUE(q.set((q.begin() + 1)->first, q.begin()->first));
    EXPECT_EQ(text(q.to_string()), "a=2&b=a");
    q.erase(q.begin()->first);
    EXPECT_EQ(text(q.to_string()), "b=a");
    q.erase(q.begin()->first);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(text(q.to_string()), "");
    EXPECT_EQ(net::detail::UrlAccess::written_size(q), 0u);
}

// The categories at their ends: every errc worded, a number outside them
// said to be unknown rather than read past a table; an EAI_* code in the
// lookup category, not misread as errno
TEST(NetBounds_Tests, TheCategoriesAtTheirEnds) {
    for (int c = int(net::errc::invalid_address); c <= int(net::errc::sftp_protocol); ++c) {
        auto code = net::make_error_code(net::errc(c));
        EXPECT_EQ(&code.category(), &net::category());
        EXPECT_NE(code.message(), "unknown net error") << c;
    }
    EXPECT_EQ(net::category().message(0), "unknown net error");
    EXPECT_EQ(net::category().message(int(net::errc::sftp_protocol) + 1), "unknown net error");
    EXPECT_EQ(net::category().message(-1), "unknown net error");
    EXPECT_STREQ(net::category().name(), "net");
    EXPECT_STREQ(net::lookup_category().name(), "lookup");
    EXPECT_FALSE(net::lookup_category().message(EAI_AGAIN).empty());
    EXPECT_FALSE(net::lookup_category().message(-12345).empty());
    error_code e = net::errc::host_not_found;   // the enum converts
    EXPECT_EQ(e, net::make_error_code(net::errc::host_not_found));
    EXPECT_NE(e, error_code(int(net::errc::host_not_found), std::system_category()));
}

// Empty, one and nothing: the texts with no pair, a pair of nothing, the
// lookups and the changes of an empty list
TEST(NetBounds_Tests, PairsEmptyAndOne) {
    for (auto t : {"", "?", "&", "?&&&", "??"}) {
        auto p = net::query_params::parse(t);
        ASSERT_TRUE(p) << t;
        EXPECT_EQ(p->size(), std::string_view(t) == "??" ? 1u : 0u) << t;   // "??": the second '?' is a name
    }
    auto one = *net::query_params::parse("=");
    ASSERT_EQ(one.size(), 1u);
    EXPECT_TRUE(one.contains(""));
    EXPECT_EQ(text(one.to_string()), "=");
    net::query_params none;
    EXPECT_EQ(none.get("a"), "");
    EXPECT_TRUE(none.get_all("a").empty());
    EXPECT_FALSE(none.contains(""));
    EXPECT_EQ(&none.erase("a"), &none);
    EXPECT_TRUE(none.begin() == none.end());
    EXPECT_EQ(none, net::query_params::parse("").value());
    ASSERT_TRUE(none.set("", ""));
    EXPECT_EQ(text(none.to_string()), "=");
    EXPECT_EQ(*net::query_params::first("", ""), "");
    EXPECT_EQ(*net::query_params::first("=", ""), "");
    EXPECT_EQ(*net::query_params::first("a", "a"), "");
}
