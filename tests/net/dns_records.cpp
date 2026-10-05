//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dns's records (lookup_mx, lookup_txt, lookup_srv, lookup_ns,
// lookup_cname and their task forms) against the DNS server of
// tests/net/dns_server.h on the loopback: every record type, both forms;
// NXDOMAIN, NODATA, SERVFAIL, REFUSED and a lame referral told apart;
// TC and a datagram past the buffer asking again over TCP, a TCP answer
// cut short; a server that drops queries (the timeout, the next attempt
// with a new id), answers first with a wrong id, another question, junk,
// or a forgery from another port (RFC 5452); EDNS0 in every query; a chain
// the server left unfollowed; the stop; the options at their boundaries;
// /etc/resolv.conf replaced by a file of the test's: the search list and
// ndots, two servers (the first failing), rotate, use-vc, the local
// machine's servers when the file names none. Nothing leaves the machine.
#include "tests/types.h"
#include "tests/net/dns_server.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
    // /etc/resolv.conf replaced by the test's file for as long as it lives,
    // the one before it put back after
    struct ResolvConfFile {
        std::string path;
        std::string previous;

        explicit ResolvConfFile(const std::string& text) {
            static std::atomic<int> n{0};
            auto d = std::filesystem::temp_directory_path() / ("sgcl_dns_" + std::to_string(::getpid()));
            std::filesystem::create_directories(d);
            path = (d / ("resolv" + std::to_string(n++) + ".conf")).string();
            std::ofstream(path, std::ios::trunc) << text;
            previous = sgcl::net::detail::resolv_conf_cache().path();
            sgcl::net::detail::resolv_conf_cache().set_path(path);
        }

        ~ResolvConfFile() {
            sgcl::net::detail::resolv_conf_cache().set_path(previous);
            std::error_code e;
            std::filesystem::remove(path, e);
            std::filesystem::remove(std::filesystem::path(path).parent_path(), e);   // when it is empty
        }
    };
}

namespace {
    using namespace dns_test;
    using namespace std::chrono_literals;
    using sgcl::net::dns;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::vector<Rr> zone() {
        return {
            rr_mx("example.test.", 20, "mx2.example.test."),
            rr_mx("example.test.", 10, "mx1.example.test."),
            rr_mx("example.test.", 20, "mx3.example.test."),
            rr_txt("example.test.", {"v=spf1 ", "-all"}),
            rr_txt("example.test.", {"second"}),
            rr_txt("empty.example.test.", {}),
            rr_named("example.test.", type::ns, "ns1.example.test."),
            rr_named("example.test.", type::ns, "ns2.example.test."),
            rr_srv("_sip._tcp.example.test.", 20, 0, 5060, "backup.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 60, 5060, "sip1.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 40, 5061, "sip2.example.test."),
            rr_srv("_none._tcp.example.test.", 0, 0, 0, "."),
            rr_mx("null.example.test.", 0, "."),
            rr_named("www.example.test.", type::cname, "web.example.test."),
            rr_named("web.example.test.", type::cname, "host.example.test."),
            rr_a("host.example.test.", 192, 0, 2, 1),
            rr_aaaa("v6only.example.test."),
            rr_named("dangling.example.test.", type::cname, "gone.example.test."),
            rr_named("alias.example.test.", type::cname, "example.test."),
            rr_a("example.test.", 192, 0, 2, 7),
            rr_mx("mail.corp.test.", 5, "in.corp.test."),
            rr_mx("mail.other.", 7, "in.other."),
            rr_named(".", type::ns, "a.root-servers.test."),
            rr_txt("big.example.test.", {std::string(255, 'a'), std::string(255, 'b'), std::string(255, 'c'), std::string(255, 'd'),
                                      std::string(255, 'e'), std::string(255, 'f'), std::string(255, 'g'), std::string(255, 'h')}),
        };
    }

    // The zone, and a resolv.conf of no search list, so that a name is
    // asked as it is whatever the machine's file says
    struct Fixture {
        ResolvConfFile conf{"search\n"};
        Behaviour b;
        explicit Fixture() {
            b.zone = zone();
        }
    };

