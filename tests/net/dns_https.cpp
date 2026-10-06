//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dns's HTTPS and SVCB records (RFC 9460: lookup_https, lookup_svcb and
// their task forms) against the DNS server of tests/net/dns_server.h on the
// loopback: the wire vectors of RFC 9460 Appendix D, every SvcParam typed
// (mandatory, alpn, no-default-alpn, port, ipv4hint, ech, ipv6hint, dohpath)
// and the others kept raw; AliasMode followed (a chain, its bound, a loop,
// an alias of ".", an alias to a name without records, an alias beside
// ServiceMode records, an alias's SvcParams ignored); ServiceMode's "."
// made its owner, behind a CNAME too; the order by priority, a priority's
// records shuffled; every malformed form of §2.2, §7 and §8 rejecting the
// whole RRset; the failures, the stop, TCP for a truncated answer. The
// SvcParams reader alone (net/detail/dns_message.h) on the same vectors, and
// against Go's dns/dnsmessage (tools/svcb_oracle.go) on random messages.
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/dns_server.h"

#include "sgcl/io/exec.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
    using namespace dns_test;
    using sgcl::net::dns;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::string bytes_text(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // Wire pieces: a name, a 16-bit number, a SvcParam
    std::string name_wire(const std::string& text) {
        nd::DnsName n = wire(text);
        return std::string(reinterpret_cast<const char*>(n.bytes), n.size);
    }

    std::string u16(unsigned v) {
        return std::string{char(v >> 8), char(v & 0xFF)};
    }

    std::string param(unsigned key, const std::string& value) {
        return u16(key) + u16(unsigned(value.size())) + value;
    }

    std::string unhex(const std::string& h) {
        std::string out;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            out += char(std::stoi(h.substr(i, 2), nullptr, 16));
        }
        return out;
    }

    std::string hex(const std::string& b) {
        static const char* d = "0123456789abcdef";
        std::string out;
        for (unsigned char c : b) {
            out += d[c >> 4];
            out += d[c & 15];
        }
        return out;
    }

    // An SVCB/HTTPS rdata: the priority, the target, the SvcParams
    std::string rdata(unsigned priority, const std::string& target, const std::string& params = "") {
        return u16(priority) + name_wire(target) + params;
    }

    Rr rr_svc(const std::string& owner, uint16_t t, const std::string& data) {
        Rr r;
        r.owner = owner;
        r.type = t;
        r.raw = data;
        return r;
    }

    Rr rr_https(const std::string& owner, const std::string& data) {
        return rr_svc(owner, type::https, data);
    }

    Rr rr_svcb(const std::string& owner, const std::string& data) {
        return rr_svc(owner, type::svcb, data);
    }

    std::vector<std::string> names(const vector<string>& v) {
        std::vector<std::string> out;
        for (auto& s : v) {
            out.push_back(str(s));
        }
        return out;
    }

    // the address bytes of a hint
    std::string address_bytes(const sgcl::net::ip_address& a) {
        auto b = a.bytes();
        return a.is_v4() ? std::string(reinterpret_cast<const char*>(b.data()) + 12, 4) : std::string(reinterpret_cast<const char*>(b.data()), 16);
    }
}

