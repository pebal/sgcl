//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: ip_address, endpoint, ip_network. The oracle is Go's net/netip
// (tools/ip_oracle.go writes ip_oracle.h): the examples of RFC 4291 and
// RFC 5952, the edges written by hand, random addresses in several
// spellings and random one-character edits of all of them, each asked the
// same questions here as there. The cases below the oracle's are the ones
// it cannot ask: the values made without text, the zone's bound, the
// exceptions, the hash and the size.
#include "tests/types.h"
#include "sgcl/net/net.h"
#include "ip_oracle.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <string>
#include <string_view>
#include <unordered_set>

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    unsigned flags_of(const ip_address& a) {
        unsigned f = 0;
        f |= a.is_v4() ? ip_oracle::V4 : 0;
        f |= a.is_v6() ? ip_oracle::V6 : 0;
        f |= a.is_v4_mapped() ? ip_oracle::Mapped : 0;
        f |= a.is_loopback() ? ip_oracle::Loopback : 0;
        f |= a.is_private() ? ip_oracle::Private : 0;
        f |= a.is_unspecified() ? ip_oracle::Unspecified : 0;
        f |= a.is_multicast() ? ip_oracle::Multicast : 0;
        f |= a.is_link_local() ? ip_oracle::LinkLocal : 0;
        f |= a.is_global_unicast() ? ip_oracle::GlobalUnicast : 0;
        return f;
    }
}

TEST(NetIp_Tests, AddressesAgainstGo) {
    size_t parsed = 0, refused = 0, zone_too_long = 0;
    for (auto& c : ip_oracle::addresses) {
        auto a = ip_address::parse(c.input);
        if (c.flags & ip_oracle::ZoneTooLong) {   // Go keeps any zone; here fifteen bytes at most
            EXPECT_FALSE(a) << c.input;
            ++zone_too_long;
            continue;
        }
        ASSERT_EQ(bool(a), c.ok) << '"' << c.input << '"';
        if (!c.ok) {
            ++refused;
            continue;
        }
        ++parsed;
        EXPECT_EQ(text(a->to_string()), c.text) << c.input;
        EXPECT_EQ(flags_of(*a), c.flags) << c.input;
        EXPECT_EQ(text(a->next().to_string()), c.next) << c.input;
        EXPECT_EQ(text(a->prev().to_string()), c.prev) << c.input;
        auto again = ip_address::parse(a->to_string());   // the text parses back to the same value
        ASSERT_TRUE(again) << c.input;
        EXPECT_EQ(*again, *a) << c.input;
    }
    EXPECT_GT(parsed, 2000u);
    EXPECT_GT(refused, 1000u);
    EXPECT_GE(zone_too_long, 1u);
}

TEST(NetIp_Tests, EndpointsAgainstGo) {
    size_t parsed = 0;
    for (auto& c : ip_oracle::endpoints) {
        auto e = net::endpoint::parse(c.input);
        ASSERT_EQ(bool(e), c.ok) << '"' << c.input << '"';
        if (e) {
            ++parsed;
            EXPECT_EQ(text(e->to_string()), c.text) << c.input;
            EXPECT_EQ(net::endpoint::parse(e->to_string()), e) << c.input;
        }
    }
    EXPECT_GT(parsed, 300u);
}

TEST(NetIp_Tests, NetworksAgainstGo) {
    size_t parsed = 0;
    for (auto& c : ip_oracle::networks) {
        auto n = ip_network::parse(c.input);
        ASSERT_EQ(bool(n), c.ok) << '"' << c.input << '"';
        if (n) {
            ++parsed;
            EXPECT_EQ(text(n->to_string()), c.text) << c.input;
            EXPECT_EQ(text(n->masked().to_string()), c.masked) << c.input;
        }
    }
    EXPECT_GT(parsed, 300u);
    for (auto& c : ip_oracle::contains) {
        auto n = ip_network::parse(c.network);
        auto a = ip_address::parse(c.address);
        ASSERT_TRUE(n && a) << c.network << ' ' << c.address;
        EXPECT_EQ(n->contains(*a), c.result) << c.network << " contains " << c.address;
    }
    for (auto& c : ip_oracle::overlaps) {
        auto x = ip_network::parse(c.a);
        auto y = ip_network::parse(c.b);
        ASSERT_TRUE(x && y) << c.a << ' ' << c.b;
        EXPECT_EQ(x->overlaps(*y), c.result) << c.a << " overlaps " << c.b;
        EXPECT_EQ(y->overlaps(*x), c.result) << c.b << " overlaps " << c.a;
    }
}

