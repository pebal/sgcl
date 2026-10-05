//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A DNS server on the loopback for the resolver's tests, written here from
// RFC 1035 §4 and §7.3 (not from the resolver's code: it reads queries and
// writes answers with the codec's reader and writer, and decides by
// itself): UDP and TCP on one port, answering from a zone of records held
// in plain std types. A recursive server's answers: the CNAME chain
// followed to its end (or, `chase` off, the first CNAME alone), NXDOMAIN
// for a name the zone does not have, NOERROR with an SOA for a name
// without the type asked. Its faults, each asked by a counter or a flag:
// queries dropped, an answer first with a wrong id, with another question,
// forged from another port, garbage; TC over UDP, an answer past the
// buffer without TC, a TCP answer cut after its length; an RCODE for a
// name; a delay. It counts what it saw.
#pragma once

#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/net.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace dns_test {
    using namespace sgcl;
    namespace nd = sgcl::net::detail;
    namespace type = sgcl::net::detail::dns_type;

    struct Rr {
        std::string owner;                 // "mx.example.test."
        uint16_t type = 0;
        uint16_t a = 0, b = 0, c = 0;      // MX: preference; SRV: priority, weight, port
        std::string target;                // MX, NS, CNAME, PTR, SRV
        std::vector<std::string> strings;  // TXT
        std::string raw;                   // A, AAAA, or any type: the rdata as it is
        uint32_t ttl = 300;
    };

    inline Rr rr_mx(std::string owner, uint16_t pref, std::string host) {
        Rr r;
        r.owner = std::move(owner);
        r.type = type::mx;
        r.a = pref;
        r.target = std::move(host);
        return r;
    }

    inline Rr rr_srv(std::string owner, uint16_t prio, uint16_t weight, uint16_t port, std::string target) {
        Rr r;
        r.owner = std::move(owner);
        r.type = type::srv;
        r.a = prio;
        r.b = weight;
        r.c = port;
        r.target = std::move(target);
        return r;
    }

    inline Rr rr_txt(std::string owner, std::vector<std::string> strings) {
        Rr r;
        r.owner = std::move(owner);
        r.type = type::txt;
        r.strings = std::move(strings);
        return r;
    }

    inline Rr rr_named(std::string owner, uint16_t t, std::string target) {
        Rr r;
        r.owner = std::move(owner);
        r.type = t;
        r.target = std::move(target);
        return r;
    }

    inline Rr rr_a(std::string owner, uint8_t x, uint8_t y, uint8_t z, uint8_t w) {
        Rr r;
        r.owner = std::move(owner);
        r.type = type::a;
        r.raw = std::string{char(x), char(y), char(z), char(w)};
        return r;
    }

    inline Rr rr_aaaa(std::string owner) {
        Rr r;
        r.owner = std::move(owner);
        r.type = type::aaaa;
        r.raw = std::string(16, '\0');
        r.raw[0] = char(0x20);
        r.raw[1] = char(0x01);
        r.raw[2] = char(0x0d);
        r.raw[3] = char(0xb8);
        r.raw[15] = 1;
        return r;
    }

    inline std::string lower(std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') {
                c = char(c - 'A' + 'a');
            }
        }
        return s;
    }

    inline nd::DnsName wire(const std::string& text) {
        nd::DnsName n;
        bool ok = nd::dns_name_from_text(text, n);
        (void)ok;
        return n;
    }

    inline std::string text_of(const nd::DnsName& n) {
        char t[nd::DnsMaxNameText];
        return std::string(t, nd::dns_name_to_text(n, t));
    }

    // What the server does and what it saw: std types and atomics, set
    // before the server starts and read by its tasks
    struct Behaviour {
        std::vector<Rr> zone;
        std::map<std::string, int> rcode;      // a name (lower case, absolute) answered with this RCODE
        std::set<std::string> truncate;        // names answered with TC over UDP
        bool chase = true;
        bool compress = true;
        bool recursion_available = true;
        bool lame = false;                     // every answer: NOERROR, empty, neither AA nor RA
        bool oversize = false;                 // a UDP answer padded past 1232 bytes, without TC
        bool tcp_cut = false;                  // a TCP answer: its length, then the connection closed
        std::chrono::milliseconds delay{0};
        std::atomic<int> drop{0};              // UDP queries left unanswered
        std::atomic<int> wrong_id{0};          // answers sent first with the id changed
        std::atomic<int> wrong_question{0};    // answers sent first to another question
        std::atomic<int> forged{0};            // answers sent first from another port, their MX changed
        std::atomic<int> garbage{0};           // datagrams of junk sent first
        std::atomic<int> udp_queries{0};
        std::atomic<int> tcp_queries{0};
        std::atomic<int> last_payload{-1};     // the EDNS UDP payload of the last query, -1 for none
        std::atomic<int> last_flags{0};
        std::atomic<int> live_connections{0};
        std::mutex m;
        std::vector<std::string> asked;        // "name type" of each query, in order
        std::set<int> ids;                     // the ids the queries carried

        bool take(std::atomic<int>& counter) {
            int v = counter.load();
            while (v > 0) {
                if (counter.compare_exchange_weak(v, v - 1)) {
                    return true;
                }
            }
            return false;
        }

        std::vector<std::string> queries() {
            std::lock_guard lock(m);
            return asked;
        }
    };

    struct Query {
        uint16_t id = 0;
        uint16_t flags = 0;
        nd::DnsName name;
        uint16_t type = 0;
        uint16_t klass = 0;
        int payload = -1;
    };

    inline bool read_query(const uint8_t* q, size_t n, Query& out) {
        nd::DnsReader r(q, n);
        nd::DnsHeader h;
        if (!r.header(h) || h.questions != 1 || (h.flags & nd::DnsFlagResponse)) {
            return false;
        }
        out.id = h.id;
        out.flags = h.flags;
        if (!r.question(out.name, out.type, out.klass)) {
            return false;
        }
        nd::DnsReader::Record rec;
        for (size_t i = 0; i < size_t(h.answers) + h.authorities + h.additionals; ++i) {
            if (!r.record(rec)) {
                return false;
            }
            if (rec.type == type::opt) {
                out.payload = rec.klass;
            }
        }
        return true;
    }

    inline bool write_rr(nd::DnsWriter& w, const Rr& r, bool compress) {
        if (!w.record_begin(wire(r.owner), r.type, nd::DnsClassIn, r.ttl, compress)) {
            return false;
        }
        switch (r.type) {
            case type::mx:
                w.u16(r.a);
                w.name(wire(r.target), compress);
                break;
            case type::srv:
                w.u16(r.a);
                w.u16(r.b);
                w.u16(r.c);
                w.name(wire(r.target), false);   // RFC 2782: no compression of the target
                break;
            case type::ns:
            case type::cname:
            case type::ptr:
                w.name(wire(r.target), compress);
                break;
            case type::txt:
                for (auto& s : r.strings) {
                    w.u8(uint8_t(s.size()));
                    w.bytes(s.data(), s.size());
                }
                break;
            default:
                w.bytes(r.raw.data(), r.raw.size());
        }
        return w.record_end();
    }

    // The answer to a query as the zone has it; `qname` in place of the
    // query's own name when given (the answer to another question)
    inline size_t answer(Behaviour& b, const Query& q, uint8_t* out, size_t cap, bool tcp, const nd::DnsName* other = nullptr, bool forged = false) {
        nd::DnsWriter w(out, cap);
        nd::DnsHeader h;
        h.id = q.id;
        h.flags = uint16_t(nd::DnsFlagResponse | (q.flags & nd::DnsFlagRecursionDesired));
        h.questions = 1;
        w.header(h);
        const nd::DnsName& asked = other ? *other : q.name;
        w.question(asked, q.type, q.klass, b.compress);
        std::string name = lower(text_of(q.name));
        if (b.lame) {
            w.set_header(h);
            return w.ok() ? w.size() : 0;
        }
        if (b.recursion_available) {
            h.flags |= nd::DnsFlagRecursionAvailable;
        }
        if (auto it = b.rcode.find(name); it != b.rcode.end()) {
            h.flags = uint16_t(h.flags | (it->second & 0xF));
            w.set_header(h);
            return w.ok() ? w.size() : 0;
        }
        if (!tcp && b.truncate.count(name)) {
            h.flags |= nd::DnsFlagTruncated;
            w.set_header(h);
            return w.ok() ? w.size() : 0;
        }
        std::string current = name;
        int answers = 0;
        for (int hops = 0; hops < 20; ++hops) {
            const Rr* alias = nullptr;
            for (auto& r : b.zone) {
                if (lower(r.owner) == current && r.type == type::cname && q.type != type::cname) {
                    alias = &r;
                }
            }
            if (!alias) {
                break;
            }
            write_rr(w, *alias, b.compress);
            ++answers;
            current = lower(alias->target);
            if (!b.chase) {
                h.answers = uint16_t(answers);
                w.set_header(h);
                return w.ok() ? w.size() : 0;
            }
        }
        bool exists = false;
        int found = 0;
        for (auto& r : b.zone) {
            if (lower(r.owner) == current) {
                exists = true;
                if (r.type == q.type) {
                    Rr copy = r;
                    if (forged && copy.type == type::mx) {
                        copy.target = "forged.example.";
                    }
                    write_rr(w, copy, b.compress);
                    ++found;
                }
            }
        }
        answers += found;
        h.answers = uint16_t(answers);
        if (!exists) {
            h.flags = uint16_t(h.flags | nd::dns_rcode::name_error);   // RFC 6604: the code is the chain's last name's
        }
        if (found == 0) {
            // the SOA of the negative answer (RFC 2308 §2.1)
            w.record_begin(wire("example.test."), type::soa, nd::DnsClassIn, 60, b.compress);
            w.name(wire("ns.example.test."), b.compress);
            w.name(wire("hostmaster.example.test."), b.compress);
            w.u32(1);
            w.u32(3600);
            w.u32(600);
            w.u32(86400);
            w.u32(60);
            w.record_end();
            h.authorities = 1;
            h.flags |= nd::DnsFlagAuthoritative;
        }
        if (q.payload >= 0) {
            w.opt(nd::DnsUdpPayload);
            h.additionals = 1;
        }
        if (!tcp && b.oversize) {
            // a TXT of padding in the additional section, past the payload
            w.record_begin(wire("pad.example.test."), type::txt, nd::DnsClassIn, 1);
            for (int i = 0; i < 8; ++i) {
                std::string pad(200, 'p');
                w.u8(200);
                w.bytes(pad.data(), pad.size());
            }
            w.record_end();
            ++h.additionals;
        }
        w.set_header(h);
        return w.ok() ? w.size() : 0;
    }

    inline void note(Behaviour& b, const Query& q) {
        std::lock_guard lock(b.m);
        b.asked.push_back(lower(text_of(q.name)) + " " + std::to_string(q.type));
        b.ids.insert(q.id);
        b.last_payload = q.payload;
        b.last_flags = q.flags;
    }

    inline async::task<> serve_udp(net::udp::socket s, Behaviour* b) {
        tracked_ptr<array<byte, 4096>> in = make_tracked<array<byte, 4096>>();
        tracked_ptr<array<byte, 4096>> out = make_tracked<array<byte, 4096>>();
        auto u8 = [](tracked_ptr<array<byte, 4096>>& p) { return reinterpret_cast<uint8_t*>(p->data()); };
        for (;;) {
            auto d = co_await s.async_receive_from(slice<byte>(in, in->data(), in->size()));
            if (!d) {
                co_return;
            }
            Query q;
            if (!read_query(u8(in), d->size, q)) {
                continue;
            }
            ++b->udp_queries;
            note(*b, q);
            if (b->take(b->drop)) {
                continue;
            }
            if (b->delay.count()) {
                co_await async::sleep(b->delay);
            }
            auto send = [&](size_t n) -> async::task<> {
                (void)co_await s.async_send_to(slice<const byte>(out, out->data(), n), d->from);
            };
            if (b->take(b->garbage)) {
                for (int i = 0; i < 5; ++i) {
                    (*out)[size_t(i)] = byte(0xA5);
                }
                co_await send(5);
            }
            if (b->take(b->wrong_id)) {
                Query other = q;
                other.id = uint16_t(q.id ^ 0x5A5A);
                co_await send(answer(*b, other, u8(out), out->size(), false));
            }
            if (b->take(b->wrong_question)) {
                nd::DnsName elsewhere = wire("elsewhere.example.test.");
                co_await send(answer(*b, q, u8(out), out->size(), false, &elsewhere));
            }
            if (b->take(b->forged)) {
                auto f = net::udp::bind("127.0.0.1:0");
                if (f) {
                    size_t n = answer(*b, q, u8(out), out->size(), false, nullptr, true);
                    (void)co_await f->async_send_to(slice<const byte>(out, out->data(), n), d->from);
                    (void)f->close();
                }
            }
            co_await send(answer(*b, q, u8(out), out->size(), false));
        }
    }

    inline async::task<> serve_connection(net::connection c, Behaviour* b) {
        tracked_ptr<array<byte, 8192>> in = make_tracked<array<byte, 8192>>();
        tracked_ptr<array<byte, 8192>> out = make_tracked<array<byte, 8192>>();
        for (;;) {
            auto r = co_await c.async_read_full(slice<byte>(in, in->data(), 2));
            if (!r || *r != 2) {
                break;
            }
            size_t len = size_t((*in)[0]) << 8 | size_t((*in)[1]);
            r = co_await c.async_read_full(slice<byte>(in, in->data(), len));
            if (!r || *r != len) {
                break;
            }
            Query q;
            if (!read_query(reinterpret_cast<uint8_t*>(in->data()), len, q)) {
                break;
            }
            ++b->tcp_queries;
            note(*b, q);
            size_t n = answer(*b, q, reinterpret_cast<uint8_t*>(out->data()) + 2, out->size() - 2, true);
            (*out)[0] = byte(n >> 8);
            (*out)[1] = byte(n);
            if (b->tcp_cut) {
                (void)co_await c.async_write(slice<const byte>(out, out->data(), 2));
                break;
            }
            if (!co_await c.async_write(slice<const byte>(out, out->data(), n + 2))) {
                break;
            }
        }
        (void)c.close();
        --b->live_connections;
    }

    inline async::task<> serve_tcp(net::listener l, Behaviour* b) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            ++b->live_connections;
            async::go(serve_connection(*c, b));
        }
    }

    // The server: UDP and TCP on one port of 127.0.0.1, for as long as the
    // object lives (on the test's stack)
    struct Server {
        Behaviour& b;
        net::udp::socket udp;
        net::listener tcp;
        async::task<> udp_task;
        async::task<> tcp_task;
        uint16_t port = 0;
        std::string host;

        // On `host` ("127.0.0.1", "::1"), at `wanted` or a free port
        explicit Server(Behaviour& behaviour, const std::string& host = "127.0.0.1", uint16_t wanted = 0)
        : b(behaviour) {
            std::string h = host.find(':') != std::string::npos ? "[" + host + "]" : host;
            for (int i = 0; i < 50 && !tcp; ++i) {
                auto u = net::udp::bind(string(h + ":" + std::to_string(wanted)));
                if (!u) {
                    break;
                }
                udp = *u;
                port = udp.local_endpoint().port();
                auto l = net::tcp::listen(string(h + ":" + std::to_string(port)));
                if (l) {
                    tcp = *l;
                } else {
                    (void)udp.close();
                    udp = net::udp::socket();
                    if (wanted) {
                        break;
                    }
                }
            }
            this->host = h;
            if (udp && tcp) {
                udp_task = async::spawn(serve_udp(udp, &b));
                tcp_task = async::spawn(serve_tcp(tcp, &b));
            }
        }

        bool ok() const {
            return udp && tcp;
        }

        ~Server() {
            if (!ok()) {
                return;
            }
            (void)udp.close();
            (void)tcp.close();
            udp_task.wait();
            tcp_task.wait();
            for (int i = 0; i < 500 && b.live_connections.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::string address() const {
            return host + ":" + std::to_string(port);
        }

        net::dns::options options(std::chrono::milliseconds timeout = std::chrono::milliseconds(500), int attempts = 1) const {
            net::dns::options o;
            o.servers = {string(address())};
            o.timeout = timeout;
            o.attempts = attempts;
            return o;
        }
    };
}