// RFC 9460 Appendix D: the wire forms of the presentation examples, each
// read back as the example says
TEST(NetDnsHttps_Tests, TheRfcVectors) {
    Behaviour b;
    const std::string foo_com = "foo.example.com.";
    b.zone = {
        // D.2, figure 2: "SVCB 1 ." — the owner's name is the target
        rr_svcb("d2a.example.test.", unhex("000100")),
        // figure 3: "SVCB 16 foo.example.com. port=53"
        rr_svcb("d2b.example.test.", unhex("0010") + name_wire(foo_com) + unhex("000300020035")),
        // figure 4: "SVCB 1 foo.example.com. key667=hello"
        rr_svcb("d2c.example.test.", unhex("0001") + name_wire(foo_com) + unhex("029b000568656c6c6f")),
        // figure 5: key667="hello\210qoo"
        rr_svcb("d2d.example.test.", unhex("0001") + name_wire(foo_com) + unhex("029b000968656c6c6fd2716f6f")),
        // figure 6: ipv6hint="2001:db8::1,2001:db8::53:1"
        rr_svcb("d2e.example.test.", unhex("0001") + name_wire(foo_com) +
                                         unhex("0006002020010db800000000000000000000000120010db8000000000000000000530001")),
        // figure 7: ipv6hint="2001:db8:122:344::192.0.2.33"
        rr_svcb("d2f.example.test.", unhex("0001") + name_wire(foo_com) + unhex("0006001020010db80122034400000000c0000221")),
        // figure 8: "SVCB 16 foo.example.org. alpn=h2,h3-19 mandatory=ipv4hint,alpn ipv4hint=192.0.2.1"
        rr_svcb("d2g.example.test.", unhex("0010") + name_wire("foo.example.org.") + unhex("0000000400010004") +
                                         unhex("000100090268320568332d3139") + unhex("00040004c0000201")),
        // figure 9: alpn="f\\\\oo\\,bar,h2": the ids "f\oo,bar" and "h2"
        rr_svcb("d2h.example.test.", unhex("0010") + name_wire("foo.example.org.") + unhex("0001000c08665c6f6f2c626172026832")),
    };
    Server s(b);
    ASSERT_TRUE(s.ok());
    auto one = [&](const char* name) {
        auto r = dns::lookup_svcb(name, s.options());
        EXPECT_TRUE(r.has_value()) << name << ": " << (r ? "" : str(r.error().message()));
        EXPECT_EQ(r ? r->size() : 0u, 1u) << name;
        return r && r->size() == 1 ? (*r)[0] : dns::svcb();
    };
    auto a = one("d2a.example.test.");
    EXPECT_EQ(a.priority, 1);
    EXPECT_EQ(str(a.target), "d2a.example.test.");   // "." is the owner (§2.5.2)
    EXPECT_EQ(a, (dns::svcb{.priority = 1, .target = "d2a.example.test."}));

    auto p = one("d2b.example.test.");
    EXPECT_EQ(p.priority, 16);
    EXPECT_EQ(str(p.target), foo_com);
    EXPECT_EQ(p.port, 53);
    EXPECT_TRUE(p.params.empty());

    auto k = one("d2c.example.test.");
    ASSERT_EQ(k.params.size(), 1u);
    EXPECT_EQ(k.params[0].key, 667);
    EXPECT_EQ(bytes_text(k.params[0].value), "hello");
    auto k2 = one("d2d.example.test.");
    ASSERT_EQ(k2.params.size(), 1u);
    EXPECT_EQ(bytes_text(k2.params[0].value), std::string("hello\xd2qoo"));

    auto h = one("d2e.example.test.");
    ASSERT_EQ(h.ipv6_hints.size(), 2u);
    EXPECT_EQ(str(h.ipv6_hints[0].to_string()), "2001:db8::1");
    EXPECT_EQ(str(h.ipv6_hints[1].to_string()), "2001:db8::53:1");
    auto h2 = one("d2f.example.test.");
    ASSERT_EQ(h2.ipv6_hints.size(), 1u);
    EXPECT_EQ(address_bytes(h2.ipv6_hints[0]), unhex("20010db80122034400000000c0000221"));

    auto m = one("d2g.example.test.");
    EXPECT_EQ(m.priority, 16);
    EXPECT_EQ(str(m.target), "foo.example.org.");
    EXPECT_EQ(std::vector<uint16_t>(m.mandatory.begin(), m.mandatory.end()), (std::vector<uint16_t>{1, 4}));
    EXPECT_EQ(names(m.alpn), (std::vector<std::string>{"h2", "h3-19"}));
    ASSERT_EQ(m.ipv4_hints.size(), 1u);
    EXPECT_EQ(str(m.ipv4_hints[0].to_string()), "192.0.2.1");

    auto e = one("d2h.example.test.");
    EXPECT_EQ(names(e.alpn), (std::vector<std::string>{"f\\oo,bar", "h2"}));
}