    std::vector<std::string> sorted(std::vector<std::string> v) {
        std::sort(v.begin(), v.end());
        return v;
    }
}

TEST(NetDnsRecords_Tests, MX) {
    Fixture f;
    Server s(f.b);
    ASSERT_TRUE(s.ok());
    auto r = dns::lookup_mx("example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 3u);
    EXPECT_EQ(str((*r)[0].host), "mx1.example.test.");
    EXPECT_EQ((*r)[0].preference, 10);
    EXPECT_EQ((*r)[1].preference, 20);
    EXPECT_EQ((*r)[2].preference, 20);
    EXPECT_EQ(sorted({str((*r)[1].host), str((*r)[2].host)}), (std::vector<std::string>{"mx2.example.test.", "mx3.example.test."}));
    EXPECT_EQ(f.b.last_payload.load(), 1232);   // EDNS0 in the query
    EXPECT_TRUE(f.b.last_flags.load() & 0x0100);   // recursion desired
    auto t = dns::async_lookup_mx("example.test", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->size(), 3u);
    auto null = dns::lookup_mx("null.example.test.", s.options());
    ASSERT_TRUE(null.has_value());
    ASSERT_EQ(null->size(), 1u);
    EXPECT_EQ(str((*null)[0].host), ".");   // RFC 7505
    EXPECT_EQ((*null)[0], (dns::mx{".", 0}));
    // a CNAME before the MX: followed
    auto alias = dns::lookup_mx("alias.example.test", s.options());
    ASSERT_TRUE(alias.has_value());
    EXPECT_EQ(alias->size(), 3u);
    // the equal preferences come in both orders
    std::set<std::string> seconds;
    for (int i = 0; i < 40 && seconds.size() < 2; ++i) {
        auto again = dns::lookup_mx("example.test", s.options());
        ASSERT_TRUE(again.has_value());
        seconds.insert(str((*again)[1].host));
    }
    EXPECT_EQ(seconds.size(), 2u);
}

TEST(NetDnsRecords_Tests, TXT) {
    Fixture f;
    Server s(f.b);
    auto r = dns::lookup_txt("example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ(str((*r)[0]), "v=spf1 -all");   // the strings of one record joined
    EXPECT_EQ(str((*r)[1]), "second");
    auto e = dns::lookup_txt("empty.example.test", s.options());
    ASSERT_TRUE(e.has_value());
    ASSERT_EQ(e->size(), 1u);
    EXPECT_TRUE((*e)[0].empty());
    auto t = dns::async_lookup_txt("example.test", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->size(), 2u);
}

TEST(NetDnsRecords_Tests, SRV) {
    Fixture f;
    Server s(f.b);
    std::set<std::string> firsts;
    for (int i = 0; i < 60 && firsts.size() < 2; ++i) {
        auto r = dns::lookup_srv("sip", "tcp", "example.test", s.options());
        ASSERT_TRUE(r.has_value()) << str(r.error().message());
        ASSERT_EQ(r->size(), 3u);
        EXPECT_EQ((*r)[0].priority, 10);
        EXPECT_EQ((*r)[1].priority, 10);
        EXPECT_EQ((*r)[2], (dns::srv{"backup.example.test.", 5060, 20, 0}));
        firsts.insert(str((*r)[0].target));
    }
    EXPECT_EQ(firsts, (std::set<std::string>{"sip1.example.test.", "sip2.example.test."}));   // weights 60 and 40: both lead at times
    auto whole = dns::lookup_srv("", "", "_sip._tcp.example.test", s.options());
    ASSERT_TRUE(whole.has_value());
    EXPECT_EQ(whole->size(), 3u);
    auto none = dns::lookup_srv("none", "tcp", "example.test.", s.options());
    ASSERT_TRUE(none.has_value());
    ASSERT_EQ(none->size(), 1u);
    EXPECT_EQ(str((*none)[0].target), ".");   // RFC 2782: not offered
    auto t = dns::async_lookup_srv("sip", "tcp", "example.test", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->size(), 3u);
    auto missing = dns::lookup_srv("xmpp", "tcp", "example.test", s.options());
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(str(missing.error().path()), "_xmpp._tcp.example.test");
    EXPECT_EQ(missing.error().code(), net::errc::host_not_found);
}

TEST(NetDnsRecords_Tests, NS) {
    Fixture f;
    Server s(f.b);
    auto r = dns::lookup_ns("example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ(str((*r)[0]), "ns1.example.test.");
    EXPECT_EQ(str((*r)[1]), "ns2.example.test.");
    auto root = dns::lookup_ns(".", s.options());
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(str(root->front()), "a.root-servers.test.");
    auto t = dns::async_lookup_ns("example.test", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->size(), 2u);
}

TEST(NetDnsRecords_Tests, CNAME) {
    Fixture f;
    Server s(f.b);
    auto r = dns::lookup_cname("www.example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(str(*r), "host.example.test.");   // the end of www -> web -> host
    auto self = dns::lookup_cname("host.example.test", s.options());
    ASSERT_TRUE(self.has_value());
    EXPECT_EQ(str(*self), "host.example.test.");   // no alias: the name itself
    auto v6 = dns::lookup_cname("V6ONLY.example.test", s.options());
    ASSERT_TRUE(v6.has_value());
    EXPECT_EQ(str(*v6), "V6ONLY.example.test.");   // a name without A, that exists
    auto dangling = dns::lookup_cname("dangling.example.test", s.options());
    ASSERT_TRUE(dangling.has_value());
    EXPECT_EQ(str(*dangling), "gone.example.test.");   // the chain's end, though it has nothing
    auto missing = dns::lookup_cname("nothing.example.test", s.options());
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code(), net::errc::host_not_found);
    auto t = dns::async_lookup_cname("www.example.test", s.options()).wait();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(str(*t), "host.example.test.");
}

// NXDOMAIN, NODATA, SERVFAIL, REFUSED and a lame referral, each its own
// code; the error names the operation and the name as asked
TEST(NetDnsRecords_Tests, TheFailuresToldApart) {
    Fixture f;
    f.b.rcode["servfail.example.test."] = 2;
    f.b.rcode["refused.example.test."] = 5;
    f.b.rcode["formerr.example.test."] = 1;
    Server s(f.b);
    auto nx = dns::lookup_mx("nothing.example.test", s.options());
    ASSERT_FALSE(nx.has_value());
    EXPECT_EQ(nx.error().code(), net::errc::host_not_found);
    EXPECT_EQ(str(nx.error().message()), "lookup nothing.example.test: no such host");
    auto nodata = dns::lookup_mx("host.example.test", s.options());
    ASSERT_FALSE(nodata.has_value());
    EXPECT_EQ(nodata.error().code(), net::errc::no_data);
    EXPECT_EQ(str(nodata.error().message()), "lookup host.example.test: no DNS record of the type asked");
    auto servfail = dns::lookup_txt("servfail.example.test", s.options());
    ASSERT_FALSE(servfail.has_value());
    EXPECT_EQ(servfail.error().code(), net::errc::server_failure);
    EXPECT_EQ(str(servfail.error().message()), "lookup servfail.example.test: DNS server failure");
    auto refused = dns::lookup_ns("refused.example.test", s.options());
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), net::errc::server_misbehaving);
    auto formerr = dns::lookup_srv("", "", "formerr.example.test", s.options());
    ASSERT_FALSE(formerr.has_value());
    EXPECT_EQ(formerr.error().code(), net::errc::server_misbehaving);
    Fixture g;
    g.b.lame = true;
    Server lame(g.b);
    auto l = dns::lookup_mx("example.test", lame.options());
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error().code(), net::errc::server_misbehaving);
}

