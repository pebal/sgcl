//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "headers.h"
#include "request.h"
#include "response.h"
#include "response_writer.h"
#include "status.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/event.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../async/when.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/weak_ptr.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

// A reverse proxy (Go's net/http/httputil.ReverseProxy) as a handler of
// http::server: the request passed on to a backend through an http::client,
// the response passed back as it comes. The hop-by-hop fields both ways
// (RFC 9110 §7.6.1), X-Forwarded-For/-Host/-Proto, Forwarded (RFC 7239) and
// Via (RFC 9110 §7.6.3) as options, the trailers, 1xx informational
// responses, upgrades (WebSocket) as a pipe of bytes, the backend's errors
// as 502 and its timeouts as 504; several backends balanced, their health
// watched, idempotent requests retried on another.
namespace sgcl::net::http {
    class reverse_proxy;

    namespace detail {
        // The fields of one connection (RFC 9110 §7.6.1), which a proxy does
        // not pass on: Go's list (Proxy-Authorization and -Authenticate are
        // the hop's credentials). Trailer is passed on in a response: the
        // trailers go after the body as they came
        SGCL_INLINE_HOT bool hop_by_hop(std::string_view n, bool response) noexcept {
            switch (n.size()) {
                case 2:
                    return iequal(n, "te");
                case 7:
                    return iequal(n, "upgrade") || (!response && iequal(n, "trailer"));
                case 10:
                    return iequal(n, "connection") || iequal(n, "keep-alive");
                case 16:
                    return iequal(n, "proxy-connection");
                case 17:
                    return iequal(n, "transfer-encoding");
                case 18:
                    return iequal(n, "proxy-authenticate");
                case 19:
                    return iequal(n, "proxy-authorization");
                default:
                    return false;
            }
        }

        // Whether a Connection field (any of them) names the field
        inline bool named_by_connection(const headers& h, std::string_view name) noexcept {
            return HeadersAccess::has_token(h, "connection", name);
        }

        // The fields that go on: every field but the hop-by-hop ones and
        // those the Connection fields name (RFC 9110 §7.6.1), and those the
        // caller drops (`skip`, true for a field it writes itself). An
        // upgrade keeps Upgrade, and its Connection becomes "Upgrade"
        template<class Skip>
        inline headers end_to_end(const headers& in, bool response, bool upgrade, Skip&& skip) noexcept {
            headers out;
            auto& to = HeadersAccess::fields(out);
            const auto& from = HeadersAccess::fields(in);
            to.reserve(from.size() + 4);
            const bool listed = HeadersAccess::count(in, "connection") != 0;
            for (auto& f : from) {
                auto n = f.first.view();
                if (upgrade && iequal(n, "upgrade")) {
                    to.push_back(f);
                    continue;
                }
                if (hop_by_hop(n, response) || skip(n)) {
                    continue;
                }
                if (listed && named_by_connection(in, n)) {
                    continue;
                }
                to.push_back(f);
            }
            if (upgrade) {
                out.add("Connection", "Upgrade");
            }
            return out;
        }

        // RFC 7239 §4: a forwarded-element is pairs of token "=" (token /
        // quoted-string) joined by ";", the elements by ",", OWS around
        // each separator. Whether the text is a list of them (an empty text
        // is not); the pairs' names and values are not interpreted
        inline bool forwarded_valid(std::string_view v) noexcept {
            size_t i = 0;
            const size_t n = v.size();
            auto ows = [&] {
                while (i < n && (v[i] == ' ' || v[i] == '\t')) {
                    ++i;
                }
            };
            auto token = [&] {
                size_t from = i;
                while (i < n && token_char(uint8_t(v[i]))) {
                    ++i;
                }
                return i > from;
            };
            ows();
            if (i == n) {
                return false;
            }
            for (;;) {
                // a pair
                if (!token() || i == n || v[i] != '=') {
                    return false;
                }
                ++i;
                if (i < n && v[i] == '"') {
                    ++i;
                    for (;;) {
                        if (i == n) {
                            return false;
                        }
                        unsigned char c = uint8_t(v[i]);
                        if (c == '"') {
                            ++i;
                            break;
                        }
                        if (c == '\\') {
                            if (i + 1 == n) {
                                return false;
                            }
                            unsigned char q = uint8_t(v[i + 1]);
                            if (!(q == '\t' || (q >= 0x20 && q != 0x7F))) {
                                return false;
                            }
                            i += 2;
                            continue;
                        }
                        if (!(c == '\t' || (c >= 0x20 && c != 0x7F))) {
                            return false;
                        }
                        ++i;
                    }
                } else if (!token()) {
                    return false;
                }
                ows();
                if (i == n) {
                    return true;
                }
                if (v[i] != ';' && v[i] != ',') {
                    return false;
                }
                ++i;
                ows();
            }
        }

        // A value of a pair of Forwarded: as a token when it is one, else
        // quoted (an IPv6 address in its brackets, a host with its port)
        inline void forwarded_value(std::string& out, std::string_view v) {
            if (is_token(v)) {
                out += v;
                return;
            }
            out += '"';
            for (char c : v) {
                if (c == '"' || c == '\\') {
                    out += '\\';
                }
                out += c;
            }
            out += '"';
        }

        // The element this hop adds: for=, host=, proto= (RFC 7239 §5.2–5.4)
        inline std::string forwarded_element(std::string_view client, std::string_view host, std::string_view proto) {
            std::string e;
            e.reserve(64);
            e += "for=";
            forwarded_value(e, client);
            if (!host.empty()) {
                e += ";host=";
                forwarded_value(e, host);
            }
            e += ";proto=";
            e += proto;
            return e;
        }

