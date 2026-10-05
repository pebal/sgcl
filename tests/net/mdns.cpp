//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Multicast DNS (RFC 6762) and DNS-SD (RFC 6763): net::mdns and
// net::dns_sd on the loopback interface alone, so that nothing leaves the
// machine. The messages read and written (canonical rdata, the QU and
// cache-flush bits, malformed input), the TXT record's rules (§6.4); our
// querier against our responder (a host's addresses, a service resolved,
// browsed, its subtypes, the types, withdrawn); two responders probing for
// one name, one after the other and at once (§8.2's tie-break), a peer of
// the test's that answers a probe with another address (§9); goodbye and
// a record left to expire (§10); the responder's known-answer suppression
// and legacy unicast answers (§7.1, §6.7), the querier's queries backing
// off with their known answers (§5.2, §7.1), a QU first query (§5.4);
// names under .local through net::dns; the boundaries; and the system's
// mDNSResponder through dns-sd(1), both ways, where it is installed.
#include "tests/types.h"
#include "sgcl/io/exec.h"
#include "sgcl/net/mdns.h"
#include "sgcl/net/net.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    namespace nd = sgcl::net::detail;
    using sgcl::net::mdns;
    namespace dns_sd = sgcl::net::dns_sd;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    net::network_interface loopback() {
        auto all = net::interfaces();
        EXPECT_TRUE(all.has_value());
        for (auto& i : *all) {
            if (i.loopback && i.up && i.multicast) {
                return i;
            }
        }
        ADD_FAILURE() << "no loopback interface able to multicast";
        return {};
    }

    mdns::options on_loopback(std::chrono::milliseconds timeout = 2000ms) {
        mdns::options o;
        o.interfaces.push_back(loopback());
        o.timeout = timeout;
        return o;
    }

    // A name of this run's own, so that two runs at once and the system's
    // responder never share one
    std::string unique(const std::string& base) {
        static std::atomic<int> n{0};
        return base + "-" + std::to_string(::getpid()) + "-" + std::to_string(n++);
    }

    nd::DnsName wire(const std::string& text) {
        nd::DnsName n;
        EXPECT_TRUE(nd::dns_name_from_text(text, n)) << text;
        return n;
    }

    // A peer of the test's on the loopback's group at 5353: raw messages
    // sent and read, as another host's responder or querier would
    struct Peer {
        net::udp::socket s;
        net::endpoint group;

        Peer() {
            auto lo = loopback();
            auto made = net::udp::listen_multicast("224.0.0.251:5353", lo);
            EXPECT_TRUE(made.has_value()) << (made ? "" : str(made.error().message()));
            s = *made;
            (void)s.set_multicast_ttl(255);
            group = net::endpoint(net::ip_address::v4(224, 0, 0, 251), 5353);
        }

        ~Peer() {
            (void)s.close();
        }

        void send(const std::vector<uint8_t>& m, const net::endpoint& to = {}) {
            auto r = s.send_to(std::string_view(reinterpret_cast<const char*>(m.data()), m.size()), to.is_valid() ? to : group);
            EXPECT_TRUE(r.has_value());
        }

        // The next message that reads and satisfies `pick`, within `wait`
        template<class F>
        std::optional<nd::MdnsMessage> next(F pick, std::chrono::milliseconds wait = 3000ms, net::endpoint* from = nullptr) {
            auto until = std::chrono::steady_clock::now() + wait;
            sgcl::vector<byte> buf(9000);
            for (;;) {
                auto left = until - std::chrono::steady_clock::now();
                if (left <= 0ms) {
                    return std::nullopt;
                }
                s.set_read_deadline(clock::now() + duration(std::chrono::duration_cast<std::chrono::nanoseconds>(left)));
                auto d = s.receive_from(buf);
                if (!d) {
                    return std::nullopt;
                }
                nd::MdnsMessage m;
                if (nd::mdns_read(reinterpret_cast<const uint8_t*>(buf.data()), d->size, m) && pick(m)) {
                    if (from) {
                        *from = d->from;
                    }
                    return m;
                }
            }
        }
    };

    std::vector<uint8_t> message(uint16_t id, uint16_t flags, const std::vector<nd::MdnsQuestion>& qs, const std::vector<nd::MdnsRecord>& answers,
                                 const std::vector<nd::MdnsRecord>& authorities = {}) {
        std::vector<uint8_t> buf(9000);
        nd::MdnsWriter w(buf.data(), buf.size(), id, flags);
        for (auto& q : qs) {
            EXPECT_TRUE(w.question(q.name, q.type, q.unicast));
        }
        for (auto& r : answers) {
            EXPECT_TRUE(w.record(0, r, r.ttl, r.unique && (flags & nd::DnsFlagResponse)));
        }
        for (auto& r : authorities) {
            EXPECT_TRUE(w.record(1, r, r.ttl, false));
        }
        buf.resize(w.finish());
        return buf;
    }

    nd::MdnsQuestion question(const std::string& name, uint16_t type, bool unicast = false) {
        nd::MdnsQuestion q;
        q.name = wire(name);
        q.type = type;
        q.unicast = unicast;
        return q;
    }

    nd::MdnsRecord a_record(const std::string& name, uint8_t last, uint32_t ttl = 120) {
        nd::MdnsRecord r;
        r.name = wire(name);
        r.type = nd::dns_type::a;
        r.unique = true;
        r.ttl = ttl;
        r.rdata = std::string{char(10), char(99), char(0), char(last)};
        return r;
    }

    nd::MdnsRecord ptr_record(const std::string& owner, const std::string& target, uint32_t ttl = 4500) {
        nd::MdnsRecord r;
        r.name = wire(owner);
        r.type = nd::dns_type::ptr;
        r.ttl = ttl;
        nd::mdns_append_name(r.rdata, wire(target));
        return r;
    }

    bool has_question(const nd::MdnsMessage& m, const std::string& name, uint16_t type) {
        auto n = wire(name);
        for (auto& q : m.questions) {
            if (q.type == type && q.name == n) {
                return true;
            }
        }
        return false;
    }

    dns_sd::service web(const std::string& name, uint16_t port, const std::string& type = "_sgcl-test._tcp") {
        dns_sd::service s;
        s.name = sgcl::string(name);
        s.type = sgcl::string(type);
        s.port = port;
        s.txt = {{"path", "/api"}, {"v", "1"}};
        return s;
    }

    // The next event of a browser within `wait`
    std::optional<dns_sd::event> next_event(const dns_sd::browser& b, std::chrono::milliseconds wait = 3000ms) {
        async::stop_source stop;
        stop.stop_after(wait);
        auto e = b.async_next(stop.token()).wait();
        if (!e) {
            return std::nullopt;
        }
        return *e;
    }
}