// TC over UDP: the same query over TCP, within the same deadline; an
// answer of more than the UDP buffer read whole there
TEST(NetDnsRecords_Tests, TruncatedAnswersAskAgainOverTCP) {
    Fixture f;
    f.b.truncate.insert("example.test.");
    f.b.truncate.insert("big.example.test.");
    Server s(f.b);
    auto r = dns::lookup_mx("example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(r->size(), 3u);
    EXPECT_EQ(f.b.udp_queries.load(), 1);
    EXPECT_EQ(f.b.tcp_queries.load(), 1);
    auto big = dns::lookup_txt("big.example.test", s.options());
    ASSERT_TRUE(big.has_value()) << str(big.error().message());
    ASSERT_EQ(big->size(), 1u);
    EXPECT_EQ((*big)[0].size(), 255u * 8);
    EXPECT_EQ(f.b.tcp_queries.load(), 2);
}

TEST(NetDnsRecords_Tests, ADatagramPastTheBufferIsTakenAsTruncated) {
    Fixture f;
    f.b.oversize = true;
    Server s(f.b);
    auto r = dns::lookup_mx("example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(r->size(), 3u);
    EXPECT_EQ(f.b.tcp_queries.load(), 1);
}

TEST(NetDnsRecords_Tests, ATCPAnswerCutShortIsAFailure) {
    Fixture f;
    f.b.truncate.insert("example.test.");
    f.b.tcp_cut = true;
    Server s(f.b);
    auto r = dns::lookup_mx("example.test", s.options());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(str(r.error().op()), "lookup");
    EXPECT_EQ(f.b.tcp_queries.load(), 1);
}

// A server that drops a query: the attempt waits its timeout, the next
// one asks again with a new id; every attempt dropped is ETIMEDOUT
TEST(NetDnsRecords_Tests, TimeoutsAndAttempts) {
    Fixture f;
    f.b.drop = 1;
    Server s(f.b);
    auto t0 = std::chrono::steady_clock::now();
    auto r = dns::lookup_mx("example.test", s.options(150ms, 2));
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 140ms);
    EXPECT_EQ(f.b.udp_queries.load(), 2);
    EXPECT_EQ(f.b.ids.size(), 2u);   // a new id for the new query
    Fixture g;
    g.b.drop = 1000;
    Server dead(g.b);
    t0 = std::chrono::steady_clock::now();
    auto none = dns::lookup_mx("example.test", dead.options(100ms, 3));
    auto took = std::chrono::steady_clock::now() - t0;
    ASSERT_FALSE(none.has_value());
    EXPECT_TRUE(none.error().is_timeout());
    EXPECT_EQ(str(none.error().message()), "lookup example.test: Operation timed out");
    EXPECT_EQ(g.b.udp_queries.load(), 3);
    EXPECT_GE(took, 290ms);
    EXPECT_LT(took, 3s);
}

// RFC 5452 §9.1: what is not the answer to the query is ignored, and the
// wait goes on for the answer
TEST(NetDnsRecords_Tests, WrongIdsQuestionsJunkAndForgeriesIgnored) {
    Fixture f;
    f.b.wrong_id = 1;
    f.b.wrong_question = 1;
    f.b.garbage = 1;
    f.b.forged = 1;
    Server s(f.b);
    auto r = dns::lookup_mx("example.test", s.options(2s, 1));
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    ASSERT_EQ(r->size(), 3u);
    for (auto& m : *r) {
        EXPECT_NE(str(m.host), "forged.example.");   // the forgery came from another port: the connected socket never saw it
    }
    EXPECT_EQ(f.b.udp_queries.load(), 1);
    EXPECT_EQ(f.b.wrong_id.load(), 0);
    EXPECT_EQ(f.b.forged.load(), 0);
}

// A server that gives the CNAME alone: the chain's end is asked next
TEST(NetDnsRecords_Tests, AChainTheServerLeftIsFollowed) {
    Fixture f;
    f.b.chase = false;
    Server s(f.b);
    auto r = dns::lookup_mx("alias.example.test", s.options());
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(r->size(), 3u);
    EXPECT_EQ(f.b.queries(), (std::vector<std::string>{"alias.example.test. 15", "example.test. 15"}));
    auto c = dns::lookup_cname("www.example.test", s.options());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(str(*c), "host.example.test.");
    // a loop of CNAMEs, one per answer: the bound ends it
    Fixture g;
    g.b.chase = false;
    g.b.zone = {rr_named("l1.test.", type::cname, "l2.test."), rr_named("l2.test.", type::cname, "l1.test.")};
    Server loop(g.b);
    auto l = dns::lookup_mx("l1.test", loop.options());
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error().code(), net::errc::server_misbehaving);
    EXPECT_LE(g.b.udp_queries.load(), 9);
}