// Every key typed, the others raw in their order; HTTPS and SVCB apart
TEST(NetDnsHttps_Tests, EveryParam) {
    Behaviour b;
    const std::string ech = unhex("0045fe0d0041c100200020") + std::string(32, 'k') + unhex("0004000100010012") + "cloudflare-ech.com" + unhex("0000");
    const std::string params = param(0, u16(3) + u16(5)) + param(1, "\x02h2\x02h3") + param(2, "") + param(3, u16(8443)) +
                               param(4, unhex("c0000201c0000202")) + param(5, ech) + param(6, unhex("20010db8000000000000000000000001")) +
                               param(7, "/dns-query{?dns}") + param(8, "") + param(9, u16(29)) + param(65280, "private");
    b.zone = {
        rr_https("all.example.test.", rdata(1, "svc.example.test.", params)),
        rr_svcb("all.example.test.", rdata(2, "other.example.test.")),
        rr_a("svc.example.test.", 192, 0, 2, 1),
    };
    Server s(b);
    ASSERT_TRUE(s.ok());
    auto r = dns::lookup_https("all.example.test.", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 1u);
    const dns::svcb& x = (*r)[0];
    EXPECT_EQ(x.priority, 1);
    EXPECT_EQ(str(x.target), "svc.example.test.");
    EXPECT_EQ(std::vector<uint16_t>(x.mandatory.begin(), x.mandatory.end()), (std::vector<uint16_t>{3, 5}));
    EXPECT_EQ(names(x.alpn), (std::vector<std::string>{"h2", "h3"}));
    EXPECT_TRUE(x.no_default_alpn);
    EXPECT_EQ(x.port, 8443);
    ASSERT_EQ(x.ipv4_hints.size(), 2u);
    EXPECT_EQ(str(x.ipv4_hints[1].to_string()), "192.0.2.2");
    ASSERT_EQ(x.ipv6_hints.size(), 1u);
    EXPECT_EQ(str(x.ipv6_hints[0].to_string()), "2001:db8::1");
    EXPECT_EQ(bytes_text(x.ech), ech);
    EXPECT_EQ(str(x.dohpath), "/dns-query{?dns}");
    ASSERT_EQ(x.params.size(), 3u);
    EXPECT_EQ(x.params[0].key, 8);
    EXPECT_TRUE(x.params[0].value.empty());
    EXPECT_EQ(x.params[1].key, 9);
    EXPECT_EQ(bytes_text(x.params[1].value), u16(29));
    EXPECT_EQ(x.params[2], (dns::svc_param{65280, vector<byte>(reinterpret_cast<const byte*>("private"), reinterpret_cast<const byte*>("private") + 7)}));
    // the same name's SVCB is another record
    auto v = dns::lookup_svcb("all.example.test.", s.options());
    ASSERT_TRUE(v.has_value());
    ASSERT_EQ(v->size(), 1u);
    EXPECT_EQ((*v)[0], (dns::svcb{.priority = 2, .target = "other.example.test."}));
    // the task forms
    auto t = dns::async_lookup_https("all.example.test.", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ((*t)[0], x);
    auto t2 = dns::async_lookup_svcb("all.example.test.", s.options()).wait();
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(t2->size(), 1u);
    EXPECT_EQ(dns::svcb(), dns::svcb());
    EXPECT_FALSE((*t)[0] == (*v)[0]);
}

// AliasMode (§2.4.2): followed to its target's records; a chain of eight,
// a ninth too many; a loop ends at the bound; "." is no service; an alias
// to a name without records gives that name; an alias in an RRset hides the
// ServiceMode records beside it; an alias's SvcParams are not read
TEST(NetDnsHttps_Tests, AliasMode) {
    Behaviour b;
    b.zone = {
        rr_https("alias.example.test.", rdata(0, "svc.example.test.")),
        rr_https("svc.example.test.", rdata(1, ".", param(1, "\x02h2"))),
        rr_https("svc.example.test.", rdata(2, "backup.example.test.")),
        rr_https("dot.example.test.", rdata(0, ".")),
        rr_https("bare.example.test.", rdata(0, "host.example.test.")),
        rr_a("host.example.test.", 192, 0, 2, 1),
        rr_https("gone.example.test.", rdata(0, "nowhere.example.test.")),
        rr_https("mixed.example.test.", rdata(1, "ignored.example.test.")),
        rr_https("mixed.example.test.", rdata(0, "svc.example.test.")),
        rr_https("garbage.example.test.", rdata(0, "svc.example.test.", param(3, "x") + param(1, ""))),   // malformed, ignored
        rr_https("loop1.example.test.", rdata(0, "loop2.example.test.")),
        rr_https("loop2.example.test.", rdata(0, "loop1.example.test.")),
        rr_named("cname.example.test.", type::cname, "alias.example.test."),
    };
    for (int i = 0; i < 9; ++i) {   // c0 -> c1 -> ... -> c9 (a ServiceMode record)
        b.zone.push_back(rr_https("c" + std::to_string(i) + ".example.test.", rdata(0, "c" + std::to_string(i + 1) + ".example.test.")));
    }
    b.zone.push_back(rr_https("c9.example.test.", rdata(1, ".")));
    Server s(b);
    ASSERT_TRUE(s.ok());

    auto r = dns::lookup_https("alias.example.test.", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ(str((*r)[0].target), "svc.example.test.");   // the "." of the alias's target: its name
    EXPECT_EQ(names((*r)[0].alpn), (std::vector<std::string>{"h2"}));
    EXPECT_EQ(str((*r)[1].target), "backup.example.test.");
    // behind a CNAME, the same
    auto c = dns::lookup_https("cname.example.test.", s.options());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, *r);
    // beside ServiceMode records: the alias wins
    auto mixed = dns::lookup_https("mixed.example.test.", s.options());
    ASSERT_TRUE(mixed.has_value());
    EXPECT_EQ(str((*mixed)[0].target), "svc.example.test.");
    // its SvcParams are ignored, not checked
    auto garbage = dns::lookup_https("garbage.example.test.", s.options());
    ASSERT_TRUE(garbage.has_value()) << str(garbage.error().message());
    EXPECT_EQ(garbage->size(), 2u);
    // "." : the service is not offered
    auto dot = dns::lookup_https("dot.example.test.", s.options());
    ASSERT_FALSE(dot.has_value());
    EXPECT_EQ(dot.error().code(), net::errc::no_data);
    EXPECT_EQ(str(dot.error().path()), "dot.example.test.");
    // to a name with no HTTPS records: that name, priority 0
    auto bare = dns::lookup_https("bare.example.test.", s.options());
    ASSERT_TRUE(bare.has_value());
    ASSERT_EQ(bare->size(), 1u);
    EXPECT_EQ((*bare)[0], (dns::svcb{.priority = 0, .target = "host.example.test."}));
    // to a name that does not exist: the lookup's error, the name asked
    auto gone = dns::lookup_https("gone.example.test.", s.options());
    ASSERT_FALSE(gone.has_value());
    EXPECT_EQ(gone.error().code(), net::errc::host_not_found);
    EXPECT_EQ(str(gone.error().path()), "gone.example.test.");
    // eight aliases followed, nine not
    auto eight = dns::lookup_https("c1.example.test.", s.options());
    ASSERT_TRUE(eight.has_value()) << str(eight.error().message());
    EXPECT_EQ(str((*eight)[0].target), "c9.example.test.");
    auto nine = dns::lookup_https("c0.example.test.", s.options());
    ASSERT_FALSE(nine.has_value());
    EXPECT_EQ(nine.error().code(), net::errc::server_misbehaving);
    auto loop = dns::lookup_https("loop1.example.test.", s.options());
    ASSERT_FALSE(loop.has_value());
    EXPECT_EQ(loop.error().code(), net::errc::server_misbehaving);
}

// By priority, the lowest first; the records of one priority in both orders
TEST(NetDnsHttps_Tests, Order) {
    Behaviour b;
    b.zone = {
        rr_https("order.example.test.", rdata(3, "c.example.test.")),
        rr_https("order.example.test.", rdata(1, "a1.example.test.")),
        rr_https("order.example.test.", rdata(65535, "z.example.test.")),
        rr_https("order.example.test.", rdata(1, "a2.example.test.")),
        rr_https("order.example.test.", rdata(2, "b.example.test.")),
    };
    Server s(b);
    ASSERT_TRUE(s.ok());
    std::set<std::string> firsts;
    for (int i = 0; i < 60 && firsts.size() < 2; ++i) {
        auto r = dns::lookup_https("order.example.test.", s.options());
        ASSERT_TRUE(r.has_value());
        ASSERT_EQ(r->size(), 5u);
        EXPECT_EQ((*r)[0].priority, 1);
        EXPECT_EQ((*r)[1].priority, 1);
        EXPECT_EQ(str((*r)[2].target), "b.example.test.");
        EXPECT_EQ(str((*r)[3].target), "c.example.test.");
        EXPECT_EQ((*r)[4].priority, 65535);
        firsts.insert(str((*r)[0].target));
    }
    EXPECT_EQ(firsts, (std::set<std::string>{"a1.example.test.", "a2.example.test."}));
}

// A malformed record (§2.2) rejects the whole RRset: server_misbehaving
TEST(NetDnsHttps_Tests, MalformedRejectsTheSet) {
    const std::string t = "svc.example.test.";
    const std::vector<std::pair<std::string, std::string>> bad = {
        {"order", rdata(1, t, param(3, u16(443)) + param(1, "\x02h2"))},
        {"duplicate", rdata(1, t, param(3, u16(443)) + param(3, u16(444)))},
        {"past", rdata(1, t, u16(3) + u16(4) + u16(443))},
        {"cut-key", rdata(1, t, std::string("\x00", 1))},
        {"cut-length", rdata(1, t, u16(3) + std::string("\x00", 1))},
        {"alpn-empty", rdata(1, t, param(1, ""))},
        {"alpn-zero-id", rdata(1, t, param(1, std::string("\x00\x02h2", 4)))},
        {"alpn-short", rdata(1, t, param(1, "\x03h2"))},
        {"no-default-value", rdata(1, t, param(1, "\x02h2") + param(2, "x"))},
        {"port-short", rdata(1, t, param(3, "x"))},
        {"port-long", rdata(1, t, param(3, "xyz"))},
        {"ipv4-empty", rdata(1, t, param(4, ""))},
        {"ipv4-five", rdata(1, t, param(4, "12345"))},
        {"ipv6-fifteen", rdata(1, t, param(6, std::string(15, '\x01')))},
        {"mandatory-empty", rdata(1, t, param(0, "") + param(3, u16(1)))},
        {"mandatory-odd", rdata(1, t, param(0, std::string("\x00\x03\x00", 3)) + param(3, u16(1)))},
        {"mandatory-self", rdata(1, t, param(0, u16(0) + u16(3)) + param(3, u16(1)))},
        {"mandatory-unsorted", rdata(1, t, param(0, u16(3) + u16(1)) + param(1, "\x02h2") + param(3, u16(1)))},
        {"mandatory-twice", rdata(1, t, param(0, u16(3) + u16(3)) + param(3, u16(1)))},
        {"mandatory-missing", rdata(1, t, param(0, u16(1) + u16(4)) + param(1, "\x02h2"))},
        {"mandatory-missing-last", rdata(1, t, param(0, u16(1) + u16(9)) + param(1, "\x02h2") + param(3, u16(1)))},
        {"invalid-key", rdata(1, t, param(65535, ""))},
        {"no-target", u16(1)},
        {"target-past", u16(1) + std::string("\x03svc", 4)},
    };
    Behaviour b;
    for (auto& [name, data] : bad) {
        b.zone.push_back(rr_https(name + ".example.test.", data));
        b.zone.push_back(rr_https(name + ".example.test.", rdata(1, "good.example.test.")));   // a good record beside it
    }
    Server s(b);
    ASSERT_TRUE(s.ok());
    for (auto& [name, data] : bad) {
        auto r = dns::lookup_https(string(name + ".example.test."), s.options());
        ASSERT_FALSE(r.has_value()) << name;
        EXPECT_EQ(r.error().code(), net::errc::server_misbehaving) << name;
    }
}

// The failures as the other record lookups have them; the stop; a server
// that is none; a truncated answer asked again over TCP
TEST(NetDnsHttps_Tests, Failures) {
    Behaviour b;
    std::string many;
    for (int i = 0; i < 40; ++i) {
        many += param(unsigned(100 + i), std::string(40, char('a' + i % 26)));
    }
    b.zone = {
        rr_a("plain.example.test.", 192, 0, 2, 1),
        rr_https("big.example.test.", rdata(1, ".", many)),
        rr_https("tc.example.test.", rdata(1, ".")),
    };
    b.rcode["fail.example.test."] = nd::dns_rcode::server_failure;
    b.rcode["refused.example.test."] = nd::dns_rcode::refused;
    b.truncate.insert("tc.example.test.");
    Server s(b);
    ASSERT_TRUE(s.ok());
    auto missing = dns::lookup_https("missing.example.test.", s.options());
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code(), net::errc::host_not_found);
    EXPECT_EQ(str(missing.error().op()), "lookup");
    auto nodata = dns::lookup_https("plain.example.test.", s.options());
    ASSERT_FALSE(nodata.has_value());
    EXPECT_EQ(nodata.error().code(), net::errc::no_data);
    auto fail = dns::lookup_svcb("fail.example.test.", s.options());
    ASSERT_FALSE(fail.has_value());
    EXPECT_EQ(fail.error().code(), net::errc::server_failure);
    auto refused = dns::lookup_https("refused.example.test.", s.options());
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), net::errc::server_misbehaving);
    auto empty = dns::lookup_https("", s.options());
    ASSERT_FALSE(empty.has_value());
    EXPECT_EQ(empty.error().code(), net::errc::host_not_found);
    auto dots = dns::lookup_https("a..b", s.options());
    ASSERT_FALSE(dots.has_value());
    EXPECT_EQ(dots.error().code(), net::errc::host_not_found);
    dns::options bad;
    bad.servers = {"dns.example"};
    auto none = dns::lookup_https("x.example.test.", bad);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::errc::invalid_address);
    // a stop requested before: ECANCELED, nothing asked
    int before = b.udp_queries.load();
    async::stop_source source;
    source.request_stop();
    auto stopped = dns::async_lookup_https("big.example.test.", s.options(), source.token()).wait();
    ASSERT_FALSE(stopped.has_value());
    EXPECT_EQ(stopped.error().code(), std::errc::operation_canceled);
    EXPECT_EQ(b.udp_queries.load(), before);
    // a stop while the server holds the answer
    b.delay = std::chrono::milliseconds(300);
    async::stop_source later;
    later.stop_after(50 * millisecond);
    auto t0 = std::chrono::steady_clock::now();
    auto late = dns::async_lookup_svcb("tc.example.test.", s.options(std::chrono::milliseconds(2000)), later.token()).wait();
    ASSERT_FALSE(late.has_value());
    EXPECT_EQ(late.error().code(), std::errc::operation_canceled);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::milliseconds(280));
    b.delay = std::chrono::milliseconds(0);
    // past the UDP payload: TC by the server's writer, then TCP
    int tcp = b.tcp_queries.load();
    auto big = dns::lookup_https("big.example.test.", s.options());
    ASSERT_TRUE(big.has_value()) << str(big.error().message());
    ASSERT_EQ(big->size(), 1u);
    EXPECT_EQ((*big)[0].params.size(), 40u);
    EXPECT_EQ(str((*big)[0].target), "big.example.test.");
    auto tc = dns::lookup_https("tc.example.test.", s.options());
    ASSERT_TRUE(tc.has_value());
    EXPECT_GT(b.tcp_queries.load(), tcp);
    // a query to nobody: refused at once
    auto gone = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(gone.has_value());
    dns::options closed;
    closed.servers = {gone->local_endpoint().to_string()};
    closed.timeout = 500 * millisecond;
    closed.attempts = 1;
    (void)gone->close();
    auto refusedc = dns::lookup_https("x.example.test.", closed);
    ASSERT_FALSE(refusedc.has_value());
}