        // The Forwarded fields of a request with this hop's element after
        // them, one field; fields that are not RFC 7239's dropped (no
        // element of theirs can be trusted to be where it says)
        inline std::string forwarded_append(const headers& h, std::string_view element) {
            std::string out;
            bool valid = true;
            for (auto& f : HeadersAccess::fields(h)) {
                if (!iequal(f.first.view(), "forwarded")) {
                    continue;
                }
                auto v = trim_ows(f.second.view());
                if (!forwarded_valid(v)) {
                    valid = false;
                    break;
                }
                if (!out.empty()) {
                    out += ", ";
                }
                out += v;
            }
            if (!valid) {
                out.clear();
            }
            if (!out.empty()) {
                out += ", ";
            }
            out += element;
            return out;
        }

        // A list field's values joined (X-Forwarded-For, Via), the new one
        // last
        inline std::string list_append(const headers& h, std::string_view name, std::string_view value) {
            std::string out;
            for (auto& f : HeadersAccess::fields(h)) {
                if (iequal(f.first.view(), name)) {
                    auto v = trim_ows(f.second.view());
                    if (v.empty()) {
                        continue;
                    }
                    if (!out.empty()) {
                        out += ", ";
                    }
                    out += v;
                }
            }
            if (!out.empty()) {
                out += ", ";
            }
            out += value;
            return out;
        }

        // The protocol of a message as Via writes it (RFC 9110 §7.6.3):
        // "1.1", "1.0", "2"
        SGCL_INLINE_HOT std::string_view via_protocol(bool h2, int minor) noexcept {
            return h2 ? std::string_view("2") : minor == 0 ? std::string_view("1.0") : std::string_view("1.1");
        }

        // The URL of the outgoing request: the backend's scheme and host,
        // its path joined with the request's by one slash and the queries
        // joined by "&" (Go's single-host joining), the request's as it
        // came, escapes kept
        inline string join_target(const net::url& backend, std::string_view backend_origin, std::string_view backend_path, std::string_view backend_query,
                                  std::string_view path, std::string_view query) {
            std::string out;
            out.reserve(backend_origin.size() + backend_path.size() + path.size() + query.size() + backend_query.size() + 2);
            out += backend_origin;
            if (backend_path.empty() || backend_path == "/") {
                out += path.empty() ? std::string_view("/") : path;
            } else {
                const bool a = backend_path.back() == '/';
                const bool b = !path.empty() && path.front() == '/';
                out += backend_path;
                if (a && b) {
                    out += path.substr(1);
                } else if (!a && !b) {
                    if (!path.empty()) {
                        out += '/';
                        out += path;
                    }
                } else {
                    out += path;
                }
            }
            if (!backend_query.empty() && !query.empty()) {
                out += '?';
                out += backend_query;
                out += '&';
                out += query;
            } else if (!backend_query.empty() || !query.empty()) {
                out += '?';
                out += backend_query.empty() ? query : backend_query;
            }
            (void)backend;
            return string(std::string_view(out));
        }

        // The path and the query of a received request as they came: the
        // origin-form target split at '?', or those of an absolute-form one
        inline void request_path_query(RequestImpl& in, string& holder, std::string_view& path, std::string_view& query) {
            std::string_view t = in.target.empty() ? std::string_view() : in.target.view();
            if (!t.empty() && t.front() == '/') {
                auto q = t.find('?');
                path = t.substr(0, q);
                query = q == std::string_view::npos ? std::string_view() : t.substr(q + 1);
                return;
            }
            if (auto u = in.url_of()) {
                holder = u->request_target();   // "/path?query" of an absolute-form target, or a test's request
                auto v = holder.view();
                auto q = v.find('?');
                path = v.substr(0, q);
                query = q == std::string_view::npos ? std::string_view() : v.substr(q + 1);
                return;
            }
            path = "/";
            query = {};
        }

        SGCL_INLINE_HOT bool idempotent_method(std::string_view m) noexcept {
            return m == "GET" || m == "HEAD" || m == "OPTIONS" || m == "TRACE" || m == "PUT" || m == "DELETE";
        }

        // One backend of a proxy, its counts and its health
        struct ProxyBackend {
            string text;                         // the URL as given
            net::url url;
            string origin;                       // "scheme://host[:port]" as the URL writes it
            string path;                         // its path, "" for "/"
            string query;
            std::atomic<int64_t> active = {0};   // requests in progress (least connections)
            std::atomic<uint64_t> requests = {0};
            std::atomic<uint64_t> failures = {0};
            std::atomic<int> fails = {0};        // failures in a row (passive health)
            std::atomic<int64_t> down_until = {0};   // ns of the module's clock: skipped before it
            std::atomic<bool> healthy = {true};  // the active check's verdict

            SGCL_INLINE_HOT explicit ProxyBackend(const net::url& u) noexcept
            : url(u) {
            }

            SGCL_INLINE_HOT bool available(int64_t now) const noexcept {
                return healthy.load(std::memory_order_relaxed) && down_until.load(std::memory_order_relaxed) <= now;
            }
        };

        // The settings of a proxy and its backends, shared by its copies
        struct ProxyState {
            vector<tracked_ptr<ProxyBackend>> backends;
            tracked_ptr<ClientSettings> cfg;     // the client's settings, taken once
            tracked_ptr<Pool> pool;              // its pool, shared with the client given
            http::client client;
            function<void(request&, const request&)> rewrite;
            function<void(response_writer&, const response&)> modify_response;
            function<void(response_writer&, const request&, const io::error&)> error_handler;
            duration flush_interval;
            size_t buffer_size = 32 * 1024;
            bool preserve_host = false;
            bool x_forwarded = true;
            bool forwarded = false;
            string via;
            int policy = 0;                      // reverse_proxy::balancing
            int max_fails = 3;
            duration fail_timeout;
            string health_path;
            duration health_interval;
            duration health_timeout;
            int retries = 1;
            size_t retry_body_bytes = 0;
            std::atomic<uint64_t> next = {0};    // round robin's turn
            std::atomic<uint64_t> seed = {0x9E3779B97F4A7C15ull};
            std::atomic<bool> closed = {false};
        };