TEST(NetDnsRecords_Tests, TheStop) {
    Fixture f;
    f.b.drop = 1000;
    Server s(f.b);
    async::stop_source done;
    done.request_stop();
    auto before = dns::async_lookup_mx("example.test", s.options(5s, 1), done.token()).wait();
    ASSERT_FALSE(before.has_value());
    EXPECT_EQ(before.error().code(), std::errc::operation_canceled);
    EXPECT_EQ(f.b.udp_queries.load(), 0);   // stopped before: nothing asked
    async::stop_source later;
    later.stop_after(100ms);
    auto t0 = std::chrono::steady_clock::now();
    auto during = dns::async_lookup_txt("example.test", s.options(5s, 1), later.token()).wait();
    auto took = std::chrono::steady_clock::now() - t0;
    ASSERT_FALSE(during.has_value());
    EXPECT_EQ(during.error().code(), std::errc::operation_canceled);
    EXPECT_EQ(str(during.error().message()), "lookup example.test: Operation canceled");
    EXPECT_LT(took, 2s);
    EXPECT_EQ(f.b.udp_queries.load(), 1);
    // a token that is never stopped changes nothing
    Fixture g;
    Server ok(g.b);
    async::stop_source never;
    auto fine = dns::async_lookup_ns("example.test", ok.options(), never.token()).wait();
    ASSERT_TRUE(fine.has_value());
    auto fine2 = dns::async_lookup_cname("www.example.test", ok.options(), never.token()).wait();
    ASSERT_TRUE(fine2.has_value());
    auto fine3 = dns::async_lookup_srv("sip", "tcp", "example.test", ok.options(), never.token()).wait();
    ASSERT_TRUE(fine3.has_value());
    // the stop while the TCP exchange waits
    Fixture h;
    h.b.truncate.insert("example.test.");
    h.b.delay = 0ms;
    Server tc(h.b);
    async::stop_source mid;
    mid.stop_after(50ms);
    auto any = dns::async_lookup_mx("example.test", tc.options(5s, 1), mid.token()).wait();
    EXPECT_TRUE(any.has_value() || any.error().code() == std::errc::operation_canceled);
}

