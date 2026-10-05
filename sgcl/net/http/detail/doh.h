//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../client.h"
#include "../../detail/dns_resolver.h"
#include "../../../core/map.h"
#include "../../../core/root_ptr.h"
#include "../../../encoding/base64.h"

#include <mutex>

// DNS over HTTPS (RFC 8484), the stub resolver's transport for a server
// "https://...": installed into dns_hooks() as this header is included
// (the http client includes it), so that dns.h needs no HTTP.
//
// The queries go through an http::client of the transport's own, one per
// set of roots and server name, kept for the process: its pool keeps the
// connections, HTTP/2 offered first by ALPN multiplexes the queries over
// one, HTTP/1.1 where the server has no HTTP/2. A query is the message
// with id 0 (§4.1: the same question is the same request, for the caches
// on the way), padded to a multiple of 128 bytes (RFC 8467 §4.1), sent
// as the body of a POST of application/dns-message, or with
// DnsSettings::https_get as GET with the message in base64url without
// padding in the variable `dns` (§4.1). An answer counts only as a 2xx of
// application/dns-message whose body, at most 65535 bytes, reads as the
// answer to the question; anything else is the server's failure (§4.2.1),
// and the next server is asked. The resolver keeps no cache, so the
// freshness of the response (Cache-Control, Age, §5.1) changes nothing.
namespace sgcl::net::detail {
    inline constexpr size_t DohMaxAnswer = 65535;

    struct DohPool {
        std::mutex m;
        map<string, http::client> clients;

        http::client client(const DnsSettings& s, const DnsServerSpec& spec) noexcept {
            string key = s.roots_pem + "|" + spec.name;
            std::lock_guard lock(m);
            auto it = clients.find(key);
            if (it != clients.end()) {
                return it->second;
            }
            http::client c;
            c.timeout = std::chrono::seconds(60);   // the bound of an exchange a query gave up on
            c.connect_timeout = std::chrono::seconds(30);
            if (!s.roots_pem.empty()) {
                c.tls.roots = crypto::x509::certificate_pool::from_pem(s.roots_pem);
            }
            c.tls.server_name = spec.name;
            clients.insert_or_assign(key, c);
            return c;
        }
    };

    inline DohPool& doh_pool() noexcept {
        static root_ptr<DohPool>* pool = new root_ptr<DohPool>(make_tracked<DohPool>());   // never destroyed
        return **pool;
    }

    // What one exchange came to: the answer's bytes, or the error
    struct DohWait {
        SGCL_INLINE_HOT DohWait() noexcept
        : done(1) {
        }

        vector<byte> answer;
        size_t size = 0;
        optional<io::error> error;
        bool misbehaving = false;   // an answer that is no answer: not 2xx, another type, too long
        async::detail::ChannelState<bool> done;
    };