        SGCL_INLINE_HOT int64_t proxy_now() noexcept {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(sgcl::clock::now().time_since_epoch()).count();
        }

        // A random number for the random policy (splitmix64 over a shared
        // counter: not for secrets)
        SGCL_INLINE_HOT uint64_t proxy_random(ProxyState& p) noexcept {
            uint64_t z = p.seed.fetch_add(0x9E3779B97F4A7C15ull, std::memory_order_relaxed) + 0x9E3779B97F4A7C15ull;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }

        // The backends a request has tried: a word of bits, more for a
        // proxy of more than 64
        struct TriedSet {
            uint64_t bits = 0;
            std::vector<bool> more;

            SGCL_INLINE_HOT bool has(size_t i) const noexcept {
                return i < 64 ? (bits >> i) & 1 : (i - 64 < more.size() && more[i - 64]);
            }

            SGCL_INLINE_HOT void add(size_t i) {
                if (i < 64) {
                    bits |= uint64_t(1) << i;
                } else {
                    if (more.size() <= i - 64) {
                        more.resize(i - 63);
                    }
                    more[i - 64] = true;
                }
            }
        };

        // The backend of the next attempt by the policy among those
        // available (healthy, past their cooldown) and not tried; every one
        // not tried when none is available (the proxy fails open); -1 when
        // all were tried
        inline long choose_backend(ProxyState& p, const TriedSet& tried) {
            const size_t n = p.backends.size();
            if (n == 1) {
                return tried.has(0) ? -1 : 0;
            }
            const int64_t now = proxy_now();
            for (int pass = 0; pass < 2; ++pass) {
                auto eligible = [&](size_t i) {
                    return !tried.has(i) && (pass == 1 || p.backends[i]->available(now));
                };
                if (p.policy == 2) {   // random: among the eligible
                    size_t count = 0;
                    for (size_t i = 0; i < n; ++i) {
                        count += eligible(i) ? 1 : 0;
                    }
                    if (count) {
                        size_t k = size_t(proxy_random(p) % count);
                        for (size_t i = 0; i < n; ++i) {
                            if (eligible(i) && k-- == 0) {
                                return long(i);
                            }
                        }
                    }
                    continue;
                }
                const size_t start = size_t(p.next.fetch_add(1, std::memory_order_relaxed) % n);
                long best = -1;
                int64_t least = INT64_MAX;
                for (size_t j = 0; j < n; ++j) {
                    size_t i = (start + j) % n;
                    if (!eligible(i)) {
                        continue;
                    }
                    if (p.policy == 0) {   // round robin: the first eligible from the turn
                        return long(i);
                    }
                    int64_t a = p.backends[i]->active.load(std::memory_order_relaxed);
                    if (a < least) {   // least connections, ties broken by the turn
                        least = a;
                        best = long(i);
                    }
                }
                if (best >= 0) {
                    return best;
                }
            }
            return -1;
        }

        SGCL_INLINE_HOT void backend_failed(ProxyState& p, ProxyBackend& b) noexcept {
            b.failures.fetch_add(1, std::memory_order_relaxed);
            if (p.max_fails > 0 && b.fails.fetch_add(1, std::memory_order_relaxed) + 1 >= p.max_fails) {
                b.fails.store(0, std::memory_order_relaxed);
                b.down_until.store(proxy_now() + p.fail_timeout.nanoseconds(), std::memory_order_relaxed);
            }
        }

        SGCL_INLINE_HOT void backend_succeeded(ProxyBackend& b) noexcept {
            if (b.fails.load(std::memory_order_relaxed)) {
                b.fails.store(0, std::memory_order_relaxed);
            }
        }

        // A request counted among a backend's active ones while it runs
        struct ActiveCount {
            ProxyBackend* b = nullptr;

            SGCL_INLINE_HOT void hold(ProxyBackend* x) noexcept {
                release();
                b = x;
                b->active.fetch_add(1, std::memory_order_relaxed);
                b->requests.fetch_add(1, std::memory_order_relaxed);
            }

            SGCL_INLINE_HOT void release() noexcept {
                if (b) {
                    b->active.fetch_sub(1, std::memory_order_relaxed);
                    b = nullptr;
                }
            }

            SGCL_INLINE_HOT ~ActiveCount() {
                release();
            }
        };

        // Whether the connection a server's request came over is TLS
        SGCL_INLINE_HOT bool came_over_tls(const RequestImpl& in) noexcept {
            return in.conn && dynamic_cast<const net::tls::detail::TlsImpl*>(&net::detail::ConnectionAccess::impl(in.conn)) != nullptr;
        }

        // The Host a request came with: the field, HTTP/2's :authority, or
        // the URL's (a test's request)
        inline std::string_view incoming_host(RequestImpl& in, string& holder) {
            if (!in.host.empty()) {
                return in.host.view();
            }
            if (auto h = HeadersAccess::find(in.fields, "host")) {
                return *h;
            }
            if (auto u = in.url_of()) {
                holder = u->host();
                return holder.view();
            }
            return {};
        }