TEST(NetIp_Tests, OrderAgainstGo) {
    size_t n = std::size(ip_oracle::ordered);
    for (size_t i = 0; i + 1 < n; ++i) {
        auto a = ip_address::parse(ip_oracle::ordered[i].address);
        auto b = ip_address::parse(ip_oracle::ordered[i + 1].address);
        ASSERT_TRUE(a && b);
        int c = ip_oracle::ordered[i].compare_to_next;
        auto r = *a <=> *b;
        EXPECT_EQ(r < 0 ? -1 : r > 0 ? 1 : 0, c) << ip_oracle::ordered[i].address << " <=> " << ip_oracle::ordered[i + 1].address;
    }
}

// A text that is not an address, a network, an endpoint or a URL is an
// error with the reason and the text, not an empty optional
TEST(NetIp_Tests, ParseGivesTheReason) {
    auto a = ip_address::parse("10.0.0.x");
    ASSERT_FALSE(a);
    EXPECT_EQ(a.error().code(), errc::invalid_address);
    EXPECT_EQ(a.error().message(), "parse IP address 10.0.0.x: invalid address");
    EXPECT_EQ(ip_network::parse("10.0.0.0/33").error().code(), errc::invalid_address);
    EXPECT_EQ(endpoint::parse("10.0.0.1").error().code(), errc::invalid_address);   // no port
    auto u = url::parse("http://[::1");
    ASSERT_FALSE(u);
    EXPECT_EQ(u.error().code(), errc::invalid_url);
    EXPECT_EQ(u.error().message(), "parse URL http://[::1: invalid URL");
    EXPECT_TRUE(ip_address::parse("10.0.0.1"));
    // a setter the standard declines: the error, with the setter and the value
    auto mail = *url::parse("mailto:a@b");
    auto h = mail.with_host("example.com");                  // an opaque path: no host
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().code(), errc::invalid_url);
    EXPECT_EQ(h.error().message(), "set URL host example.com: invalid URL");
    EXPECT_EQ(mail.with_password("secret").error().message(), "set URL password: invalid URL");   // never the password
    EXPECT_EQ(url::parse("http://x/")->with_scheme("https")->to_string(), "https://x/");
}

TEST(NetIp_Tests, ValuesWithoutText) {
    static_assert(sizeof(ip_address) == 32);
    static_assert(std::is_trivially_copyable_v<ip_address>);
    static_assert(std::is_trivially_copyable_v<net::endpoint>);
    static_assert(std::is_trivially_copyable_v<ip_network>);
    ip_address none;
    EXPECT_FALSE(none.is_valid());
    EXPECT_EQ(text(none.to_string()), "invalid IP");
    EXPECT_FALSE(none.next().is_valid());
    EXPECT_FALSE(none.prev().is_valid());
    EXPECT_LT(none, ip_address::any_v4());
    EXPECT_LT(ip_address::loopback_v4(), ip_address::any_v6());   // every IPv4 address before every IPv6 one
    EXPECT_EQ(text(ip_address::v4(10, 0, 0, 1).to_string()), "10.0.0.1");
    EXPECT_EQ(text(ip_address::loopback_v6().to_string()), "::1");
    EXPECT_EQ(text(ip_address::any_v6().to_string()), "::");
    EXPECT_EQ(text(ip_address::any_v4().to_string()), "0.0.0.0");
    array<uint8_t, 16> b = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    auto v6 = ip_address::v6(b);
    EXPECT_EQ(text(v6.to_string()), "2001:db8::1");
    EXPECT_EQ(v6.bytes(), b);
    auto v4 = ip_address::v4(1, 2, 3, 4);
    array<uint8_t, 16> mapped = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 1, 2, 3, 4};
    EXPECT_EQ(v4.bytes(), mapped);                                   // the form a dual-stack socket sees
    EXPECT_NE(ip_address::v6(mapped), v4);                            // two values, as in Go
    EXPECT_EQ(ip_address::v6(mapped).unmap(), v4);
    EXPECT_EQ(v4.unmap(), v4);
}

TEST(NetIp_Tests, Zones) {
    auto a = *ip_address::parse("fe80::1%en0");
    EXPECT_EQ(text(a.zone()), "en0");
    EXPECT_TRUE(a.has_zone());
    EXPECT_NE(a, *ip_address::parse("fe80::1"));
    EXPECT_LT(*ip_address::parse("fe80::1"), a);                     // no zone first
    auto b = a.with_zone("utun4");
    EXPECT_EQ(text(b.to_string()), "fe80::1%utun4");
    EXPECT_EQ(a.with_zone("").zone(), sgcl::string());
    EXPECT_FALSE(a.with_zone("").has_zone());
    EXPECT_EQ(text(a.with_zone("123456789012345").to_string()), "fe80::1%123456789012345");   // fifteen bytes: the bound
    EXPECT_THROW(a.with_zone("1234567890123456"), std::invalid_argument);
    EXPECT_THROW(a.with_zone(sgcl::string(std::string_view("a\0b", 3))), std::invalid_argument);
    EXPECT_EQ(ip_address::v4(1, 2, 3, 4).with_zone("en0"), ip_address::v4(1, 2, 3, 4));   // IPv4 has none
    EXPECT_FALSE(ip_address::parse(sgcl::string(std::string_view("fe80::1%a\0b", 11))));
    auto e = net::endpoint(a, 80);
    EXPECT_EQ(text(e.to_string()), "[fe80::1%en0]:80");
    auto odd = net::endpoint::parse("[fe80::1%a]b]:80");   // a zone may hold a bracket, as in Go: it round-trips
    ASSERT_TRUE(odd);
    EXPECT_EQ(text(odd->address().zone()), "a]b");
    EXPECT_EQ(net::endpoint::parse(odd->to_string()), odd);
    EXPECT_EQ(ip_network(a, 64).address(), *ip_address::parse("fe80::1"));   // a network has no zone
    EXPECT_FALSE(ip_network(*ip_address::parse("fe80::"), 10).contains(a));   // nor matches an address with one
}