// --- the messages -------------------------------------------------------------

TEST(NetMdnsMessage_Tests, ReadAndWriteRoundTrip) {
    auto inst = ptr_record("_http._tcp.local.", "My Box._http._tcp.local.");
    nd::MdnsRecord srv;
    srv.name = wire("My Box._http._tcp.local.");
    srv.type = nd::dns_type::srv;
    srv.unique = true;
    srv.ttl = 120;
    nd::mdns_append_u16(srv.rdata, 0);
    nd::mdns_append_u16(srv.rdata, 0);
    nd::mdns_append_u16(srv.rdata, 8080);
    nd::mdns_append_name(srv.rdata, wire("box.local."));
    nd::MdnsRecord txt;
    txt.name = srv.name;
    txt.type = nd::dns_type::txt;
    txt.unique = true;
    txt.ttl = 4500;
    txt.rdata = nd::mdns_txt_rdata({"a=1", "flag"});
    auto bytes = message(7, nd::DnsFlagResponse | nd::DnsFlagAuthoritative, {question("box.local.", nd::dns_type::any, true)},
                         {inst, srv, txt, a_record("box.local.", 5)});
    nd::MdnsMessage m;
    ASSERT_TRUE(nd::mdns_read(bytes.data(), bytes.size(), m));
    EXPECT_EQ(m.header.id, 7);
    EXPECT_TRUE(m.is_response());
    ASSERT_EQ(m.questions.size(), 1u);
    EXPECT_TRUE(m.questions[0].unicast);
    EXPECT_EQ(m.questions[0].klass, nd::DnsClassIn);
    ASSERT_EQ(m.answers.size(), 4u);
    EXPECT_TRUE(m.answers[0].same(inst));
    EXPECT_FALSE(m.answers[0].unique);   // a PTR is shared: no cache-flush bit
    EXPECT_TRUE(m.answers[1].same(srv)); // the names compressed on the wire, canonical once read
    EXPECT_TRUE(m.answers[1].unique);
    EXPECT_TRUE(m.answers[2].same(txt));
    EXPECT_EQ(m.answers[3].rdata.size(), 4u);
    EXPECT_LT(bytes.size(), 200u);       // compressed
    // the records read compare by their bytes whatever the case of a name's letters
    auto upper = ptr_record("_HTTP._tcp.LOCAL.", "My Box._http._tcp.local.");
    EXPECT_TRUE(upper.same(inst));
    // a record too big for the rest of the buffer: left out, the message cut there
    std::vector<uint8_t> small(60);
    nd::MdnsWriter w(small.data(), small.size(), 0, nd::DnsFlagResponse);
    EXPECT_TRUE(w.record(0, a_record("x.local.", 1), 120, true));
    EXPECT_FALSE(w.record(0, srv, 120, true));
    EXPECT_TRUE(w.record(0, a_record("x.local.", 2), 120, true));
    size_t n = w.finish();
    ASSERT_TRUE(nd::mdns_read(small.data(), n, m));
    EXPECT_EQ(m.answers.size(), 2u);
    // a question after a record is refused, a record of an earlier section too
    EXPECT_FALSE(w.question(wire("y.local."), 1, false));
}

