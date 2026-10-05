//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns_resolver.h"
#include "../tls.h"
#include "../../async/mutex.h"
#include "../../async/timer.h"
#include "../../core/map.h"
#include "../../core/root_ptr.h"
#include "../../core/detail/hash_bytes.h"
#include "../../crypto/sha256.h"

#include <mutex>

// DNS over TLS (RFC 7858, RFC 8310), the stub resolver's transport for a
// server "tls://...": installed into dns_hooks() as this header is
// included (tls.h includes it), so that dns.h itself needs no TLS.
//
// A server's connection is kept and shared by every lookup that asks it
// (RFC 7858 §3.4): the queries are written one after another, each framed
// by its length as over TCP (RFC 1035 §4.2.2), each with an id of its own
// among those in flight, and the answers are taken in whatever order they
// come (RFC 7766 §6.2.1.1). No task reads the connection for them: one of
// the queries waiting at a time holds the reader's role (a token in a
// channel), reads the frames that come and hands each to the query of its
// id, and gives the role back once its own answer is there, so that a
// lookup alone on the connection reads its answer itself, with no task in
// between. A connection idle for DotIdle is closed; one that fails, a
// frame cut short or a read that timed out inside a frame among it, fails
// the queries in flight, and the next query makes a new one, resuming the
// TLS session of the last (the pool's session cache). A query is padded to a multiple of 128 bytes (RFC 8467
// §4.1) and offers "dot" by ALPN (RFC 8310 §7.1).
//
// Strict (the default, RFC 8310 §6): the server's chain verified against
// the roots (the system's, or the options' PEM) for the server's name (the
// name given, else the host of the address: an address is checked as one),
// or, with pins, its keys checked against them instead (§4.2 of RFC 7858:
// the SHA-256 of a SubjectPublicKeyInfo of the chain matches one); a
// failure is the server's, and the next server is asked. Opportunistic
// (RFC 8310 §5): neither is required, and a server that cannot be reached
// over TLS at all is asked in clear text on port 53 when its host is an
// address.
namespace sgcl::net::detail {
    inline constexpr duration DotIdle = std::chrono::seconds(10);

    // The port of the opportunistic profile's clear text: 53, another in
    // the tests alone (a port below 1024 is the system's)
    inline std::atomic<uint16_t>& dot_clear_port() noexcept {
        static std::atomic<uint16_t> port{53};
        return port;
    }

    // A query waiting for its answer: the answer's bytes, and the channel
    // that says it came (true) or the connection failed (false)
    struct DotWait {
        SGCL_INLINE_HOT DotWait() noexcept
        : done(1) {
        }

        vector<byte> answer;
        async::detail::ChannelState<bool> done;
    };

    // One connection to a server and the queries in flight on it
    struct DotConn {
        SGCL_INLINE_HOT DotConn() noexcept
        : role(1) {
            role.try_send(true);
        }

        net::connection c;
        async::mutex writing;                       // one query's bytes at a time
        async::detail::ChannelState<bool> role;     // the reader's role: its token here while nobody reads
        std::mutex m;                               // guards the rest
        map<uint16_t, tracked_ptr<DotWait>> waiting;
        time_point used;                            // the last query written or answer read
        bool dead = false;

        // The query's id: random, none of those in flight; nullopt when the
        // connection has failed
        optional<uint16_t> enter(const tracked_ptr<DotWait>& w) noexcept {
            std::lock_guard lock(m);
            if (dead) {
                return nullopt;
            }
            for (;;) {
                uint16_t id = dns_random16();
                if (waiting.find(id) == waiting.end()) {
                    waiting.insert_or_assign(id, w);
                    used = sgcl::clock::now();
                    return id;
                }
            }
        }

        void leave(uint16_t id) noexcept {
            std::lock_guard lock(m);
            waiting.erase(id);
        }

        tracked_ptr<DotWait> take(uint16_t id) noexcept {
            std::lock_guard lock(m);
            auto it = waiting.find(id);
            if (it == waiting.end()) {
                return nullptr;
            }
            tracked_ptr<DotWait> w = it->second;
            waiting.erase(it);
            used = sgcl::clock::now();
            return w;
        }

        // Failed: every query in flight told so, the connection closed
        void fail() noexcept {
            vector<tracked_ptr<DotWait>> told;
            {
                std::lock_guard lock(m);
                if (dead) {
                    return;
                }
                dead = true;
                for (auto& [id, w] : waiting) {
                    told.push_back(w);
                }
                waiting.clear();
            }
            for (auto& w : told) {
                w->done.try_send(false);
            }
            (void)c.close();
        }