TEST(NetIp_Tests, NetworkConstruction) {
    EXPECT_THROW(ip_network(ip_address::v4(10, 0, 0, 0), 33), std::invalid_argument);
    EXPECT_THROW(ip_network(ip_address::v4(10, 0, 0, 0), -1), std::invalid_argument);
    EXPECT_THROW(ip_network(ip_address::any_v6(), 129), std::invalid_argument);
    EXPECT_NO_THROW(ip_network(ip_address::any_v6(), 128));
    ip_network empty(ip_address(), 7);
    EXPECT_FALSE(empty.is_valid());
    EXPECT_EQ(empty.bits(), -1);
    EXPECT_EQ(text(empty.to_string()), "invalid Prefix");
    EXPECT_FALSE(empty.contains(ip_address::any_v4()));
    auto n = ip_network(ip_address::v4(192, 168, 7, 9), 20);
    EXPECT_EQ(text(n.to_string()), "192.168.7.9/20");
    EXPECT_EQ(text(n.masked().to_string()), "192.168.0.0/20");
    EXPECT_TRUE(n.contains(ip_address::v4(192, 168, 15, 255)));
    EXPECT_FALSE(n.contains(ip_address::v4(192, 168, 16, 0)));
    EXPECT_FALSE(n.contains(*ip_address::parse("::ffff:192.168.1.1")));   // the mapped form is IPv6
    EXPECT_EQ(net::endpoint(), net::endpoint());
    EXPECT_EQ(text(net::endpoint().to_string()), "invalid AddrPort");
}

TEST(NetIp_Tests, Hashes) {
    std::unordered_set<ip_address> set;
    for (auto& c : ip_oracle::addresses) {
        if (c.ok && !(c.flags & ip_oracle::ZoneTooLong)) {
            set.insert(*ip_address::parse(c.input));
        }
    }
    for (auto& c : ip_oracle::addresses) {
        if (c.ok && !(c.flags & ip_oracle::ZoneTooLong)) {
            EXPECT_TRUE(set.count(*ip_address::parse(c.text))) << c.input;   // equal values, equal hashes
        }
    }
    std::unordered_set<net::endpoint> eps = {net::endpoint(ip_address::loopback_v4(), 1), net::endpoint(ip_address::loopback_v4(), 2)};
    EXPECT_EQ(eps.size(), 2u);
    std::unordered_set<ip_network> nets = {*ip_network::parse("10.0.0.0/8"), *ip_network::parse("10.0.0.0/9")};
    EXPECT_EQ(nets.size(), 2u);
}

// Random text through the three parsers, the alphabet of addresses and a
// few bytes that are not (NUL, a space, high bytes): nothing read past the
// input (the test runs under the address sanitizer too), and whatever is
// accepted writes a text that parses back to the same value
TEST(NetIp_Tests, RandomTextThroughTheParsers) {
    static const char alphabet[] = "0123456789abcdefABCDEF:.%[]/ -+xg\0\xff\x80";
    uint64_t x = 0x9e3779b97f4a7c15ull;
    auto next = [&] {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
    };
    size_t accepted = 0;
    for (int i : range(200000)) {
        (void)i;
        size_t n = next() % 48;
        std::string s;
        for (size_t k = 0; k < n; ++k) {
            s += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        sgcl::string text(s);
        if (auto a = ip_address::parse(text)) {
            ++accepted;
            EXPECT_EQ(ip_address::parse(a->to_string()), a) << s;
        }
        if (auto e = net::endpoint::parse(text)) {
            EXPECT_EQ(net::endpoint::parse(e->to_string()), e) << s;
        }
        if (auto w = ip_network::parse(text)) {
            EXPECT_EQ(ip_network::parse(w->to_string()), w) << s;
        }
    }
    EXPECT_GT(accepted, 0u);
}