TEST(NetMdnsMessage_Tests, MalformedInputIsRefused) {
    nd::MdnsMessage m;
    EXPECT_FALSE(nd::mdns_read(nullptr, 0, m));
    uint8_t header[12] = {0, 0, 0x84, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    EXPECT_FALSE(nd::mdns_read(header, 12, m));   // a question counted, none there
    header[5] = 0;
    EXPECT_TRUE(nd::mdns_read(header, 12, m));    // an empty response
    header[7] = 200;                              // 200 answers in no bytes
    EXPECT_FALSE(nd::mdns_read(header, 12, m));
    header[7] = 0;
    header[2] = 0x84 | 0x28;                      // opcode 5
    EXPECT_FALSE(nd::mdns_read(header, 12, m));
    // an A record of five bytes, a PTR whose name does not read
    auto good = message(0, nd::DnsFlagResponse, {}, {a_record("x.local.", 1)});
    auto bad = good;
    bad[bad.size() - 5] = 5;                      // rdlength 4 -> 5, past the end
    EXPECT_FALSE(nd::mdns_read(bad.data(), bad.size(), m));
    auto ptr = message(0, nd::DnsFlagResponse, {}, {ptr_record("a.local.", "b.local.")});
    ptr[ptr.size() - 2] = 0xFF;                   // the target's pointer made one pointing forward
    ptr[ptr.size() - 1] = 0xFF;
    EXPECT_FALSE(nd::mdns_read(ptr.data(), ptr.size(), m));
    auto cut = message(0, nd::DnsFlagResponse, {}, {ptr_record("a.local.", "b.local.")});
    cut.pop_back();                               // a message cut inside its last record
    EXPECT_FALSE(nd::mdns_read(cut.data(), cut.size(), m));
    // past 9000 bytes
    std::vector<uint8_t> big(9001, 0);
    EXPECT_FALSE(nd::mdns_read(big.data(), big.size(), m));
}

// --- the TXT record ------------------------------------------------------------

TEST(NetMdnsTxt_Tests, TheRecordAtItsBoundaries) {
    dns_sd::txt_record t;
    EXPECT_TRUE(t.empty());
    EXPECT_FALSE(t.contains("a"));
    EXPECT_FALSE(t.get("a").has_value());
    EXPECT_FALSE(t.remove("a"));
    t.set("Path", "/x");
    t.set("flag");
    t.set("empty", "");
    EXPECT_EQ(t.size(), 3u);
    EXPECT_EQ(str(*t.get("path")), "/x");           // keys without regard to case
    EXPECT_TRUE(t.contains("FLAG"));
    EXPECT_FALSE(t.get("flag").has_value());         // a key alone has no value
    ASSERT_TRUE(t.get("empty").has_value());
    EXPECT_EQ(str(*t.get("empty")), "");
    t.set("PATH", "/y");                             // set again: its place kept
    ASSERT_EQ(t.entries().size(), 3u);
    EXPECT_EQ(str(t.entries()[0]), "PATH=/y");
    auto keys = t.keys();
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(str(keys[1]), "flag");
    t.set("bin", sgcl::string(std::string("a=b\0c", 5)));   // a value of any bytes, '=' among them
    EXPECT_EQ(t.get("bin")->size(), 5u);
    EXPECT_TRUE(t.remove("Flag"));
    EXPECT_EQ(t.size(), 3u);
    for (const char* bad : {"", "a=b", "tab\there", "\x7f"}) {
        SCOPED_TRACE(bad);
        EXPECT_THROW(t.set(bad, "v"), std::invalid_argument);
        EXPECT_THROW(t.set(bad), std::invalid_argument);
    }
    EXPECT_THROW(t.set("k", sgcl::string(std::string(254, 'v'))), std::invalid_argument);   // "k=" + 254: 256 bytes
    t.set("k", sgcl::string(std::string(253, 'v')));                                       // 255: the most
    EXPECT_EQ(t.get("k")->size(), 253u);
    dns_sd::txt_record u = {{"a", "1"}, {"b", "2"}};
    dns_sd::txt_record same = {{"a", "1"}, {"b", "2"}};
    EXPECT_TRUE(u == same);
    EXPECT_THROW((dns_sd::txt_record{{"", "1"}}), std::invalid_argument);
    dns_sd::txt_record moved = std::move(u);
    EXPECT_EQ(moved.size(), 2u);
}

TEST(NetMdnsTxt_Tests, TheWireRules) {
    std::vector<std::string> strings;
    ASSERT_TRUE(nd::mdns_txt_strings(std::string("\x03" "a=1" "\x00" "\x04" "=bad" "\x03" "A=2" "\x04" "flag", 19), strings));
    EXPECT_EQ(strings.size(), 5u);
    auto entries = nd::mdns_txt_entries(strings);
    // §6.4: the empty string and one starting with '=' dropped, the second "a" ignored
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0], "a=1");
    EXPECT_EQ(entries[1], "flag");
    EXPECT_FALSE(nd::mdns_txt_strings(std::string("\x05" "ab", 3), strings));   // a length past the end
    EXPECT_TRUE(nd::mdns_txt_strings(std::string(), strings));
    EXPECT_TRUE(strings.empty());
    EXPECT_EQ(nd::mdns_txt_rdata({}), std::string(1, '\0'));                  // §6.1: one empty string
    EXPECT_EQ(nd::mdns_txt_rdata({"k=v"}), std::string("\x03k=v"));
}

// --- querier and responder ---------------------------------------------------------

TEST(NetMdns_Tests, LookupTheRespondersHost) {
    auto o = on_loopback();
    std::string host = unique("sgcl-host");
    o.host = sgcl::string(host);
    auto r = mdns::responder::start(o);
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(str(r->host_name()), host + ".local.");
    auto found = mdns::lookup(sgcl::string(host), o);
    ASSERT_TRUE(found.has_value()) << str(found.error().message());
    std::set<std::string> got;
    for (auto& a : *found) {
        got.insert(str(a.to_string()));
    }
    EXPECT_TRUE(got.count("127.0.0.1")) << got.size();
    EXPECT_TRUE(got.count("::1"));
    EXPECT_TRUE((*found)[0].is_v4());             // IPv4's first
    // ".local" written or not, the task form, a trailing dot
    EXPECT_TRUE(mdns::lookup(sgcl::string(host + ".local"), o).has_value());
    EXPECT_TRUE(mdns::async_lookup(sgcl::string(host + ".local."), o).wait().has_value());
    // nobody of that name: not found after the timeout
    auto quick = on_loopback(300ms);
    auto t0 = std::chrono::steady_clock::now();
    auto none = mdns::lookup(sgcl::string(unique("nobody")), quick);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::errc::host_not_found);
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 250ms);
    r->close();
    r->close();   // a second close: nothing
}