        bool alive() noexcept {
            std::lock_guard lock(m);
            return !dead;
        }

        // Idle past DotIdle, with nothing in flight: closed (true)
        bool close_if_idle() noexcept {
            {
                std::lock_guard lock(m);
                if (dead) {
                    return true;
                }
                if (!waiting.empty() || sgcl::clock::now() - used < DotIdle) {
                    return false;
                }
            }
            fail();
            return true;
        }
    };

    // A server's slot in the pool: its connection, and the lock that lets
    // one query at a time make a new one
    struct DotSlot {
        async::mutex connecting;
        std::mutex m;
        tracked_ptr<DotConn> conn;
    };

    struct DotPool {
        std::mutex m;
        map<string, tracked_ptr<DotSlot>> slots;
        tls::session_cache sessions;

        tracked_ptr<DotSlot> slot(const string& key) noexcept {
            std::lock_guard lock(m);
            auto it = slots.find(key);
            if (it != slots.end()) {
                return it->second;
            }
            tracked_ptr<DotSlot> s = make_tracked<DotSlot>();
            slots.insert_or_assign(key, s);
            return s;
        }
    };

    inline DotPool& dot_pool() noexcept {
        static root_ptr<DotPool>* pool = new root_ptr<DotPool>(make_tracked<DotPool>());   // never destroyed
        return **pool;
    }

    // What tells one connection from another: the server, the name and
    // pins it is checked for, the roots, the profile
    inline string dot_key(const DnsSettings& s, const DnsServerSpec& spec) noexcept {
        std::string k(spec.host.view());
        k += '|';
        k += std::to_string(spec.port);
        k += '|';
        k += spec.name.view();
        for (auto& p : spec.pins) {
            k += '|';
            k.append(reinterpret_cast<const char*>(p.data()), p.size());
        }
        k += s.opportunistic ? "|o|" : "|s|";
        if (!s.roots_pem.empty()) {
            k += std::to_string(hash_bytes(s.roots_pem.data(), s.roots_pem.size()));   // the roots by their hash: a key of a few bytes, not the PEM
        }
        return string(std::string_view(k));
    }

    // What one read of the reader's role came to: a frame handed to the
    // query of its id (or dropped, nobody's), the deadline passed before a
    // byte of a frame, or the connection's end: closed, failed, a frame
    // shorter than a message or cut short, a deadline inside a frame
    enum class DotRead : uint8_t {
        frame,
        timeout,
        failed
    };

    inline async::task<DotRead> dot_read_one(tracked_ptr<DotConn> conn, tracked_ptr<array<byte, 2>> head) noexcept {
        size_t got = 0;
        while (got < 2) {
            auto r = co_await conn->c.async_read(slice<byte>(head, head->data() + got, 2 - got));
            if (!r) {
                co_return got == 0 && r.error().is_timeout() ? DotRead::timeout : DotRead::failed;
            }
            if (*r == 0) {
                co_return DotRead::failed;   // the end of the stream
            }
            got += *r;
        }
        size_t len = size_t((*head)[0]) << 8 | size_t((*head)[1]);
        if (len < DnsHeaderSize) {
            co_return DotRead::failed;   // no message: the stream cannot be trusted past it
        }
        vector<byte> answer(len);
        auto r = co_await conn->c.async_read_full(answer.as_slice());
        if (!r || *r != len) {
            co_return DotRead::failed;
        }
        uint16_t id = uint16_t(uint16_t(answer[0]) << 8 | uint16_t(answer[1]));
        if (tracked_ptr<DotWait> w = conn->take(id)) {
            w->answer = std::move(answer);
            w->done.try_send(true);
        }   // an answer nobody waits for (its query gave up): dropped
        co_return DotRead::frame;
    }

    // The frames of a connection read until it ends, each handed to its
    // query, the connection failed after: the reader's role held for good
    // (the fuzz harness's way to read a stream whole)
    inline async::task<void> dot_read(tracked_ptr<DotConn> conn) noexcept {
        tracked_ptr<array<byte, 2>> head = make_tracked<array<byte, 2>>();
        while (co_await dot_read_one(conn, head) == DotRead::frame) {
        }
        conn->fail();
    }