// The reader of SvcParams alone: the vectors and the malformed forms
TEST(NetDnsHttps_Tests, TheParamsReader) {
    auto ok = [](const std::string& p) {
        return nd::dns_svc_params_ok(reinterpret_cast<const uint8_t*>(p.data()), p.size());
    };
    EXPECT_TRUE(ok(""));
    EXPECT_TRUE(ok(param(3, u16(53))));
    EXPECT_TRUE(ok(param(0, u16(1) + u16(4)) + param(1, "\x02h2\x05h3-19") + param(4, unhex("c0000201"))));
    EXPECT_TRUE(ok(param(5, "")));                    // ech: bytes as they come
    EXPECT_TRUE(ok(param(7, "")));
    EXPECT_TRUE(ok(param(65534, std::string(300, 'x'))));
    EXPECT_FALSE(ok(param(65535, "")));
    EXPECT_FALSE(ok(param(3, u16(53)) + param(3, u16(53))));
    EXPECT_FALSE(ok(param(4, unhex("c0000201")) + param(3, u16(53))));
    EXPECT_FALSE(ok(param(3, u16(53)).substr(0, 5)));
    EXPECT_FALSE(ok(u16(3)));
    EXPECT_FALSE(ok(param(0, u16(1)) + param(3, u16(53))));
    // the visitor sees each in order
    std::vector<unsigned> keys;
    std::string p = param(1, "\x02h2") + param(3, u16(53)) + param(600, "zz");
    nd::dns_svc_params_each(reinterpret_cast<const uint8_t*>(p.data()), p.size(), [&](uint16_t k, const uint8_t*, size_t n) {
        keys.push_back(k * 1000u + unsigned(n));
    });
    EXPECT_EQ(keys, (std::vector<unsigned>{1003, 3002, 600002}));
}