TEST(NetMdns_Tests, PublishResolveBrowseRemove) {
    auto o = on_loopback();
    o.host = sgcl::string(unique("sgcl-pub"));
    auto r = mdns::responder::start(o);
    ASSERT_TRUE(r.has_value());
    std::string type = "_" + std::to_string(::getpid() % 100000) + "a._tcp";
    auto b = dns_sd::browse(sgcl::string(type), o);
    ASSERT_TRUE(b.has_value()) << str(b.error().message());
    auto s = web("Kitchen Display", 8080, type);
    s.subtypes = {sgcl::string("_kiosk")};
    auto name = r->publish(s);
    ASSERT_TRUE(name.has_value()) << str(name.error().message());
    EXPECT_EQ(str(*name), "Kitchen Display");
    auto e = next_event(*b);
    ASSERT_TRUE(e.has_value());
    EXPECT_TRUE(e->added);
    EXPECT_EQ(str(e->name), "Kitchen Display");
    EXPECT_EQ(str(e->type), type);
    EXPECT_EQ(str(e->domain), "local.");
    EXPECT_EQ(e->interface, loopback().index);
    auto found = dns_sd::resolve(e->name, e->type, o);
    ASSERT_TRUE(found.has_value()) << str(found.error().message());
    EXPECT_EQ(found->port, 8080);
    EXPECT_EQ(str(found->host), str(r->host_name()));
    EXPECT_EQ(str(*found->txt.get("path")), "/api");
    EXPECT_EQ(str(*found->txt.get("v")), "1");
    EXPECT_FALSE(found->addresses.empty());
    // the task form, the type with its domain
    auto again = dns_sd::async_resolve("Kitchen Display", sgcl::string(type + ".local."), o).wait();
    ASSERT_TRUE(again.has_value()) << str(again.error().message());
    EXPECT_EQ(again->port, 8080);
    auto services = r->services();
    ASSERT_EQ(services.size(), 1u);
    EXPECT_EQ(str(services[0].name), "Kitchen Display");
    // the subtype browsed
    auto sub = dns_sd::browse(sgcl::string("_kiosk._sub." + type), o);
    ASSERT_TRUE(sub.has_value()) << str(sub.error().message());
    auto se = next_event(*sub);
    ASSERT_TRUE(se.has_value());
    EXPECT_EQ(str(se->name), "Kitchen Display");
    EXPECT_EQ(str(se->type), type);
    sub->close();
    // the types of the link hold it
    auto types = dns_sd::types(on_loopback(1500ms));
    ASSERT_TRUE(types.has_value());
    EXPECT_NE(std::find(types->begin(), types->end(), sgcl::string(type)), types->end());
    // withdrawn: the browser sees it go, at once (its goodbye)
    auto t0 = std::chrono::steady_clock::now();
    EXPECT_TRUE(r->remove("Kitchen Display", sgcl::string(type)));
    EXPECT_FALSE(r->remove("Kitchen Display", sgcl::string(type)));
    auto gone = next_event(*b);
    ASSERT_TRUE(gone.has_value());
    EXPECT_FALSE(gone->added);
    EXPECT_EQ(str(gone->name), "Kitchen Display");
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 2500ms);
    EXPECT_TRUE(r->services().empty());
    b->close();
    auto closed = b->next();
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code(), io::errc::closed);
    r->close();
}

TEST(NetMdns_Tests, ASecondResponderOfTheSameServiceIsRenamed) {
    auto o = on_loopback();
    std::string type = "_" + std::to_string(::getpid() % 100000) + "b._tcp";
    auto first = dns_sd::publish(web("Printer", 631, type), o);
    ASSERT_TRUE(first.has_value()) << str(first.error().message());
    auto second = dns_sd::publish(web("Printer", 632, type), o);   // another port: a conflicting SRV
    ASSERT_TRUE(second.has_value());
    auto s1 = first->services(), s2 = second->services();
    ASSERT_EQ(s1.size(), 1u);
    ASSERT_EQ(s2.size(), 1u);
    EXPECT_EQ(str(s1[0].name), "Printer");
    EXPECT_EQ(str(s2[0].name), "Printer (2)");
    auto r1 = dns_sd::resolve("Printer", sgcl::string(type), o);
    auto r2 = dns_sd::resolve("Printer (2)", sgcl::string(type), o);
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->port, 631);
    EXPECT_EQ(r2->port, 632);
    // the same service again on a third: identical records are no conflict? another
    // port again: "(3)"
    auto third = dns_sd::publish(web("Printer", 633, type), o);
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(str(third->services()[0].name), "Printer (3)");
    first->close();
    second->close();
    third->close();
}

TEST(NetMdns_Tests, SimultaneousProbesAreTieBroken) {
    auto o = on_loopback();
    std::string type = "_" + std::to_string(::getpid() % 100000) + "c._tcp";
    auto x = async::spawn(dns_sd::async_publish(web("Speaker", 7000, type), o));
    auto y = async::spawn(dns_sd::async_publish(web("Speaker", 7001, type), o));
    auto rx = x.wait();
    auto ry = y.wait();
    ASSERT_TRUE(rx.has_value());
    ASSERT_TRUE(ry.has_value());
    std::set<std::string> names = {str(rx->services()[0].name), str(ry->services()[0].name)};
    EXPECT_EQ(names, (std::set<std::string>{"Speaker", "Speaker (2)"}));
    // §8.2: the lexicographically later records win: the port 7001's SRV
    auto& winner = ry->services()[0].name == "Speaker" ? *ry : *rx;
    EXPECT_EQ(winner.services()[0].port, 7001);
    rx->close();
    ry->close();
}

