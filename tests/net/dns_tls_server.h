//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A DNS-over-TLS server on the loopback for the resolver's tests (RFC 7858),
// over the zone and the answers of tests/net/dns_server.h: TLS 1.3 with the
// test certificates of tests/net/tls_testdata (a leaf for localhost,
// 127.0.0.1 and ::1 under a test CA), the messages framed by their length
// as over TCP. It reads the queries of a connection as they come and
// answers each in a task of its own, so that several are in flight on one
// connection; its faults: the first answers held back (so that a later
// query is answered first), an answer with another id, a message too short
// to be one, the connection closed after a number of answers, no answer at
// all. It counts the connections and the queries, and keeps the size of
// each query and whether it carried the Padding option (RFC 7830).
#pragma once

#include "tests/net/dns_server.h"
#include "tests/source_root.h"
#include "sgcl/net/tls.h"

#include <fstream>
#include <sstream>

namespace dns_test {
    inline std::string tls_testdata(const std::string& name) {
        std::ifstream in(source_root() / "tests/net/tls_testdata" / name);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    inline net::tls::config dot_server_tls(std::vector<std::string> alpn = {"dot"}) {
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(tls_testdata("ecdsa.pem")), sgcl::string(tls_testdata("ecdsa.key")))};
        for (auto& a : alpn) {
            c.alpn.push_back(sgcl::string(a));
        }
        return c;
    }

    // The base64 SHA-256 of the test leaf's key: its pin (RFC 7858 §4.2)
    inline std::string leaf_pin() {
        auto cert = crypto::x509::certificate::from_pem(sgcl::string(tls_testdata("ecdsa.pem")));
        auto digest = crypto::sha256::of(cert->raw_subject_public_key_info());
        return std::string(encoding::base64::standard.encode(slice<const byte>(digest)).view());
    }

    inline std::string ca_pem() {
        return tls_testdata("ca.pem");
    }

    struct DotBehaviour {
        std::atomic<int> hold{0};            // the first answers held back this long, in ms, each
        std::atomic<int> wrong_id{0};        // answers sent with another id in place of their own
        std::atomic<int> runt{0};            // a message of three bytes in place of an answer
        std::atomic<int> close_after{-1};    // the connection closed after this many answers (each connection)
        std::atomic<int> silent{0};          // queries never answered
        std::atomic<int> connections{0};
        std::atomic<int> queries{0};
        std::atomic<int> padded{0};          // queries with the Padding option, their size a multiple of 128
        std::atomic<int> unpadded{0};
        std::atomic<int> in_flight_max{0};   // the most queries read and not yet answered on one connection
    };

    // Whether a query carries the Padding option and is a multiple of 128
    inline bool padded_query(const uint8_t* q, size_t n) {
        nd::DnsReader r(q, n);
        nd::DnsHeader h;
        if (!r.header(h)) {
            return false;
        }
        nd::DnsName name;
        uint16_t t = 0, k = 0;
        if (!r.question(name, t, k)) {
            return false;
        }
        nd::DnsReader::Record rec;
        for (size_t i = 0; i < size_t(h.answers) + h.authorities + h.additionals; ++i) {
            if (!r.record(rec)) {
                return false;
            }
            if (rec.type == type::opt && rec.length >= 4 && r.u16_at(rec.rdata) == nd::DnsOptionPadding) {
                return n % 128 == 0;
            }
        }
        return false;
    }

    struct DotConnectionState {
        net::connection c;
        async::mutex writing;
        std::atomic<int> answered{0};
        std::atomic<int> pending{0};
    };

    inline async::task<> dot_answer(tracked_ptr<DotConnectionState> st, Behaviour* b, DotBehaviour* d, Query q) {
        int now = ++st->pending;
        int seen = d->in_flight_max.load();
        while (now > seen && !d->in_flight_max.compare_exchange_weak(seen, now)) {
        }
        bool hold = false;
        {
            int v = d->hold.load();
            hold = v > 0;
        }
        if (hold && b->take(d->hold)) {
            co_await async::sleep(std::chrono::milliseconds(150));
        }
        if (b->take(d->silent)) {
            --st->pending;
            co_return;
        }
        vector<byte> out(8192);
        uint8_t* o = reinterpret_cast<uint8_t*>(out.data());
        size_t n = 0;
        if (b->take(d->runt)) {
            n = 3;
            o[2] = 1;
            o[3] = 2;
            o[4] = 3;
        } else {
            Query sent = q;
            if (b->take(d->wrong_id)) {
                sent.id = uint16_t(q.id ^ 0x5A5A);
            }
            n = answer(*b, sent, o + 2, out.size() - 2, true);
        }
        o[0] = uint8_t(n >> 8);
        o[1] = uint8_t(n);
        {
            auto guard = co_await st->writing.scoped_lock();
            (void)co_await st->c.async_write(static_cast<const vector<byte>&>(out).as_slice().first(n + 2));
        }
        --st->pending;
        int answered = ++st->answered;
        int limit = d->close_after.load();
        if (limit >= 0 && answered >= limit) {
            (void)st->c.close();
        }
    }

    inline async::task<> dot_serve_connection(net::connection c, Behaviour* b, DotBehaviour* d) {
        tracked_ptr<DotConnectionState> st = make_tracked<DotConnectionState>();
        st->c = c;
        vector<byte> in(65536);
        for (;;) {
            auto r = co_await c.async_read_full(in.as_slice(0, 2));
            if (!r || *r != 2) {
                break;
            }
            size_t len = size_t(in[0]) << 8 | size_t(in[1]);
            r = co_await c.async_read_full(in.as_slice(0, len));
            if (!r || *r != len) {
                break;
            }
            const uint8_t* qb = reinterpret_cast<const uint8_t*>(in.data());
            Query q;
            if (!read_query(qb, len, q)) {
                break;
            }
            ++d->queries;
            (padded_query(qb, len) ? d->padded : d->unpadded)++;
            note(*b, q);
            async::go(dot_answer(st, b, d, q));
        }
        (void)c.close();
        --b->live_connections;
    }

    // The connections accepted, closed with the server
    struct DotConns {
        std::mutex m;
        vector<net::connection> all;

        void close_all() {
            std::lock_guard lock(m);
            for (auto& c : all) {
                (void)c.close();
            }
        }
    };

    inline async::task<> dot_serve(net::listener l, Behaviour* b, DotBehaviour* d, tracked_ptr<DotConns> conns) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            ++b->live_connections;
            ++d->connections;
            {
                std::lock_guard lock(conns->m);
                conns->all.push_back(*c);
            }
            async::go(dot_serve_connection(*c, b, d));
        }
    }

    // The server on a port of 127.0.0.1 for as long as the object lives
    struct DotServer {
        Behaviour& b;
        DotBehaviour d;
        net::listener l;
        tracked_ptr<DotConns> conns = make_tracked<DotConns>();
        async::task<> serving;
        uint16_t port = 0;

        explicit DotServer(Behaviour& behaviour, net::tls::config c = dot_server_tls())
        : b(behaviour) {
            auto made = net::tls::listen("127.0.0.1:0", c);
            if (made) {
                l = *made;
                port = l.local_endpoint().port();
                serving = async::spawn(dot_serve(l, &b, &d, conns));
            }
        }

        // Every connection closed now, the listener left open
        void drop_connections() {
            conns->close_all();
        }

        bool ok() const {
            return (bool)l;
        }

        ~DotServer() {
            if (!ok()) {
                return;
            }
            (void)l.close();
            serving.wait();
            conns->close_all();
            for (int i = 0; i < 500 && b.live_connections.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        std::string address() const {
            return "tls://127.0.0.1:" + std::to_string(port);
        }

        // Strict, the test CA trusted
        net::dns::options options(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000), int attempts = 1) const {
            net::dns::options o;
            o.servers = {sgcl::string(address())};
            o.timeout = timeout;
            o.attempts = attempts;
            o.roots_pem = sgcl::string(ca_pem());
            return o;
        }
    };
}