// The task forms start when first awaited, never at the call
TEST(NetDnsRecords_Tests, TheTaskFormsStartLazily) {
    Fixture f;
    Server s(f.b);
    {
        auto t = dns::async_lookup_mx("example.test", s.options());
        std::this_thread::sleep_for(50ms);
        EXPECT_EQ(f.b.udp_queries.load(), 0);
        auto r = t.wait();
        EXPECT_TRUE(r.has_value());
        EXPECT_EQ(f.b.udp_queries.load(), 1);
    }
    {
        auto t = dns::async_lookup_txt("example.test");   // dropped unstarted: nothing asked of anyone
        (void)t;
    }
}

TEST(NetDnsRecords_Tests, TheNamesAtTheirBoundaries) {
    Fixture f;
    Server s(f.b);
    for (std::string bad : {std::string(), std::string(".."), std::string("a..b"), std::string(64, 'x') + ".test",
                            std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." + std::string(62, 'd')}) {
        SCOPED_TRACE(bad);
        auto r = dns::lookup_mx(sgcl::string(bad), s.options());
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), net::errc::host_not_found);
        auto t = dns::lookup_txt(sgcl::string(bad), s.options());
        ASSERT_FALSE(t.has_value());
        auto c = dns::lookup_cname(sgcl::string(bad), s.options());
        ASSERT_FALSE(c.has_value());
        auto n = dns::lookup_ns(sgcl::string(bad), s.options());
        ASSERT_FALSE(n.has_value());
        auto v = dns::lookup_srv("", "", sgcl::string(bad), s.options());
        ASSERT_FALSE(v.has_value());
    }
    EXPECT_EQ(f.b.udp_queries.load(), 0);   // none of them asked
    // the longest name: asked, and not found
    std::string longest = std::string(63, 'a') + "." + std::string(63, 'b') + "." + std::string(63, 'c') + "." + std::string(61, 'd');
    auto r = dns::lookup_mx(sgcl::string(longest), s.options());
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), net::errc::host_not_found);
    EXPECT_EQ(f.b.udp_queries.load(), 1);
    // a name with a NUL: one name of those bytes, not cut at the NUL
    auto nul = dns::lookup_mx(sgcl::string(std::string_view("example.test\0.evil", 18)), s.options());
    ASSERT_FALSE(nul.has_value());
    // escapes: "\." inside a label
    auto esc = dns::lookup_txt("example\\.test", s.options());
    ASSERT_FALSE(esc.has_value());
    EXPECT_EQ(f.b.queries().back(), "example\\.test. 16");
    // the case of the answer's question is not the query's: the same name
    auto upper = dns::lookup_ns("EXAMPLE.TEST", s.options());
    ASSERT_TRUE(upper.has_value());
}