TEST(NetMdns_Tests, APeerHoldingTheHostNameRenamesIt) {
    Peer peer;
    std::string host = unique("sgcl-taken");
    std::string fqdn = host + ".local.";
    // the peer answers every probe of the name with an address of its own
    std::atomic<bool> done{false};
    sgcl::thread answering([&] {
        while (!done) {
            auto m = peer.next([&](const nd::MdnsMessage& m) { return !m.is_response() && has_question(m, fqdn, nd::dns_type::any); }, 200ms);
            if (m) {
                peer.send(message(0, nd::DnsFlagResponse | nd::DnsFlagAuthoritative, {}, {a_record(fqdn, 77)}));
            }
        }
    });
    auto o = on_loopback();
    o.host = sgcl::string(host);
    auto r = mdns::responder::start(o);
    done = true;
    answering.join();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(str(r->host_name()), host + "-2.local.");
    r->close();
}

TEST(NetMdns_Tests, GoodbyeAndCacheExpiry) {
    auto o = on_loopback();
    std::string type = "_" + std::to_string(::getpid() % 100000) + "d._tcp";
    std::string owner = type + ".local.";
    auto b = dns_sd::browse(sgcl::string(type), o);
    ASSERT_TRUE(b.has_value());
    Peer peer;
    // a record of two seconds, never refreshed
    peer.send(message(0, nd::DnsFlagResponse, {}, {ptr_record(owner, "Short Lived." + owner, 2)}));
    auto added = next_event(*b);
    ASSERT_TRUE(added.has_value());
    EXPECT_TRUE(added->added);
    EXPECT_EQ(str(added->name), "Short Lived");
    auto t0 = std::chrono::steady_clock::now();
    auto expired = next_event(*b, 4000ms);
    ASSERT_TRUE(expired.has_value());
    EXPECT_FALSE(expired->added);
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 1500ms);
    // the querier asked for it again before it ended (80% of its TTL: refresh)
    // a goodbye: gone a second later (§10.1)
    peer.send(message(0, nd::DnsFlagResponse, {}, {ptr_record(owner, "Bye." + owner, 4500)}));
    auto bye_added = next_event(*b);
    ASSERT_TRUE(bye_added.has_value());
    EXPECT_EQ(str(bye_added->name), "Bye");
    t0 = std::chrono::steady_clock::now();
    peer.send(message(0, nd::DnsFlagResponse, {}, {ptr_record(owner, "Bye." + owner, 0)}));
    auto bye = next_event(*b);
    ASSERT_TRUE(bye.has_value());
    EXPECT_FALSE(bye->added);
    auto took = std::chrono::steady_clock::now() - t0;
    EXPECT_GE(took, 800ms);
    EXPECT_LT(took, 2500ms);
    // a response from a port other than 5353 is ignored (§6)
    auto other = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(other.has_value());
    ASSERT_TRUE(other->set_multicast_interface(loopback()).has_value());
    auto m = message(0, nd::DnsFlagResponse, {}, {ptr_record(owner, "Spoof." + owner, 4500)});
    (void)other->send_to(std::string_view(reinterpret_cast<const char*>(m.data()), m.size()), peer.group);
    EXPECT_FALSE(next_event(*b, 700ms).has_value());
    b->close();
}