// Against Go's golang.org/x/net/dns/dnsmessage (the copy vendored in Go's
// standard library, built here in a module of the test's own: nothing is
// downloaded): random HTTPS and SVCB answers, written here from RFC 9460,
// read by both; the same records, priorities, targets and SvcParams byte
// for byte, and the same refusal of the framing faults (a key out of
// order, a value past the rdata). Skipped when there is no go
namespace {
    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    std::string svcb_oracle() {
        static std::string built = [] {
            std::string go = go_path();
            if (go.empty()) {
                return std::string();
            }
            io::command root(sgcl::string(go), sgcl::string("env"), sgcl::string("GOROOT"));
            auto out = root.output();
            if (!out) {
                return std::string();
            }
            std::string goroot(out->view());
            while (!goroot.empty() && (goroot.back() == '\n' || goroot.back() == '\r')) {
                goroot.pop_back();
            }
            namespace fs = std::filesystem;
            fs::path vendored = fs::path(goroot) / "src/vendor/golang.org/x/net/dns/dnsmessage";
            if (!fs::exists(vendored / "svcb.go")) {
                return std::string();
            }
            auto d = fs::temp_directory_path() / ("sgcl_svcb_oracle_" + std::to_string(::getpid()));
            std::error_code ec;
            fs::create_directories(d / "dnsmessage", ec);
            for (auto& f : fs::directory_iterator(vendored)) {
                auto n = f.path().filename().string();
                if (n.size() > 3 && n.ends_with(".go") && !n.ends_with("_test.go")) {
                    fs::copy_file(f.path(), d / "dnsmessage" / n, fs::copy_options::overwrite_existing, ec);
                }
            }
            fs::copy_file(source_root() / "tools/svcb_oracle.go", d / "main.go", fs::copy_options::overwrite_existing, ec);
            std::ofstream(d / "go.mod") << "module svcboracle\n\ngo 1.22\n";
            std::string bin = (d / "svcb_oracle").string();
            io::command b(sgcl::string(go), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin), sgcl::string("."));
            b.dir = sgcl::string(d.string());   // a module of no requirement: nothing to fetch
            auto r = b.combined_output();
            return r.has_value() && fs::exists(bin) ? bin : std::string();
        }();
        return built;
    }

    // One answer to the question (name, type): its records' rdata given
    std::string message(const std::string& name, uint16_t t, const std::vector<std::string>& rdatas) {
        std::string m = u16(0x1234) + u16(0x8180) + u16(1) + u16(unsigned(rdatas.size())) + u16(0) + u16(0);
        m += name_wire(name) + u16(t) + u16(1);
        for (auto& r : rdatas) {
            m += std::string("\xc0\x0c", 2) + u16(t) + u16(1) + u16(0) + u16(300) + u16(unsigned(r.size())) + r;
        }
        return m;
    }

    // SGCL's reading, in the oracle's form: "prio target k=hex,k=hex;" per record, or "ERR"
    std::string ours(const std::string& m, const std::string& name, uint16_t t) {
        nd::DnsAnswer a;
        nd::DnsName q = wire(name);
        auto st = nd::dns_read_answer(reinterpret_cast<const uint8_t*>(m.data()), m.size(), 0x1234, q, t, false, a);
        if (st != nd::DnsStatus::ok) {
            return "ERR";
        }
        std::string out;
        for (auto& r : a.records) {
            out += std::to_string(r.first) + " " + str(r.name);
            std::string p = str(r.text);
            std::string sep = " ";
            nd::dns_svc_params_each(reinterpret_cast<const uint8_t*>(p.data()), p.size(), [&](uint16_t k, const uint8_t* v, size_t n) {
                out += sep + std::to_string(k) + "=" + hex(std::string(reinterpret_cast<const char*>(v), n));
                sep = ",";
            });
            out += ";";
        }
        return out;
    }
}