    // The idle timer: the connection closed once nothing has used it for
    // DotIdle
    inline async::task<void> dot_idle(tracked_ptr<DotConn> conn) noexcept {
        for (;;) {
            co_await sgcl::async::sleep(DotIdle);
            if (conn->close_if_idle()) {
                co_return;
            }
        }
    }

    // The settings' TLS config for a server
    inline tls::config dot_config(const DnsSettings& s, const DnsServerSpec& spec, duration handshake) noexcept {
        tls::config c;
        c.server_name = spec.name.empty() ? spec.host : spec.name;
        c.alpn = {string("dot")};
        c.session_cache = dot_pool().sessions;
        c.insecure_skip_verify = s.opportunistic || !spec.pins.empty();
        if (!s.roots_pem.empty()) {
            c.roots = crypto::x509::certificate_pool::from_pem(s.roots_pem);
        }
        if (handshake > duration::zero()) {
            c.handshake_timeout = handshake;
        }
        return c;
    }

    // Whether a key of the chain is one of the pins
    inline bool dot_pinned(const net::connection& c, const DnsServerSpec& spec) noexcept {
        auto st = tls::state_of(c);
        if (!st) {
            return false;
        }
        for (auto& cert : st->peer_certificates) {
            auto digest = crypto::sha256::of(cert.raw_subject_public_key_info());
            for (auto& pin : spec.pins) {
                uint8_t diff = 0;
                for (size_t i = 0; i < 32; ++i) {
                    diff = uint8_t(diff | (uint8_t(digest[i]) ^ pin[i]));
                }
                if (diff == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    // A server's connection: the live one, or a new one made by one query
    // while the others wait for it. The error of a failed one, and whether
    // TLS itself could not be had (refused, unreachable, timed out: the
    // opportunistic fallback's case) rather than the server refused
    struct DotDial {
        tracked_ptr<DotConn> conn;
        optional<io::error> error;
        bool unreachable = false;
    };

    inline async::task<DotDial> dot_connection(tracked_ptr<DnsContext> ctx, size_t i, time_point deadline) noexcept {
        const DnsServerSpec& spec = ctx->settings.servers[i];
        tracked_ptr<DotSlot> slot = dot_pool().slot(dot_key(ctx->settings, spec));
        auto live = [&]() -> tracked_ptr<DotConn> {
            std::lock_guard lock(slot->m);
            if (slot->conn && slot->conn->alive()) {
                return slot->conn;
            }
            return nullptr;
        };
        DotDial out;
        if ((out.conn = live())) {
            co_return out;
        }
        auto guard = co_await slot->connecting.scoped_lock();
        if ((out.conn = live())) {
            co_return out;
        }
        duration left = deadline - sgcl::clock::now();
        if (left <= duration::zero()) {
            out.error = system_error(ETIMEDOUT, "lookup", spec.host);
            out.unreachable = true;
            co_return out;
        }
        std::string address(spec.host.view());
        if (address.find(':') != std::string::npos) {
            address = "[" + address + "]";
        }
        address += ':';
        address += std::to_string(spec.port);
        auto c = co_await tls::async_connect(string(std::string_view(address)), dot_config(ctx->settings, spec, left));
        if (!c) {
            out.error = c.error();
            out.unreachable = c.error().code().category() != tls::category();
            co_return out;
        }
        if (!spec.pins.empty() && !ctx->settings.opportunistic && !dot_pinned(*c, spec)) {
            (void)c->close();
            out.error = io::error(tls::make_error_code(tls::alert::bad_certificate), "lookup", string(std::string_view(address)));
            co_return out;
        }
        tracked_ptr<DotConn> conn = make_tracked<DotConn>();
        conn->c = *c;
        conn->used = sgcl::clock::now();
        {
            std::lock_guard lock(slot->m);
            slot->conn = conn;
        }
        sgcl::async::go(dot_idle(conn));
        out.conn = conn;
        co_return out;
    }

    // One query over the server's connection: written, its answer waited
    // for until the deadline or the lookup's stop. A connection found dead
    // as the query is written is made again once
    inline async::task<DnsAnswer> dot_exchange(tracked_ptr<DnsContext> ctx, size_t i, DnsName qname, uint16_t qtype, time_point deadline) noexcept {
        if (ctx->is_stopped()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        const DnsServerSpec& spec = ctx->settings.servers[i];
        for (int round = 0; round < 2; ++round) {
            DotDial d = co_await dot_connection(ctx, i, deadline);
            if (!d.conn) {
                if (d.unreachable && ctx->settings.opportunistic && spec.address.is_valid()) {
                    co_return co_await dns_exchange_udp(ctx, endpoint(spec.address.address(), dot_clear_port().load(std::memory_order_relaxed)), qname, qtype, deadline);   // RFC 8310 §5: clear text
                }
                co_return dns_socket_failure(*ctx, *d.error);
            }
            tracked_ptr<DotWait> w = make_tracked<DotWait>();
            auto id = d.conn->enter(w);
            if (!id) {
                continue;   // failed meanwhile: a new one
            }
            vector<byte> query(DnsContext::QuerySize);
            uint8_t* q = reinterpret_cast<uint8_t*>(query.data());
            size_t n = dns_write_query(q + 2, DnsContext::QuerySize - 2, *id, qname, qtype, true, DnsQueryPadBlock);
            q[0] = uint8_t(n >> 8);
            q[1] = uint8_t(n);
            bool written;
            {
                auto guard = co_await d.conn->writing.scoped_lock();
                written = (bool)co_await d.conn->c.async_write(static_cast<const vector<byte>&>(query).as_slice().first(n + 2));
            }
            if (!written) {
                d.conn->leave(*id);
                d.conn->fail();
                continue;
            }
            // its answer: handed over by the reader of the moment, or read by
            // itself once it holds the reader's role
            optional<bool> got;
            bool timed_out = false, stopped = false;
            tracked_ptr<array<byte, 2>> head;
            while (!got && !timed_out && !stopped) {
                bool lead = false;
                if (auto token = d.conn->role.try_receive()) {
                    lead = true;   // nobody reads: the role at once, no wait to arm
                } else if (ctx->stop.stop_possible()) {
                    co_await sgcl::async::select(w->done.on_receive([&](optional<bool> v) { got = v ? v : optional<bool>(false); }),
                                                 d.conn->role.on_receive([&](optional<bool>) { lead = true; }),
                                                 sgcl::async::timeout(deadline, [&] { timed_out = true; }),
                                                 ctx->stop.on_stop([&] { stopped = true; }));
                } else {
                    co_await sgcl::async::select(w->done.on_receive([&](optional<bool> v) { got = v ? v : optional<bool>(false); }),
                                                 d.conn->role.on_receive([&](optional<bool>) { lead = true; }),
                                                 sgcl::async::timeout(deadline, [&] { timed_out = true; }));
                }
                if (!lead) {
                    continue;
                }
                if (!head) {
                    head = make_tracked<array<byte, 2>>();
                }
                d.conn->c.set_read_deadline(deadline);
                for (;;) {
                    if (auto v = w->done.try_receive()) {
                        got = *v;   // handed over just before the role came, or read now
                        break;
                    }
                    DotRead r = co_await dot_read_one(d.conn, head);
                    if (r == DotRead::timeout) {
                        timed_out = true;
                        break;
                    }
                    if (r == DotRead::failed) {
                        d.conn->fail();   // the queries in flight told, this one among them
                    }
                }
                d.conn->role.try_send(true);   // the role to the next query waiting
            }
            if (timed_out || stopped) {
                d.conn->leave(*id);
                co_return dns_outcome(stopped ? DnsStatus::cancelled : DnsStatus::timeout);
            }
            if (!*got) {
                if (round == 0) {
                    continue;   // the connection failed under the query: once more on a new one
                }
                DnsAnswer a = dns_outcome(DnsStatus::failed);
                a.error = net_error(errc::server_misbehaving, "lookup", spec.host);
                co_return a;
            }
            DnsAnswer out;
            dns_read_answer(reinterpret_cast<const uint8_t*>(w->answer.data()), w->answer.size(), *id, qname, qtype, true, out);
            if (out.status == DnsStatus::foreign || out.status == DnsStatus::truncated) {
                out.status = DnsStatus::misbehaving;   // its id, another question: a fault, as over TCP
            }
            co_return out;
        }
        DnsAnswer a = dns_outcome(DnsStatus::failed);
        a.error = net_error(errc::server_misbehaving, "lookup", spec.host);
        co_return a;
    }

    // The transport installed as this header is included
    inline const bool dns_tls_installed = (dns_hooks().tls.store(&dot_exchange, std::memory_order_release), true);
}