TEST(NetMdns_Tests, KnownAnswersAndLegacyUnicast) {
    auto o = on_loopback();
    std::string type = "_" + std::to_string(::getpid() % 100000) + "e._tcp";
    auto r = dns_sd::publish(web("Camera", 554, type), o);
    ASSERT_TRUE(r.has_value());
    std::string owner = type + ".local.";
    std::string instance = "Camera." + owner;
    // a one-shot query from a port of its own: a unicast answer to it, its id
    // and question echoed, no TTL past 10 s, no cache-flush bit (§6.7)
    auto q = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(q.has_value());
    ASSERT_TRUE(q->set_multicast_interface(loopback()).has_value());
    auto ask = message(0x1234, 0, {question(owner, nd::dns_type::ptr)}, {});
    auto group = net::endpoint(net::ip_address::v4(224, 0, 0, 251), 5353);
    ASSERT_TRUE(q->send_to(std::string_view(reinterpret_cast<const char*>(ask.data()), ask.size()), group).has_value());
    sgcl::vector<byte> buf(9000);
    q->set_read_deadline(clock::now() + 2 * second);
    nd::MdnsMessage m;
    bool answered = false;
    for (int i = 0; i < 10 && !answered; ++i) {
        auto d = q->receive_from(buf);
        ASSERT_TRUE(d.has_value()) << str(d.error().message());
        if (!nd::mdns_read(reinterpret_cast<const uint8_t*>(buf.data()), d->size, m) || m.header.id != 0x1234) {
            continue;
        }
        answered = true;
        EXPECT_EQ(d->from.port(), 5353);
        ASSERT_EQ(m.questions.size(), 1u);
        EXPECT_TRUE(m.questions[0].name == wire(owner));
        ASSERT_FALSE(m.answers.empty());
        for (auto& a : m.answers) {
            EXPECT_LE(a.ttl, 10u);
            EXPECT_FALSE(a.unique);
        }
        bool srv_additional = false;
        for (auto& a : m.additionals) {
            EXPECT_LE(a.ttl, 10u);
            srv_additional = srv_additional || a.type == nd::dns_type::srv;
        }
        EXPECT_TRUE(srv_additional);   // DNS-SD §12: the PTR's SRV beside it
    }
    EXPECT_TRUE(answered);
    // the same with the answer known: no answer (§7.1)
    auto known = message(0x2345, 0, {question(owner, nd::dns_type::ptr)}, {ptr_record(owner, instance, 4500)});
    ASSERT_TRUE(q->send_to(std::string_view(reinterpret_cast<const char*>(known.data()), known.size()), group).has_value());
    q->set_read_deadline(clock::now() + 800 * millisecond);
    bool any = false;
    for (;;) {
        auto d = q->receive_from(buf);
        if (!d) {
            break;
        }
        any = any || (nd::mdns_read(reinterpret_cast<const uint8_t*>(buf.data()), d->size, m) && m.header.id == 0x2345);
    }
    EXPECT_FALSE(any);
    // a known answer of less than half the TTL does not count
    auto stale = message(0x3456, 0, {question(owner, nd::dns_type::ptr)}, {ptr_record(owner, instance, 100)});
    ASSERT_TRUE(q->send_to(std::string_view(reinterpret_cast<const char*>(stale.data()), stale.size()), group).has_value());
    q->set_read_deadline(clock::now() + 2 * second);
    bool fresh = false;
    for (int i = 0; i < 10 && !fresh; ++i) {
        auto d = q->receive_from(buf);
        if (!d) {
            break;
        }
        fresh = nd::mdns_read(reinterpret_cast<const uint8_t*>(buf.data()), d->size, m) && m.header.id == 0x3456;
    }
    EXPECT_TRUE(fresh);
    r->close();
}

TEST(NetMdns_Tests, ContinuousQueriesBackOffWithKnownAnswers) {
    Peer peer;
    auto o = on_loopback();
    std::string type = "_" + std::to_string(::getpid() % 100000) + "f._tcp";
    std::string owner = type + ".local.";
    auto t0 = std::chrono::steady_clock::now();
    auto b = dns_sd::browse(sgcl::string(type), o);
    ASSERT_TRUE(b.has_value());
    auto is_ours = [&](const nd::MdnsMessage& m) { return !m.is_response() && has_question(m, owner, nd::dns_type::ptr); };
    auto first = peer.next(is_ours, 1000ms);
    ASSERT_TRUE(first.has_value());
    auto at1 = std::chrono::steady_clock::now() - t0;
    EXPECT_GE(at1, 15ms);    // §5.2: 20-120 ms first
    EXPECT_LT(at1, 600ms);
    EXPECT_TRUE(first->answers.empty());
    // an answer comes: the next query lists it as known
    peer.send(message(0, nd::DnsFlagResponse, {}, {ptr_record(owner, "Lamp." + owner, 4500)}));
    auto e = next_event(*b);
    ASSERT_TRUE(e.has_value());
    auto second = peer.next(is_ours, 2000ms);
    ASSERT_TRUE(second.has_value());
    auto at2 = std::chrono::steady_clock::now() - t0;
    ASSERT_EQ(second->answers.size(), 1u);
    EXPECT_TRUE(second->answers[0].same(ptr_record(owner, "Lamp." + owner)));
    auto third = peer.next(is_ours, 3000ms);
    ASSERT_TRUE(third.has_value());
    auto at3 = std::chrono::steady_clock::now() - t0;
    // a second, then two
    EXPECT_GE(at2 - at1, 800ms);
    EXPECT_GE(at3 - at2, 1700ms);
    b->close();
}

TEST(NetMdns_Tests, AFirstQueryAsksForUnicast) {
    Peer peer;
    auto o = on_loopback();
    std::string host = unique("sgcl-qu");
    o.host = sgcl::string(host);
    auto r = mdns::responder::start(o);
    ASSERT_TRUE(r.has_value());
    auto ask = on_loopback();
    ask.unicast_response = true;
    std::string fqdn = host + ".local.";
    auto looked = async::spawn(mdns::async_lookup(sgcl::string(host), ask));
    auto q = peer.next([&](const nd::MdnsMessage& m) { return !m.is_response() && has_question(m, fqdn, nd::dns_type::a); }, 2000ms);
    ASSERT_TRUE(q.has_value());
    EXPECT_TRUE(q->questions[0].unicast);
    auto found = looked.wait();
    EXPECT_TRUE(found.has_value()) << str(found.error().message());
    r->close();
}