TEST(NetDnsHttps_Tests, AgainstGo) {
    std::string bin = svcb_oracle();
    if (bin.empty()) {
        GTEST_SKIP() << "no go with a vendored dns/dnsmessage on this machine";
    }
    std::mt19937 rng(9460);
    auto pick = [&](unsigned n) { return unsigned(rng() % n); };
    std::vector<std::string> msgs;
    std::vector<bool> disorder;
    for (int i = 0; i < 400; ++i) {
        const uint16_t t = pick(2) ? type::https : type::svcb;
        std::vector<std::string> rs;
        unsigned count = 1 + pick(3);
        for (unsigned j = 0; j < count; ++j) {
            std::string params;
            unsigned key = 0;
            // faults in the first record of one message in ten: a value
            // past the rdata (both refuse), or a key out of order (RFC 9460
            // §2.2: malformed; Go's reader checks each key against a
            // previous one it never updates, so only SGCL refuses it)
            bool fault = i % 10 == 9 && j == 0;
            bool order_fault = fault && i % 20 == 19;
            for (unsigned k = 0, n = pick(6); k < n; ++k) {
                key += 1 + pick(5);
                std::string v;
                switch (key) {
                    case 1: v = "\x02h2\x08http/1.1"; break;
                    case 2: v = ""; break;
                    case 3: v = u16(1 + pick(65535)); break;
                    case 4: for (unsigned x = 0, c = 1 + pick(3); x < c; ++x) { v += unhex("c00002") + char(pick(256)); } break;
                    case 5: v = std::string(1 + pick(80), char('e' + pick(10))); break;
                    case 6: v = std::string(16 * (1 + pick(2)), char(pick(256))); break;
                    default: v = std::string(pick(20), char(pick(256)));
                }
                params += param(key, v);
            }
            if (order_fault) {
                params += param(3, u16(80)) + param(1, "\x02h2");   // a key after a larger one
            } else if (fault) {
                params += u16(500) + u16(9) + u16(1);   // a value past the rdata
            }
            rs.push_back(rdata(1 + pick(3), pick(2) ? "." : "t" + std::to_string(pick(100)) + ".example.test.", params));
        }
        msgs.push_back(message("q.example.test.", t, rs) + "|" + std::to_string(t));
        disorder.push_back(i % 20 == 19);
    }
    // Go reads each message: one line of input, one line of output
    std::string input;
    for (auto& m : msgs) {
        auto bar = m.find_last_of('|');
        input += hex(m.substr(0, bar)) + " " + m.substr(bar + 1) + "\n";
    }
    io::command go{sgcl::string(bin)};
    go.in = make_tracked<io::buffer>(sgcl::string(input));
    auto out = go.output();
    ASSERT_TRUE(out.has_value()) << str(out.error().message());
    std::istringstream lines{std::string(out->view())};
    std::string line;
    size_t i = 0, agreed = 0, refused = 0, disordered = 0;
    while (std::getline(lines, line) && i < msgs.size()) {
        auto bar = msgs[i].find_last_of('|');
        std::string mine = ours(msgs[i].substr(0, bar), "q.example.test.", uint16_t(std::stoi(msgs[i].substr(bar + 1))));
        if (disorder[i]) {
            EXPECT_EQ(mine, "ERR") << "message " << i;
            ++disordered;
        } else {
            EXPECT_EQ(mine, line) << "message " << i << ": " << hex(msgs[i].substr(0, bar));
            agreed += mine == line;
            refused += line == "ERR";
        }
        ++i;
    }
    EXPECT_EQ(i, msgs.size());
    EXPECT_EQ(agreed + disordered, msgs.size());
    EXPECT_EQ(disordered, 20u);
    EXPECT_EQ(refused, 20u);   // the values past the rdata
}