        // The client's address as X-Forwarded-For and Forwarded write it:
        // the IP without its port (an IPv6 address in brackets for
        // Forwarded)
        inline std::string client_ip(const RequestImpl& in, bool bracketed) {
            auto a = in.remote.address().unmap();
            std::string ip(a.to_string().view());
            if (bracketed && a.is_v6()) {
                return "[" + ip + "]";
            }
            return ip;
        }

        // The fields of the outgoing request (Go's ReverseProxy: the
        // incoming ones less the hop-by-hop, TE: trailers when the client
        // asked for trailers, X-Forwarded-*, Forwarded, Via, the Host by the
        // policy). The lists this hop appends to are what is left of the
        // incoming ones (a list a Connection field named is gone)
        inline headers outgoing_fields(const ProxyState& p, RequestImpl& in, bool upgrade) {
            headers out = end_to_end(in.fields, false, upgrade, [&](std::string_view n) {
                return iequal(n, "host") || iequal(n, "expect") || (p.x_forwarded && (iequal(n, "x-forwarded-host") || iequal(n, "x-forwarded-proto")));
            });
            string host_holder;
            const std::string_view host = incoming_host(in, host_holder);
            if (p.preserve_host && !host.empty()) {
                out.set("Host", string(host));
            }
            if (HeadersAccess::has_token(in.fields, "te", "trailers")) {
                out.set("TE", "trailers");
            }
            const bool tls = came_over_tls(in);
            if (p.x_forwarded) {
                out.set("X-Forwarded-For", string(std::string_view(list_append(out, "x-forwarded-for", client_ip(in, false)))));
                if (!host.empty()) {
                    out.set("X-Forwarded-Host", string(host));
                }
                out.set("X-Forwarded-Proto", tls ? "https" : "http");
            }
            if (p.forwarded) {
                auto e = forwarded_element(client_ip(in, true), host, tls ? "https" : "http");
                out.set("Forwarded", string(std::string_view(forwarded_append(out, e))));
            }
            if (!p.via.empty()) {
                std::string v(via_protocol(in.h2, in.minor));
                v += ' ';
                v += p.via.view();
                out.set("Via", string(std::string_view(list_append(out, "via", v))));
            }
            return out;
        }

        // The informational response of the backend passed on, 100
        // Continue aside (the server sends its own when the body is read)
        inline async::task<> forward_informational(response_writer w, int code, http::headers fields) noexcept {
            if (code == 100 || code == 101) {
                co_return;
            }
            headers f = end_to_end(fields, true, false, [](std::string_view) {
                return false;
            });
            (void)co_await w.async_send_informational(code, f);
        }

        // The backend's error answered: by the program's handler, or 504
        // for a timeout and 502 for the rest
        inline void answer_error(const ProxyState& p, response_writer& w, const request& in, const io::error& e) {
            if (p.error_handler) {
                p.error_handler(w, in, e);
                return;
            }
            if (!w.header_sent()) {
                w.headers() = http::headers();
                w.error(e.is_timeout() ? status::gateway_timeout : status::bad_gateway);
            }
        }

        SGCL_INLINE_HOT bool is_event_stream(const http::headers& h) noexcept {
            auto t = HeadersAccess::find(h, "content-type");
            if (!t) {
                return false;
            }
            auto v = trim_ows(t->substr(0, t->find(';')));
            return iequal(v, "text/event-stream");
        }

        // What the reader of a body hands to its writer (a flush interval):
        // a block read, or the end (n = 0), or the error
        struct BodyPiece {
            vector<byte> block;
            size_t n = 0;
            optional<io::error> error;
        };

        // The body read a block at a time into a channel, for a copy that
        // flushes on a timer between the blocks
        inline async::task<> read_pieces(io::reader body, size_t size, async::channel<tracked_ptr<BodyPiece>> out) noexcept {
            for (;;) {
                tracked_ptr piece = make_tracked<BodyPiece>();
                piece->block = vector<byte>(size);
                auto r = co_await body.async_read(piece->block.as_slice());
                if (!r) {
                    piece->error = r.error();
                } else {
                    piece->n = *r;
                }
                const bool last = !r || *r == 0;
                if (!co_await out.send(piece) || last) {
                    break;
                }
            }
            out.close();
        }

        // The trailers of the backend's body, read to its end, after the
        // client's
        inline void pass_trailers(const response& res, response_writer& w) {
            auto& body = ResponseAccess::impl(res)->body;
            if (!body) {
                return;
            }
            const auto& from = HeadersAccess::fields(body->trailers());
            if (from.empty()) {
                return;
            }
            auto& to = HeadersAccess::fields(w.trailers());
            for (auto& f : from) {
                to.push_back(f);
            }
        }