TEST(NetMdns_Tests, LocalNamesThroughDns) {
    auto o = on_loopback();
    std::string host = unique("sgcl-dns");
    o.host = sgcl::string(host);
    std::string type = "_" + std::to_string(::getpid() % 100000) + "g._tcp";
    auto r = mdns::responder::start(o);
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(r->publish(web("Via Dns", 9000, type)).has_value());
    // net::dns's own resolver sends a name under .local to multicast DNS
    // (every interface by default; the loopback alone here, so that nothing
    // leaves the machine)
    nd::mdns_default_interfaces().push_back(loopback());
    struct Restore {
        ~Restore() {
            nd::mdns_default_interfaces().clear();
        }
    } restore;
    net::dns::options d;
    d.timeout = 2s;
    auto ips = net::dns::lookup(sgcl::string(host + ".local"), d);
    ASSERT_TRUE(ips.has_value()) << str(ips.error().message());
    bool loop = false;
    for (auto& a : *ips) {
        loop = loop || a.is_loopback();
    }
    EXPECT_TRUE(loop);
    auto srv = net::dns::lookup_srv("", "", sgcl::string("Via Dns." + type + ".local"), d);
    ASSERT_TRUE(srv.has_value()) << str(srv.error().message());
    EXPECT_EQ((*srv)[0].port, 9000);
    auto txt = net::dns::lookup_txt(sgcl::string("Via Dns." + type + ".local"), d);
    ASSERT_TRUE(txt.has_value());
    EXPECT_EQ(str((*txt)[0]), "path=/apiv=1");
    net::dns::options quick;
    quick.timeout = 300ms;
    auto none = net::dns::lookup_mx(sgcl::string(unique("nobody") + ".local"), quick);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::errc::host_not_found);
    r->close();
}

TEST(NetMdns_Tests, TheOneLineFormOfPublish) {
    // every interface by default: the loopback alone here
    nd::mdns_default_interfaces().push_back(loopback());
    struct Restore {
        ~Restore() {
            nd::mdns_default_interfaces().clear();
        }
    } restore;
    std::string type = "_" + std::to_string(::getpid() % 100000) + "k._tcp";
    auto r = dns_sd::publish("One Line", sgcl::string(type), 4242, {{"k", "v"}});
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    auto found = dns_sd::resolve("One Line", sgcl::string(type));
    ASSERT_TRUE(found.has_value()) << str(found.error().message());
    EXPECT_EQ(found->port, 4242);
    EXPECT_EQ(str(*found->txt.get("k")), "v");
    auto plain = dns_sd::publish("Plain", sgcl::string(type), 1);   // no TXT: one empty string (§6.1)
    ASSERT_TRUE(plain.has_value());
    auto p = dns_sd::resolve("Plain", sgcl::string(type));
    ASSERT_TRUE(p.has_value());
    EXPECT_TRUE(p->txt.empty());
    EXPECT_EQ(dns_sd::publish("", sgcl::string(type), 1).error().code(), std::errc::invalid_argument);
    r->close();
    plain->close();
}

TEST(NetMdns_Tests, Boundaries) {
    auto o = on_loopback(300ms);
    // a browse of a type that is no type
    for (const char* bad : {"", "_http", "http._tcp", "_http._sctp", "_x._sub", "_toolongservicename16._tcp", "_a b._tcp"}) {
        SCOPED_TRACE(bad);
        auto b = dns_sd::browse(bad, o);
        ASSERT_FALSE(b.has_value());
        EXPECT_EQ(b.error().code(), std::errc::invalid_argument);
    }
    // resolve: nobody, a bad name, a stop
    auto none = dns_sd::resolve("Nobody", "_http._tcp", o);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::errc::host_not_found);
    EXPECT_EQ(dns_sd::resolve("", "_http._tcp", o).error().code(), std::errc::invalid_argument);
    EXPECT_EQ(dns_sd::resolve(sgcl::string(std::string(64, 'x')), "_http._tcp", o).error().code(), std::errc::invalid_argument);
    async::stop_source stop;
    stop.request_stop();
    auto stopped = dns_sd::async_resolve("Nobody", "_http._tcp", on_loopback(5000ms), stop.token()).wait();
    ASSERT_FALSE(stopped.has_value());
    EXPECT_EQ(stopped.error().code(), std::errc::operation_canceled);
    auto lstop = mdns::async_lookup("nobody-at-all", on_loopback(5000ms), stop.token()).wait();
    ASSERT_FALSE(lstop.has_value());
    EXPECT_EQ(lstop.error().code(), std::errc::operation_canceled);
    EXPECT_EQ(mdns::lookup("", o).error().code(), net::errc::host_not_found);
    // a browser's next with a stop
    auto b = dns_sd::browse(sgcl::string("_" + std::to_string(::getpid() % 100000) + "h._tcp"), o);
    ASSERT_TRUE(b.has_value());
    async::stop_source soon;
    soon.stop_after(100ms);
    auto n = b->async_next(soon.token()).wait();
    ASSERT_FALSE(n.has_value());
    EXPECT_EQ(n.error().code(), std::errc::operation_canceled);
    dns_sd::browser copy = *b;   // a copy is the same browse
    EXPECT_TRUE(copy == *b);
    copy.close();
    EXPECT_EQ(b->next().error().code(), io::errc::closed);
    b->close();   // twice: nothing
    // empty handles
    EXPECT_FALSE(dns_sd::browser());
    EXPECT_FALSE(mdns::responder());
    // a responder: a bad host name, a bad service
    auto bad_host = on_loopback();
    bad_host.host = "a.b";
    EXPECT_EQ(mdns::responder::start(bad_host).error().code(), std::errc::invalid_argument);
    auto ro = on_loopback();
    ro.host = sgcl::string(unique("sgcl-bounds"));
    auto r = mdns::responder::start(ro);
    ASSERT_TRUE(r.has_value());
    for (auto s : {web("", 1), web("x", 1, "_http"), web(std::string(64, 'n'), 1)}) {
        auto p = r->publish(s);
        ASSERT_FALSE(p.has_value());
        EXPECT_EQ(p.error().code(), std::errc::invalid_argument);
    }
    auto sub = web("Sub", 1, "_http._tcp");
    sub.subtypes = {sgcl::string("nounderscore")};
    EXPECT_EQ(r->publish(sub).error().code(), std::errc::invalid_argument);
    EXPECT_FALSE(r->remove("never", "_http._tcp"));
    mdns::responder same = *r;
    EXPECT_TRUE(same == *r);
    r->close();
    auto after = same.publish(web("Late", 1, "_http._tcp"));
    ASSERT_FALSE(after.has_value());
    EXPECT_EQ(after.error().code(), io::errc::closed);
    // an interface that cannot multicast: none joined
    net::network_interface fake;
    fake.name = "nonexistent0";
    fake.index = 9999;
    mdns::options nowhere;
    nowhere.interfaces.push_back(fake);
    EXPECT_FALSE(mdns::responder::start(nowhere).has_value());
}