TEST(NetDnsRecords_Tests, TheOptionsAtTheirBoundaries) {
    namespace nd = sgcl::net::detail;
    auto at = [](const char* text) { return str(nd::dns_server_spec(text).value().address.to_string()); };
    EXPECT_EQ(at("10.0.0.1"), "10.0.0.1:53");
    EXPECT_EQ(at("10.0.0.1:5353"), "10.0.0.1:5353");
    EXPECT_EQ(at("::1"), "[::1]:53");
    EXPECT_EQ(at("[::1]:5300"), "[::1]:5300");
    EXPECT_EQ(at("fe80::1%lo0"), "[fe80::1%lo0]:53");
    EXPECT_EQ(at("udp://10.0.0.1"), "10.0.0.1:53");
    EXPECT_EQ(at("TCP://10.0.0.1:54"), "10.0.0.1:54");
    EXPECT_EQ(nd::dns_server_spec("tcp://10.0.0.1").value().transport, nd::DnsTransport::tcp);
    EXPECT_EQ(at("tls://10.0.0.1"), "10.0.0.1:853");
    EXPECT_EQ(at("tls://[::1]:8853"), "[::1]:8853");
    auto named = nd::dns_server_spec("tls://dns.example");
    ASSERT_TRUE(named.has_value());
    EXPECT_EQ(named->transport, nd::DnsTransport::tls);
    EXPECT_FALSE(named->address.is_valid());
    EXPECT_EQ(str(named->host), "dns.example");
    EXPECT_EQ(named->port, 853);
    EXPECT_EQ(nd::dns_server_spec("tls://dns.example:8853").value().port, 8853);
    auto doh = nd::dns_server_spec("https://dns.example/dns-query");
    ASSERT_TRUE(doh.has_value());
    EXPECT_EQ(doh->transport, nd::DnsTransport::https);
    EXPECT_EQ(str(doh->host), "https://dns.example/dns-query");
    for (const char* bad : {"", "dns.example", "dns.example:53", "10.0.0.1:0", "10.0.0.1:70000", "10.0.0.1:", "[::1]", ":53", "1.2.3",
                            "udp://", "tcp://dns.example", "tls://", "tls://dns.example:0", "tls://dns.example:x", "tls://a b", "tls://a/b",
                            "https://", "https://a b/x", "http://dns.example/dns-query", "ftp://10.0.0.1", "tls://[::1]"}) {
        SCOPED_TRACE(bad);
        auto e = nd::dns_server_spec(bad);
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), net::errc::invalid_address);
        dns::options o;
        o.servers = {bad};
        auto r = dns::lookup_mx("example.test", o);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), net::errc::invalid_address);
        auto t = dns::async_lookup_txt("example.test", o).wait();
        ASSERT_FALSE(t.has_value());
        EXPECT_EQ(t.error().code(), net::errc::invalid_address);
    }
    // the defaults of a struct: none of its own
    dns::options o;
    EXPECT_TRUE(o.servers.empty());
    EXPECT_FALSE(o.opportunistic);
    EXPECT_FALSE(o.https_get);
    EXPECT_TRUE(o.roots_pem.empty());
    EXPECT_EQ(o.timeout, duration::zero());
    EXPECT_EQ(o.attempts, 0);
    dns::mx m;
    EXPECT_TRUE(m.host.empty());
    EXPECT_EQ(m.preference, 0);
    dns::srv v;
    EXPECT_TRUE(v.target.empty());
    EXPECT_EQ(v.port + v.priority + v.weight, 0);
    // a moved-from options has no servers left (a vector's move), and the
    // moved-to one asks them
    Fixture f;
    Server s(f.b);
    dns::options from = s.options();
    dns::options to = std::move(from);
    EXPECT_TRUE(from.servers.empty());
    ASSERT_EQ(to.servers.size(), 1u);
    EXPECT_TRUE(dns::lookup_mx("example.test", to).has_value());
    // a timeout of zero or less and attempts of zero or less are the file's
    dns::options neg = s.options();
    neg.timeout = -5s;
    neg.attempts = -1;
    EXPECT_TRUE(dns::lookup_mx("example.test", neg).has_value());
    // a server where nobody listens: refused at once (ICMP), not a timeout
    Fixture g;
    uint16_t port = 0;
    {
        Server gone(g.b);
        port = gone.port;
    }
    dns::options closed;
    closed.servers = {sgcl::string("127.0.0.1:" + std::to_string(port))};
    closed.timeout = 2s;
    closed.attempts = 1;
    auto t0 = std::chrono::steady_clock::now();
    auto refused = dns::lookup_mx("example.test", closed);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), std::errc::connection_refused);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 1500ms);
}