        // The backend's response passed back: its status and its fields
        // less the hop-by-hop ones (Via added), the program's modifier, the
        // body as it comes (flushed at once for a stream of events or a
        // body of no length, else when the buffer fills or the interval
        // passes), the trailers after it. A backend failing half-way breaks
        // the response off (a client reads it cut short); a client gone
        // ends the backend's
        inline async::task<> pass_response(tracked_ptr<ProxyState> p, request in, response_writer w, response res) {
            const int code = res.status();
            if (code < 200) {
                res.close();
                answer_error(*p, w, in, io::error(net::detail::net_error(net::errc::malformed_response, "proxy", "a switch of protocols the request did not ask for")));
                co_return;
            }
            w.set_status(code > 999 ? 502 : code);
            auto& resimpl = *ResponseAccess::impl(res);
            w.headers() = end_to_end(resimpl.fields, true, false, [](std::string_view) {
                return false;
            });
            if (!p->via.empty()) {
                std::string v(via_protocol(resimpl.h2, resimpl.minor));
                v += ' ';
                v += p->via.view();
                w.headers().set("Via", string(std::string_view(list_append(w.headers(), "via", v))));
            }
            if (p->modify_response) {
                p->modify_response(w, res);
                if (WriterAccess::impl(w)->body.size() || w.header_sent()) {
                    res.close();   // the program wrote a body of its own: the backend's dropped
                    w.headers().erase("Content-Length");
                    co_return;
                }
            }
            auto cl = res.content_length();
            const bool at_once = p->flush_interval < duration::zero() || !cl || is_event_stream(resimpl.fields);
            auto body = res.body();
            const size_t size = std::max<size_t>(p->buffer_size, 512);
            size_t pending = 0;
            auto fail_body = [&](const io::error& e) {
                if (!w.header_sent()) {
                    w.headers() = http::headers();
                    answer_error(*p, w, in, e);
                } else {
                    WriterAccess::abort(w);
                }
            };
            if (p->flush_interval > duration::zero() && !at_once) {
                // a reader of the backend's body beside the writer, which
                // flushes what is pending when the interval passes between
                // two blocks
                async::channel<tracked_ptr<BodyPiece>> pieces(1);
                async::go(read_pieces(body, size, pieces));
                for (;;) {
                    optional<tracked_ptr<BodyPiece>> got;
                    bool timed = false;
                    if (pending) {
                        co_await async::select(pieces.on_receive([&](optional<tracked_ptr<BodyPiece>> x) { got = std::move(x); }),
                                               async::after(p->flush_interval).on_set([&] { timed = true; }));
                    } else {
                        got = co_await pieces.receive();
                    }
                    if (timed) {
                        if (!co_await w.async_flush()) {
                            res.close();
                            pieces.close();
                            co_return;
                        }
                        pending = 0;
                        continue;
                    }
                    if (!got) {
                        break;
                    }
                    auto& piece = **got;
                    if (piece.error) {
                        fail_body(*piece.error);
                        co_return;
                    }
                    if (piece.n == 0) {
                        break;
                    }
                    w.write(piece.block.as_slice(0, piece.n));
                    pending += piece.n;
                    if (pending >= size) {
                        if (!co_await w.async_flush()) {
                            res.close();
                            pieces.close();
                            co_return;
                        }
                        pending = 0;
                    }
                }
            } else {
                vector<byte> block(cl ? size_t(std::min<uint64_t>(*cl, size)) + 1 : size);
                for (;;) {
                    auto r = co_await body.async_read(block.as_slice());
                    if (!r) {
                        fail_body(r.error());
                        co_return;
                    }
                    if (*r == 0) {
                        break;
                    }
                    w.write(block.as_slice(0, *r));
                    pending += *r;
                    if (at_once || pending >= size) {
                        if (!co_await w.async_flush()) {
                            res.close();   // the client gone: the backend's response given up
                            co_return;
                        }
                        pending = 0;
                    }
                }
            }
            pass_trailers(res, w);
        }

        // The response head of an upgrade, read off the backend's
        // connection: informational ones passed on, the final one parsed
        inline async::task<expected<tracked_ptr<ResponseImpl>, io::error>> upgrade_head(tracked_ptr<ProxyState> p, tracked_ptr<Wire> wire, response_writer w) noexcept {
            for (;;) {
                auto head = co_await wire->read_head(p->cfg->max_response_header_bytes);
                if (!head) {
                    co_return unexpected(head.error());
                }
                if (!*head) {
                    co_return unexpected(io::error(io::errc::unexpected_eof, "read", "upgrade"));
                }
                tracked_ptr impl = make_tracked<ResponseImpl>();
                impl->head = **head;
                StatusLine line;
                if (parse_response_head(impl->head, line, impl->fields)) {
                    co_return unexpected(net::detail::net_error(net::errc::malformed_response, "read", "upgrade"));
                }
                impl->status = line.status;
                impl->minor = line.minor;
                if (line.status >= 100 && line.status < 200 && line.status != 101) {
                    co_await forward_informational(w, line.status, impl->fields);
                    continue;
                }
                co_return impl;
            }
        }

        // Bytes written whole to a connection
        inline async::task<bool> write_text(net::connection c, std::string bytes) noexcept {
            if (bytes.empty()) {
                co_return true;
            }
            auto r = co_await c.async_write(string(std::string_view(bytes)));
            co_return (bool)r;
        }