// --- the system's responder ------------------------------------------------------

namespace {
    bool have_dns_sd() {
        return ::access("/usr/bin/dns-sd", X_OK) == 0 && ::access("/usr/bin/perl", X_OK) == 0;
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    struct DnsSd {
        io::command cmd;
        std::string log;
        bool started = false;

        // dns-sd runs until it is killed: started under perl's alarm, so that
        // a test process that dies first leaves it a minute at most
        static sgcl::vector<sgcl::string> capped(sgcl::vector<sgcl::string> args) {
            sgcl::vector<sgcl::string> all;
            all.push_back("-e");
            all.push_back("alarm 60; exec @ARGV");
            all.push_back("/usr/bin/dns-sd");
            for (auto& a : args) {
                all.push_back(a);
            }
            return all;
        }

        explicit DnsSd(sgcl::vector<sgcl::string> args)
        : cmd(sgcl::string("/usr/bin/perl"), capped(std::move(args))) {
            static std::atomic<int> n{0};
            log = (std::filesystem::temp_directory_path() / ("sgcl_dnssd_" + std::to_string(::getpid()) + "_" + std::to_string(n++) + ".log")).string();
            auto f = io::create(sgcl::string(log));
            if (f) {
                cmd.out = *f;
                cmd.err = *f;
                started = (bool)cmd.start();
                (void)f->close();
            }
        }

        std::string wait_for(const std::string& what, std::chrono::milliseconds wait = 5000ms) {
            for (auto until = std::chrono::steady_clock::now() + wait; std::chrono::steady_clock::now() < until;) {
                std::string s = slurp(log);
                if (s.find(what) != std::string::npos) {
                    return s;
                }
                std::this_thread::sleep_for(20ms);
            }
            return "";
        }

        ~DnsSd() {
            if (started) {
                (void)cmd.process.kill();
                (void)cmd.wait();
            }
            std::error_code e;
            std::filesystem::remove(log, e);
        }
    };
}

// The service registered with dns-sd -R (mDNSResponder, which answers on
// the loopback too): browsed and resolved by ours on the loopback
TEST(NetMdnsInterop_Tests, OursFindWhatDnsSdRegisters) {
    if (!have_dns_sd()) {
        GTEST_SKIP() << "no dns-sd (macOS's mDNSResponder is the peer)";
    }
    std::string type = "_" + std::to_string(::getpid() % 100000) + "i._tcp";
    std::string name = "SGCL Interop " + std::to_string(::getpid());
    DnsSd reg({"-R", sgcl::string(name), sgcl::string(type), "local", "4321", "path=/x", "flag"});
    ASSERT_TRUE(reg.started);
    ASSERT_FALSE(reg.wait_for("registered").empty()) << slurp(reg.log);
    auto o = on_loopback(3000ms);
    auto b = dns_sd::browse(sgcl::string(type), o);
    ASSERT_TRUE(b.has_value());
    auto e = next_event(*b, 5000ms);
    ASSERT_TRUE(e.has_value());
    EXPECT_TRUE(e->added);
    EXPECT_EQ(str(e->name), name);
    auto s = dns_sd::resolve(e->name, e->type, o);
    ASSERT_TRUE(s.has_value()) << str(s.error().message());
    EXPECT_EQ(s->port, 4321);
    EXPECT_EQ(str(*s->txt.get("path")), "/x");
    EXPECT_TRUE(s->txt.contains("flag"));
    EXPECT_FALSE(s->addresses.empty());
    b->close();
}

// Ours published on the loopback: browsed and resolved by dns-sd -B and -L
TEST(NetMdnsInterop_Tests, DnsSdFindsWhatOursPublish) {
    if (!have_dns_sd()) {
        GTEST_SKIP() << "no dns-sd (macOS's mDNSResponder is the peer)";
    }
    std::string type = "_" + std::to_string(::getpid() % 100000) + "j._tcp";
    std::string name = "Ours " + std::to_string(::getpid());
    auto o = on_loopback();
    o.host = sgcl::string(unique("sgcl-interop"));
    auto s = web(name, 5555, type);
    s.txt = {{"k", "v"}};
    auto r = dns_sd::publish(s, o);
    ASSERT_TRUE(r.has_value());
    {
        DnsSd browse({"-B", sgcl::string(type), "local"});
        auto out = browse.wait_for(name);
        EXPECT_FALSE(out.empty()) << slurp(browse.log);
        EXPECT_NE(out.find("Add"), std::string::npos);
    }
    {
        DnsSd lookup({"-L", sgcl::string(name), sgcl::string(type), "local"});
        auto out = lookup.wait_for("k=v");
        EXPECT_FALSE(out.empty()) << slurp(lookup.log);
        EXPECT_NE(out.find(str(r->host_name()) + ":5555"), std::string::npos) << out;
    }
    r->close();
}