TEST(NetDnsRecords_Tests, TheSearchListAndNdots) {
    Fixture f;
    Server s(f.b);
    ResolvConfFile conf("nameserver 127.0.0.1\nport " + std::to_string(s.port) + "\nsearch corp.test\noptions ndots:1 timeout:1 attempts:1\n");
    auto r = dns::lookup_mx("mail");   // no dot: corp.test first
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(str(r->front().host), "in.corp.test.");
    EXPECT_EQ(f.b.queries(), std::vector<std::string>{"mail.corp.test. 15"});
    auto other = dns::lookup_mx("mail.other");   // one dot, ndots 1: the name first
    ASSERT_TRUE(other.has_value());
    EXPECT_EQ(str(other->front().host), "in.other.");
    EXPECT_EQ(f.b.queries().back(), "mail.other. 15");
    auto absolute = dns::lookup_mx("nothing.");   // absolute: no search
    ASSERT_FALSE(absolute.has_value());
    EXPECT_EQ(f.b.queries().back(), "nothing. 15");
    size_t before = f.b.queries().size();
    auto both = dns::lookup_mx("nothing");   // the list, then the name: the error is the name's
    ASSERT_FALSE(both.has_value());
    EXPECT_EQ(both.error().code(), net::errc::host_not_found);
    auto q = f.b.queries();
    EXPECT_EQ(std::vector<std::string>(q.begin() + ptrdiff_t(before), q.end()), (std::vector<std::string>{"nothing.corp.test. 15", "nothing. 15"}));
    // the options' server: the file's search list still applies
    auto o = dns::lookup_mx("mail", s.options());
    ASSERT_TRUE(o.has_value());
}