        // A request that asks to switch protocols (WebSocket, any Upgrade
        // of HTTP/1.1): sent to the backend on a connection of its own; a
        // 101 back with the same protocol makes the two connections one
        // pipe of bytes until either ends, any other answer is passed back
        // as a response
        inline async::task<> proxy_upgrade(tracked_ptr<ProxyState> p, request in, response_writer w, string target) {
            auto& inimpl = *RequestAccess::impl(in);
            Outgoing out;
            out.method = inimpl.method;
            auto u = net::url::parse(target);
            if (!u) {
                answer_error(*p, w, in, net::detail::net_error(net::errc::invalid_url, "proxy", target));
                co_return;
            }
            out.target = *u;
            out.fields = outgoing_fields(*p, inimpl, true);
            if (p->rewrite) {
                request r(out.method, target);
                r.headers() = out.fields;
                p->rewrite(r, in);
                auto& ri = *RequestAccess::impl(r);
                if (!ri.url) {
                    answer_error(*p, w, in, net::detail::net_error(net::errc::invalid_url, "proxy", ri.url_text));
                    co_return;
                }
                out.target = *ri.url;
                out.fields = ri.fields;
            }
            if (auto e = unsendable(out)) {
                answer_error(*p, w, in, *e);
                co_return;
            }
            auto routed = route_for(*p->cfg, out);
            if (!routed) {
                answer_error(*p, w, in, routed.error());
                co_return;
            }
            Route route = *routed;
            if (route.kind == Route::Kind::forward) {
                route.kind = Route::Kind::tunnel;   // an upgrade goes through a tunnel, never forwarded
            }
            const time_point deadline = p->cfg->timeout > duration::zero() ? sgcl::clock::now() + p->cfg->timeout : time_point();
            auto dialed = co_await dial_origin(p->cfg, out, deadline, route);
            if (!dialed) {
                answer_error(*p, w, in, route.kind == Route::Kind::direct ? client_error(dialed.error(), out) : dialed.error());
                co_return;
            }
            net::connection back = *dialed;
            back.set_deadline(deadline);
            std::string head;
            head.reserve(512);
            head += out.method.view();
            head += ' ';
            head += out.target->request_target().view();
            head += " HTTP/1.1\r\n";
            if (!HeadersAccess::count(out.fields, "host")) {
                head += "Host: ";
                head += out.target->host().view();
                head += "\r\n";
            }
            for (auto& f : HeadersAccess::fields(out.fields)) {
                head += f.first.view();
                head += ": ";
                head += f.second.view();
                head += "\r\n";
            }
            head += "\r\n";
            if (!co_await write_text(back, std::move(head))) {
                (void)back.close();
                answer_error(*p, w, in, client_error(io::error(std::make_error_code(std::errc::broken_pipe), "write", ""), out));
                co_return;
            }
            // a body the request had (rare before a switch) goes as it came
            if (inimpl.body) {
                auto sent = co_await io::async_copy(back, in.body());
                if (!sent) {
                    (void)back.close();
                    answer_error(*p, w, in, client_error(sent.error(), out));
                    co_return;
                }
            }
            tracked_ptr wire = make_tracked<Wire>(back);
            auto got = co_await upgrade_head(p, wire, w);
            if (!got) {
                (void)back.close();
                answer_error(*p, w, in, client_error(got.error(), out));
                co_return;
            }
            tracked_ptr<ResponseImpl> resimpl = *got;
            const string asked = inimpl.fields.get("Upgrade");
            if (resimpl->status != 101) {
                // not switched: an answer as any other, its body off this
                // connection, which ends with it
                BodyFraming framing;
                if (!response_framing(resimpl->fields, resimpl->status, out.method.view() == "HEAD", framing)) {
                    (void)back.close();
                    answer_error(*p, w, in, client_error(net::errc::malformed_response, out));
                    co_return;
                }
                if (framing.kind == Framing::length) {
                    resimpl->content_length.emplace(framing.length);
                }
                resimpl->url = *out.target;
                resimpl->body = make_tracked<Body>(wire, framing, 0, true);
                resimpl->body->set_on_end([back](bool) {
                    (void)back.close();
                });
                back.set_deadline(time_point());
                co_await pass_response(p, in, w, ResponseAccess::make(resimpl));
                co_return;
            }
            const string agreed = resimpl->fields.get("Upgrade");
            if (!iequal(agreed.view(), asked.view())) {
                (void)back.close();
                answer_error(*p, w, in, net::detail::net_error(net::errc::malformed_response, "proxy", string::concat("the backend switched to \"", agreed, "\", not \"", asked, '"')));
                co_return;
            }
            std::string rest;
            auto front = WriterAccess::take_over(w, rest);
            if (!front) {
                (void)back.close();
                co_return;
            }
            back.set_deadline(time_point());
            // the 101 passed back as it came, less the hop's own fields
            headers fields = end_to_end(resimpl->fields, true, true, [](std::string_view) {
                return false;
            });
            if (!p->via.empty()) {
                fields.set("Via", string::concat(resimpl->minor == 0 ? "1.0 " : "1.1 ", p->via));
            }
            std::string answer = "HTTP/1.1 101 Switching Protocols\r\n";
            for (auto& f : HeadersAccess::fields(fields)) {
                answer += f.first.view();
                answer += ": ";
                answer += f.second.view();
                answer += "\r\n";
            }
            answer += "\r\n";
            answer += wire->view();   // what the backend sent past its head
            wire->consume(wire->buffered());
            net::connection client = *front;
            if (!co_await write_text(client, std::move(answer)) || !co_await write_text(back, std::move(rest))) {
                (void)client.close();
                (void)back.close();
                co_return;
            }
            (void)co_await async::when_any(client.async_copy_to(back), back.async_copy_to(client));
            (void)co_await client.async_close();
            (void)co_await back.async_close();
        }