    // The content type without its parameters, compared without case
    SGCL_INLINE_HOT bool doh_is_message(const string& type) noexcept {
        std::string_view v = type.view();
        if (auto semi = v.find(';'); semi != std::string_view::npos) {
            v = v.substr(0, semi);
        }
        while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) {
            v.remove_suffix(1);
        }
        constexpr std::string_view want = "application/dns-message";
        if (v.size() != want.size()) {
            return false;
        }
        for (size_t i = 0; i < want.size(); ++i) {
            char c = v[i];
            if (c >= 'A' && c <= 'Z') {
                c = char(c + 32);
            }
            if (c != want[i]) {
                return false;
            }
        }
        return true;
    }

    // The request sent, its answer read into the wait (at most
    // DohMaxAnswer bytes), the wait told
    inline async::task<void> doh_send(http::client c, http::request req, tracked_ptr<DohWait> w) noexcept {
        auto r = co_await c.async_send(req);
        if (!r) {
            w->error = r.error();
            w->done.try_send(true);
            co_return;
        }
        if (r->status() < 200 || r->status() > 299 || !doh_is_message(r->header("Content-Type"))) {
            w->misbehaving = true;
            (void)co_await r->body().async_read_all();   // the connection kept for the next
            w->done.try_send(true);
            co_return;
        }
        w->answer = vector<byte>(DohMaxAnswer + 1);
        io::reader body = r->body();
        size_t got = 0;
        for (;;) {
            auto n = co_await body.async_read(w->answer.as_slice(got, w->answer.size() - got));
            if (!n) {
                w->error = io::error(n.error().code(), "lookup", req.url().to_string());
                break;
            }
            if (*n == 0) {
                break;
            }
            got += *n;
            if (got > DohMaxAnswer) {
                w->misbehaving = true;
                break;
            }
        }
        w->size = got;
        w->done.try_send(true);
    }

    // What an exchange that came back says: no answer for a response that
    // is no DNS message, else the message read as the answer to the
    // question, its id 0; one to another question is the server's fault
    inline DnsAnswer doh_answer(const DohWait& w, const DnsName& qname, uint16_t qtype) noexcept {
        if (w.misbehaving || w.size > DohMaxAnswer || w.size > w.answer.size()) {
            return dns_outcome(DnsStatus::misbehaving);
        }
        DnsAnswer out;
        dns_read_answer(reinterpret_cast<const uint8_t*>(w.answer.data()), w.size, 0, qname, qtype, true, out);
        if (out.status == DnsStatus::foreign || out.status == DnsStatus::truncated) {
            out.status = DnsStatus::misbehaving;
        }
        return out;
    }

    // One query to the settings' server of index i: the request spawned,
    // waited for until the deadline or the lookup's stop (the exchange
    // itself goes on to its end and is dropped, the connection kept)
    inline async::task<DnsAnswer> doh_exchange(tracked_ptr<DnsContext> ctx, size_t i, DnsName qname, uint16_t qtype, time_point deadline) noexcept {
        if (ctx->is_stopped()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        const DnsServerSpec& spec = ctx->settings.servers[i];
        vector<byte> query(DnsContext::QuerySize);
        size_t n = dns_write_query(reinterpret_cast<uint8_t*>(query.data()), query.size(), 0, qname, qtype, true, DnsQueryPadBlock);
        query.resize(n);
        http::client c = doh_pool().client(ctx->settings, spec);
        auto req = [&]() -> http::request {
            if (ctx->settings.https_get) {
                std::string url(spec.host.view());
                url += url.find('?') == std::string::npos ? "?dns=" : "&dns=";
                url += encoding::base64::raw_url.encode(static_cast<const vector<byte>&>(query).as_slice()).view();
                http::request r("GET", string(std::string_view(url)));
                r.set_header("Accept", "application/dns-message");
                return r;
            }
            http::request r("POST", spec.host);
            r.set_header("Content-Type", "application/dns-message");
            r.set_header("Accept", "application/dns-message");
            r.set_body(std::move(query));
            return r;
        }();
        tracked_ptr<DohWait> w = make_tracked<DohWait>();
        sgcl::async::go(doh_send(c, req, w));
        optional<bool> got;
        bool timed_out = false, stopped = false;
        if (ctx->stop.stop_possible()) {
            co_await sgcl::async::select(w->done.on_receive([&](optional<bool> v) { got = v; }),
                                         sgcl::async::timeout(deadline, [&] { timed_out = true; }),
                                         ctx->stop.on_stop([&] { stopped = true; }));
        } else {
            co_await sgcl::async::select(w->done.on_receive([&](optional<bool> v) { got = v; }),
                                         sgcl::async::timeout(deadline, [&] { timed_out = true; }));
        }
        if (stopped) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        if (timed_out || !got) {
            co_return dns_outcome(DnsStatus::timeout);
        }
        if (w->error) {
            co_return dns_socket_failure(*ctx, *w->error);
        }
        co_return doh_answer(*w, qname, qtype);
    }

    // The transport installed as this header is included
    inline const bool dns_https_installed = (dns_hooks().https.store(&doh_exchange, std::memory_order_release), true);
}