// Two servers: the first fails (SERVFAIL), the second answers; with
// rotate, the lookups start at each in turn
TEST(NetDnsRecords_Tests, TwoServersAndRotate) {
    Fixture first, second;
    Server a(first.b, "127.0.0.1");
    ASSERT_TRUE(a.ok());
    Server b(second.b, "::1", a.port);
    if (!b.ok()) {
        GTEST_SKIP() << "no IPv6 loopback, or its port taken";
    }
    first.b.rcode["example.test."] = 2;
    ResolvConfFile conf("nameserver 127.0.0.1\nnameserver ::1\nport " + std::to_string(a.port) + "\noptions timeout:1 attempts:1\n");
    auto r = dns::lookup_mx("example.test.");
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(first.b.udp_queries.load(), 1);
    EXPECT_EQ(second.b.udp_queries.load(), 1);
    first.b.rcode.clear();
    auto t = dns::lookup_txt("example.test.");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(first.b.udp_queries.load(), 2);
    EXPECT_EQ(second.b.udp_queries.load(), 1);   // the first answered: the second not asked
    ResolvConfFile rotating("nameserver 127.0.0.1\nnameserver ::1\nport " + std::to_string(a.port) + "\noptions timeout:1 attempts:1 rotate\n");
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(dns::lookup_ns("example.test.").has_value());
    }
    EXPECT_EQ(first.b.udp_queries.load() + second.b.udp_queries.load(), 7);
    EXPECT_GE(second.b.udp_queries.load(), 2);
    EXPECT_GE(first.b.udp_queries.load(), 3);
}

// Without a nameserver line the local machine's servers, 127.0.0.1 and
// ::1, are asked (at the file's port here)
TEST(NetDnsRecords_Tests, TheLocalServersWhenTheFileNamesNone) {
    Fixture f;
    Server s(f.b);
    ResolvConfFile conf("port " + std::to_string(s.port) + "\noptions timeout:1 attempts:1\n");
    auto r = dns::lookup_mx("example.test.");
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(f.b.udp_queries.load(), 1);
}

TEST(NetDnsRecords_Tests, UseVcAsksOverTCP) {
    Fixture f;
    Server s(f.b);
    ResolvConfFile conf("nameserver 127.0.0.1\nport " + std::to_string(s.port) + "\noptions use-vc timeout:1 attempts:1\n");
    auto r = dns::lookup_txt("example.test.");
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(f.b.udp_queries.load(), 0);
    EXPECT_EQ(f.b.tcp_queries.load(), 1);
}

// Many lookups at once from tasks, each its own socket and id
TEST(NetDnsRecords_Tests, ManyAtOnce) {
    Fixture f;
    Server s(f.b);
    dns::options o = s.options(2s, 2);
    auto many = [](dns::options o) -> async::task<int> {
        async::task_group g;
        std::atomic<int> ok{0};
        for (int i = 0; i < 64; ++i) {
            g.go([](dns::options o, std::atomic<int>* ok) -> async::task<> {
                auto r = co_await dns::async_lookup_mx("example.test", o);
                if (r && r->size() == 3) {
                    ++*ok;
                }
            }(o, &ok));
        }
        co_await g;
        co_return ok.load();
    };
    EXPECT_EQ(async::run(many(o)), 64);
    EXPECT_EQ(f.b.ids.size() >= 32, true);
}

// A real domain over the internet, through the system's servers: off by
// default (the tests leave the machine alone); run with
// --gtest_also_run_disabled_tests
TEST(NetDnsRecords_Tests, DISABLED_TheInternet) {
    auto mx = dns::lookup_mx("gmail.com");
    ASSERT_TRUE(mx.has_value()) << str(mx.error().message());
    EXPECT_FALSE(mx->empty());
    auto txt = dns::lookup_txt("google.com");
    ASSERT_TRUE(txt.has_value());
    auto ns = dns::lookup_ns("example.com");
    ASSERT_TRUE(ns.has_value());
    auto cname = dns::lookup_cname("www.github.com");
    ASSERT_TRUE(cname.has_value());
    EXPECT_EQ(str(*cname), "github.com.");
    auto srv = dns::lookup_srv("xmpp-server", "tcp", "jabber.org");
    ASSERT_TRUE(srv.has_value());
}