        // One request passed on: the backend chosen, the outgoing request
        // made (Go's ReverseProxy), sent, retried on another backend when
        // the send failed and the request may go again, the response passed
        // back. What a hook of the program's throws goes to the server, as
        // a handler's throw does (500 when nothing was sent)
        inline async::task<> proxy_serve(tracked_ptr<ProxyState> p, request in, response_writer w) {
            auto& inimpl = *RequestAccess::impl(in);
            string path_holder;
            std::string_view path;
            std::string_view query;
            request_path_query(inimpl, path_holder, path, query);
            const bool upgrade = !inimpl.h2 && inimpl.minor == 1 && HeadersAccess::count(inimpl.fields, "upgrade") && named_by_connection(inimpl.fields, "upgrade");
            TriedSet tried;
            ActiveCount active;
            if (upgrade) {
                long i = choose_backend(*p, tried);
                if (i < 0) {
                    answer_error(*p, w, in, net::detail::net_error(net::errc::server_closed, "proxy", "no backend"));
                    co_return;
                }
                auto& b = *p->backends[size_t(i)];
                active.hold(&b);
                co_await proxy_upgrade(p, in, w, join_target(b.url, b.origin.view(), b.path.view(), b.query.view(), path, query));
                co_return;
            }
            const std::string_view method = inimpl.method.view();
            const bool has_body = (bool)inimpl.body;
            const bool may_retry = p->retries > 0 && p->backends.size() > 1 && idempotent_method(method);
            optional<vector<byte>> replay;
            if (has_body && may_retry && inimpl.content_length && *inimpl.content_length <= p->retry_body_bytes) {
                auto all = co_await in.async_bytes();
                if (!all) {
                    w.error(inimpl.body->error_status() == 413 ? status::content_too_large : status::bad_request);
                    co_return;
                }
                replay.emplace(std::move(*all));
            }
            const headers fields = outgoing_fields(*p, inimpl, false);
            auto hook = [w](int code, http::headers f) {
                return forward_informational(w, code, std::move(f));
            };
            for (int attempt = 0;; ++attempt) {
                long i = choose_backend(*p, tried);
                if (i < 0) {
                    answer_error(*p, w, in, net::detail::net_error(net::errc::server_closed, "proxy", "no backend"));
                    co_return;
                }
                tried.add(size_t(i));
                auto& b = *p->backends[size_t(i)];
                active.hold(&b);
                request out(inimpl.method, join_target(b.url, b.origin.view(), b.path.view(), b.query.view(), path, query));
                auto& outimpl = *RequestAccess::impl(out);
                outimpl.fields = fields;
                if (replay) {
                    out.set_body(*replay);
                } else if (has_body) {
                    out.set_body(in.body(), inimpl.content_length);
                }
                outimpl.no_redirects = true;
                outimpl.on_informational = hook;
                outimpl.stop = inimpl.stop;
                if (p->rewrite) {
                    p->rewrite(out, in);
                }
                auto res = co_await send_request(p->cfg, p->pool, RequestAccess::impl(out));
                if (!res) {
                    if (has_body && inimpl.body->failed()) {
                        // the client's body broke (gone, past the limit, its
                        // framing): no fault of the backend's
                        if (!w.header_sent()) {
                            w.error(inimpl.body->error_status() == 413 ? status::content_too_large : status::bad_request);
                        }
                        co_return;
                    }
                    backend_failed(*p, b);
                    const bool streamed = has_body && !replay;
                    if (may_retry && !streamed && attempt < p->retries && !inimpl.stop.stop_requested()) {
                        continue;
                    }
                    answer_error(*p, w, in, res.error());
                    co_return;
                }
                backend_succeeded(b);
                co_await pass_response(p, in, w, *res);
                co_return;
            }
        }

        // The active health check: every interval a GET of each backend's
        // health path, a 2xx or 3xx within the timeout its health; ends
        // with the proxy (its state gone or closed)
        inline async::task<> health_loop(weak_ptr<ProxyState> weak, duration interval) noexcept {
            for (;;) {
                co_await async::sleep(interval);
                auto p = weak.lock();
                if (!p || p->closed.load()) {
                    co_return;
                }
                for (auto& b : p->backends) {
                    if (p->closed.load()) {
                        co_return;
                    }
                    request r("GET", join_target(b->url, b->origin.view(), b->path.view(), b->query.view(), p->health_path.view(), {}));
                    RequestAccess::impl(r)->no_redirects = true;
                    async::stop_source stop;
                    stop.stop_after(p->health_timeout);
                    r.set_stop(stop.token());
                    auto res = co_await send_request(p->cfg, p->pool, RequestAccess::impl(r));
                    bool ok = res && res->status() >= 200 && res->status() < 400;
                    if (res) {
                        res->close();
                    }
                    b->healthy.store(ok, std::memory_order_relaxed);
                }
            }
        }
    }

    // A reverse proxy (Go's httputil.ReverseProxy): a handler of
    // http::server that passes each request on to a backend and its
    // response back. A handle of one word: copies share the backends and
    // their counts. The options are read when the proxy is made.
    class reverse_proxy {
    public:
        // How the backend of a request is chosen among several
        enum class balancing : uint8_t {
            round_robin,         // in turn
            least_connections,   // the one with the fewest requests in progress, in turn among equals
            random,              // at random
        };

        // A backend and its counts, as backends() gives them
        struct backend {
            string url;
            bool healthy = true;     // passing its health checks, and not skipped after failures
            int64_t active = 0;      // requests in progress
            uint64_t requests = 0;   // requests sent to it
            uint64_t failures = 0;   // sends that failed (no response)
        };

        struct options {
            // The outgoing request changed before it goes: its URL
            // (set_url), its fields, its body; the incoming one beside it.
            // It is made already: the backend's URL with the request's path
            // and query, the fields less the hop-by-hop ones, the
            // X-Forwarded ones, the body as a stream
            function<void(request& out, const request& in)> rewrite;
            // The backend's response before it goes back: its status and
            // fields are in the writer (the hop-by-hop ones gone), to be
            // changed; a body written here replaces the backend's
            function<void(response_writer& w, const response& backend)> modify_response;
            // What the client gets when the backend cannot be reached or
            // fails before its head; 502 Bad Gateway by default, 504
            // Gateway Timeout for a timeout
            function<void(response_writer& w, const request& in, const io::error& e)> error_handler;
            // How the backends are reached: TLS (roots, a client
            // certificate), HTTP/2 (h2 by ALPN, h2c), a proxy, the
            // timeouts (timeout, response_header_timeout: past one 504),
            // the pool. By default no proxy (not the environment's) and 256
            // idle connections a backend
            http::client client = default_client();
            // How often what is buffered is flushed while a body comes:
            // zero, when the buffer fills; less than zero, after every
            // read. A stream of events (text/event-stream) and a body of no
            // length are flushed after every read whatever this says
            duration flush_interval = duration::zero();
            // The block a body is copied in, the bytes buffered before a flush
            size_t buffer_size = 32 * 1024;
            // The incoming Host sent on; by default the backend's
            bool preserve_host = false;
            // X-Forwarded-For (the client's address appended to the list),
            // X-Forwarded-Host and X-Forwarded-Proto (those of the request
            // replaced)
            bool x_forwarded = true;
            // Forwarded (RFC 7239): for=, host=, proto= appended
            bool forwarded = false;
            // Via (RFC 9110 §7.6.3) appended both ways with this name, "1.1
            // name"; empty: none
            string via;
            // Several backends: the policy, and passive health (a backend
            // failing max_fails times in a row is skipped for fail_timeout;
            // zero: never)
            balancing policy = balancing::round_robin;
            int max_fails = 3;
            duration fail_timeout = std::chrono::seconds(10);
            // Active health: every health_interval a GET of this path of
            // each backend; a 2xx or 3xx within health_timeout keeps it in
            // the turn. Empty: none
            string health_path;
            duration health_interval = std::chrono::seconds(10);
            duration health_timeout = std::chrono::seconds(5);
            // An idempotent request whose send failed tried again on as many
            // other backends; a body goes again only when it was read whole
            // first: a Content-Length of at most retry_body_bytes
            int retries = 1;
            size_t retry_body_bytes = 64 * 1024;

            // The client the options start with
            static http::client default_client() {
                http::client c;
                c.proxy = http::proxy();
                c.max_idle_per_host = 256;
                return c;
            }
        };

        // A proxy to one backend ("http://10.0.0.5:8080", a path joined
        // before the request's: "http://app/api"); invalid_argument for a
        // URL that is not http:// or https://
        explicit reverse_proxy(const string& url)
        : reverse_proxy(vector<string>{url}, options()) {
        }

        reverse_proxy(const string& url, const options& o)
        : reverse_proxy(vector<string>{url}, o) {
        }

        // A proxy balancing several backends; invalid_argument for none
        explicit reverse_proxy(const vector<string>& backends)
        : reverse_proxy(backends, options()) {
        }

        reverse_proxy(const vector<string>& backends, const options& o)
        : _s(make_tracked<detail::ProxyState>()) {
            if (backends.empty()) {
                throw invalid_argument("http::reverse_proxy: no backend");
            }
            for (auto& text : backends) {
                auto u = net::url::parse(text);
                if (!u || (u->scheme() != "http" && u->scheme() != "https") || !u->has_host()) {
                    throw invalid_argument("http::reverse_proxy: a backend is an http:// or https:// URL");
                }
                tracked_ptr b = make_tracked<detail::ProxyBackend>(*u);
                b->text = text;
                b->origin = string::concat(u->scheme(), "://", u->host());
                auto path = u->path();
                b->path = path == "/" ? string() : path;
                b->query = u->query();
                _s->backends.push_back(b);
            }
            _s->client = o.client;
            _s->cfg = detail::ClientAccess::settings(o.client);
            _s->pool = detail::ClientAccess::pool(o.client);
            _s->rewrite = o.rewrite;
            _s->modify_response = o.modify_response;
            _s->error_handler = o.error_handler;
            _s->flush_interval = o.flush_interval;
            _s->buffer_size = o.buffer_size ? o.buffer_size : 1;
            _s->preserve_host = o.preserve_host;
            _s->x_forwarded = o.x_forwarded;
            _s->forwarded = o.forwarded;
            _s->via = o.via;
            _s->policy = int(o.policy);
            _s->max_fails = o.max_fails;
            _s->fail_timeout = o.fail_timeout;
            _s->health_path = o.health_path;
            _s->health_interval = o.health_interval > duration::zero() ? o.health_interval : std::chrono::seconds(10);
            _s->health_timeout = o.health_timeout > duration::zero() ? o.health_timeout : std::chrono::seconds(5);
            _s->retries = o.retries > 0 ? o.retries : 0;
            _s->retry_body_bytes = o.retry_body_bytes;
            if (!_s->health_path.empty()) {
                if (_s->health_path.view().front() != '/') {
                    _s->health_path = string::concat("/", _s->health_path);
                }
                async::go(detail::health_loop(weak_ptr<detail::ProxyState>(_s), _s->health_interval));
            }
        }

        // A copy shares the backends, their counts and the client's pool.
        // There is no move of its own: a moved-from proxy is the same proxy
        reverse_proxy(const reverse_proxy&) = default;
        reverse_proxy& operator=(const reverse_proxy&) = default;

        // The handler: `server.route("/", proxy)`. A task: it waits for
        // the backend
        SGCL_INLINE_HOT async::task<> operator()(request r, response_writer w) const noexcept {
            return detail::proxy_serve(_s, std::move(r), std::move(w));
        }

        // The backends with their counts, in the order given
        vector<backend> backends() const {
            vector<backend> out;
            const int64_t now = detail::proxy_now();
            for (auto& b : _s->backends) {
                backend x;
                x.url = b->text;
                x.healthy = b->available(now);
                x.active = b->active.load(std::memory_order_relaxed);
                x.requests = b->requests.load(std::memory_order_relaxed);
                x.failures = b->failures.load(std::memory_order_relaxed);
                out.push_back(std::move(x));
            }
            return out;
        }

        // The health checks stopped and the idle connections to the
        // backends closed; requests still go through it
        void close() const {
            _s->closed.store(true);
            _s->client.close_idle_connections();
        }

    private:
        tracked_ptr<detail::ProxyState> _s;
    };
}
