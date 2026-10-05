//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie_jar.h"
#include "proxy.h"
#include "request.h"
#include "response.h"
#include "status.h"
#include "websocket.h"
#include "detail/wire.h"
#include "detail/h2/transport.h"
#include "../connection.h"
#include "../dns.h"
#include "../socks5.h"
#include "../tls.h"
#include "../error.h"
#include "../socket.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/event.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timeout.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

namespace sgcl::net::http {
    using dial_function = function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>;

    namespace detail {
        struct IdleConnection {
            net::connection c;
            tracked_ptr<Wire> wire;
            time_point since;
            uint64_t id = 0;
        };

        struct Pool;

        // What an idle connection's timer holds: the pool, and which
        // connection it is to close if it is still idle when it fires
        struct PoolExpiry {
            tracked_ptr<Pool> pool;
            string key;
            uint64_t id = 0;
        };

        // The idle connections, by origin ("http://example.com:80"), each
        // list last in first out. A connection put in the pool gets a timer
        // of idle_timeout (the module's timer thread) that closes it if it
        // is still there, and take() skips one past its time as well
        struct Pool {
            std::mutex lock;
            map<string, vector<IdleConnection>> idle;
            uint64_t next_id = 0;
            // HTTP/2: the connections of an origin (one, a second only when
            // the first has as many streams as its server allows), and the
            // dial under way to an origin that others wait for
            map<string, vector<tracked_ptr<h2::ClientH2>>> h2;
            map<string, async::event> h2_dialing;

            // A connection of the origin with room for one more stream, the
            // room reserved (the pool's lock, then the connection's)
            tracked_ptr<h2::ClientH2> take_h2(const string& key) noexcept {
                std::lock_guard<std::mutex> g(lock);
                auto it = h2.find(key);
                if (it == h2.end()) {
                    return tracked_ptr<h2::ClientH2>();
                }
                for (auto& h : it->second) {
                    if (h->reserve()) {
                        return h;
                    }
                }
                return tracked_ptr<h2::ClientH2>();
            }

            // What the origin's server allows a connection, as one of its
            // connections learned it (0: none knows yet)
            uint32_t h2_limit(const string& key) noexcept {
                vector<tracked_ptr<h2::ClientH2>> list;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto it = h2.find(key);
                    if (it == h2.end()) {
                        return 0;
                    }
                    list = it->second;
                }
                uint32_t most = 0;
                for (auto& h : list) {
                    if (uint32_t n = h->learned_limit()) {
                        most = most ? std::min(most, n) : n;
                    }
                }
                return most;
            }

            SGCL_INLINE_HOT void add_h2(const string& key, const tracked_ptr<h2::ClientH2>& h) noexcept {
                std::lock_guard<std::mutex> g(lock);
                h2[key].push_back(h);
            }

            void remove_h2(const string& key, const tracked_ptr<h2::ClientH2>& h) noexcept {
                std::lock_guard<std::mutex> g(lock);
                auto it = h2.find(key);
                if (it == h2.end()) {
                    return;
                }
                auto& list = it->second;
                for (size_t i = 0; i < list.size(); ++i) {
                    if (list[i] == h) {
                        list.erase(list.begin() + i);
                        break;
                    }
                }
                if (list.empty()) {
                    h2.erase(it);
                }
            }

            // The dial to an origin: this request's (an event of its own
            // set, the result in the pool, when it ends) or another's to wait for
            SGCL_INLINE_HOT optional<async::event> begin_dial(const string& key, const async::event& mine) noexcept {
                std::lock_guard<std::mutex> g(lock);
                auto it = h2_dialing.find(key);
                if (it != h2_dialing.end()) {
                    return it->second;
                }
                h2_dialing.insert_or_assign(key, mine);
                return nullopt;
            }

            SGCL_INLINE_HOT void end_dial(const string& key, const async::event& mine) {
                {
                    std::lock_guard<std::mutex> g(lock);
                    h2_dialing.erase(key);
                }
                mine.set();
            }

            // the timer's: the connection closed if it is still idle
            void expire(const string& key, uint64_t id) {
                optional<net::connection> gone;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto it = idle.find(key);
                    if (it == idle.end()) {
                        return;
                    }
                    auto& list = it->second;
                    for (auto x = list.begin(); x != list.end(); ++x) {
                        if (x->id == id) {
                            gone = x->c;
                            list.erase(x);
                            break;
                        }
                    }
                    if (list.empty()) {
                        idle.erase(it);
                    }
                }
                if (gone) {
                    (void)gone->close();
                }
            }

            optional<IdleConnection> take(const string& key, duration idle_timeout) {
                vector<net::connection> stale;
                optional<IdleConnection> found;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto it = idle.find(key);
                    if (it == idle.end()) {
                        return nullopt;
                    }
                    auto& list = it->second;
                    auto now = sgcl::clock::now();
                    while (!list.empty()) {
                        auto x = list.back();
                        list.pop_back();
                        if (idle_timeout > duration::zero() && now - x.since > std::chrono::nanoseconds(idle_timeout)) {
                            stale.push_back(x.c);
                            continue;
                        }
                        found = x;
                        break;
                    }
                    if (list.empty()) {
                        idle.erase(it);
                    }
                }
                for (auto& c : stale) {
                    (void)c.close();
                }
                return found;
            }

            void put(const tracked_ptr<Pool>& self, const string& key, IdleConnection x, size_t most, duration idle_timeout) {
                optional<net::connection> dropped;
                uint64_t id = 0;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto& list = idle[key];
                    if (most == 0) {
                        dropped = x.c;
                    } else {
                        if (list.size() >= most) {
                            dropped = list.front().c;
                            list.erase(list.begin());
                        }
                        id = x.id = ++next_id;
                        list.push_back(std::move(x));
                    }
                }
                if (dropped) {
                    (void)dropped->close();
                }
                if (id && idle_timeout > duration::zero()) {
                    tracked_ptr<PoolExpiry> e = make_tracked<PoolExpiry>();
                    e->pool = self;
                    e->key = key;
                    e->id = id;
                    async::detail::add_timer(idle_timeout, tracked_ptr<void>(e), [](void* p) {
                        auto e = static_cast<PoolExpiry*>(p);
                        e->pool->expire(e->key, e->id);
                    });
                }
            }

            void close_all() {
                vector<net::connection> all;
                vector<tracked_ptr<h2::ClientH2>> multiplexed;
                {
                    std::lock_guard<std::mutex> g(lock);
                    for (auto& kv : idle) {
                        for (auto& x : kv.second) {
                            all.push_back(x.c);
                        }
                    }
                    idle.clear();
                    for (auto& kv : h2) {
                        for (auto& h : kv.second) {
                            multiplexed.push_back(h);
                        }
                    }
                }
                for (auto& c : all) {
                    (void)c.close();
                }
                for (auto& h : multiplexed) {
                    h->close_if_idle();   // GOAWAY and closed when no stream is open
                }
            }
        };

        struct ClientSettings {
            duration timeout;
            duration connect_timeout;
            duration response_header_timeout;
            duration idle_timeout;
            size_t max_idle_per_host = 16;
            int max_redirects = 10;
            size_t max_response_header_bytes = 1 << 20;
            dial_function dial;
            net::tls::config tls;
            bool http2 = true;
            bool h2c = false;
            http::proxy proxy;
            optional<cookie_jar> jar;
        };

        // What one attempt sends
        struct Outgoing {
            string method;
            optional<net::url> target;         // always set: optional only for want of a default url
            http::headers fields;
            RequestImpl::BodyKind body_kind = RequestImpl::BodyKind::none;
            string text;
            vector<byte> bytes;
            io::reader stream;
            optional<uint64_t> stream_length;   // a form's: its length when the send began
            tracked_ptr<FormState> form;
            async::stop_token stop;                                         // request::set_stop
            function<async::task<>(int, http::headers)> informational;      // the 1xx, awaited before the final head
            bool no_redirects = false;

            SGCL_INLINE_HOT bool replayable() const noexcept {
                return body_kind != RequestImpl::BodyKind::stream;
            }

            SGCL_INLINE_HOT bool idempotent() const noexcept {
                auto m = method.view();
                return m == "GET" || m == "HEAD" || m == "OPTIONS" || m == "TRACE" || m == "PUT" || m == "DELETE";
            }
        };

        // A request's stop watched while its exchange runs: a stop before
        // the end of its body cancels it (its connection closed, its HTTP/2
        // stream reset). Nothing watches an exchange that ends within
        // WatchAfter of its start, the most of them: a timer of the
        // module's (no task, no channel) is armed then cancelled. One that
        // lasts longer gets a task that waits for the stop, waking every
        // WatchPeriod to see whether the exchange has ended. A stop before
        // the timer is seen when it fires: a cancel at most WatchAfter late
        struct CancelWatch {
            static constexpr duration WatchAfter = std::chrono::milliseconds(2);
            static constexpr duration WatchPeriod = std::chrono::milliseconds(100);

            async::stop_token stop;
            net::connection conn;                // HTTP/1.1: closed by a stop
            tracked_ptr<h2::ClientH2> h2;        // HTTP/2: the stream reset by a stop
            uint32_t stream = 0;
            std::atomic<bool> done = {false};
            tracked_ptr<async::detail::Timer> timer;

            SGCL_INLINE_HOT void cancel() {
                if (h2) {
                    h2->reset(stream, h2::ErrorCode::cancel);
                } else if (conn) {
                    (void)conn.close();
                }
            }

            // The exchange over: the timer taken out of the heap
            SGCL_INLINE_HOT void finish() noexcept {
                if (done.exchange(true)) {
                    return;
                }
                if (auto& t = timer) {
                    t->cancelled.store(true, std::memory_order_release);
                    async::detail::timer_cancelled(*t);
                }
            }

            static async::task<> watch(tracked_ptr<CancelWatch> w) noexcept {
                while (!w->done.load()) {
                    bool stopped = false;
                    co_await async::select(w->stop.on_stop([&] { stopped = true; }), async::timeout(WatchPeriod, [] {}));
                    if (stopped) {
                        if (!w->done.load()) {
                            w->cancel();
                        }
                        co_return;
                    }
                }
            }

            // The timer's call, on its thread: a stop already requested
            // cancels at once, else the watch begins
            static void fire(void* p) {
                auto w = static_cast<CancelWatch*>(p);
                if (w->done.load()) {
                    return;
                }
                if (w->stop.stop_requested()) {
                    w->cancel();
                    return;
                }
                async::go(watch(tracked_ptr<CancelWatch>(w)));
            }

            // A watch of the stop for the exchange, or none when the
            // request has no stop to watch
            static tracked_ptr<CancelWatch> arm(const async::stop_token& stop, const net::connection& c, const tracked_ptr<h2::ClientH2>& h, uint32_t id) {
                if (!stop.stop_possible()) {
                    return tracked_ptr<CancelWatch>();
                }
                tracked_ptr w = make_tracked<CancelWatch>();
                w->stop = stop;
                w->conn = c;
                w->h2 = h;
                w->stream = id;
                w->timer = async::detail::add_weak_timer(sgcl::clock::now() + WatchAfter, weak_ptr<void>(tracked_ptr<void>(w)), &CancelWatch::fire);
                return w;
            }
        };

        SGCL_INLINE_HOT io::error canceled_error() noexcept {
            return io::error(error_code(ECANCELED, std::system_category()), "send", "stop");
        }

        // The pool's key of a connection: scheme://host:port, one string of
        // its pieces (string::concat), the port's digits written on the stack
        SGCL_INLINE_HOT string origin_key(const net::url& u) noexcept {
            char port[8];
            auto end = std::to_chars(port, port + sizeof port, u.effective_port()).ptr;
            return string::concat(u.scheme(), "://", net::detail::UrlAccess::host_as_written(u), ':', std::string_view(port, size_t(end - port)));
        }

        // How a request reaches its origin: directly, or through a proxy —
        // an HTTP proxy that forwards a request in absolute-form (http://
        // targets), a tunnel by CONNECT (https:// targets), a SOCKS5 proxy
        // (any target). The pool's key: the origin for a direct one; the
        // proxy, with its credentials, and the origin for a tunnel; the
        // proxy alone for forwarded requests, whose connections serve every
        // http:// origin
        struct Route {
            enum class Kind : uint8_t { direct, forward, tunnel, socks };
            Kind kind = Kind::direct;
            optional<ProxyTarget> via;
            string key;
        };

        // The text of a proxy's URL without its credentials, for an error
        inline string redacted_proxy(const string& text) noexcept {
            std::string_view v = text.view();
            size_t from = v.find("://");
            from = from == std::string_view::npos ? 0 : from + 3;
            size_t end = v.find_first_of("/?#", from);
            size_t at = v.rfind('@', end == std::string_view::npos ? v.size() : end);
            if (at == std::string_view::npos || at < from) {
                return text;
            }
            return string::concat(v.substr(0, from), "***", v.substr(at));
        }

        inline expected<Route, io::error> route_for(const ClientSettings& cfg, const Outgoing& o) noexcept {
            Route r;
            const net::url& t = *o.target;
            const bool https = t.scheme() == "https";
            const string& chosen = https ? cfg.proxy.https : cfg.proxy.http;
            if (chosen.empty() || no_proxy_matches(cfg.proxy.no_proxy.view(), net::detail::UrlAccess::host_as_written(t).view(), t.effective_port())) {
                r.key = origin_key(t);
                return r;
            }
            auto p = parse_proxy_url(chosen);
            if (!p) {
                return unexpected(io::error(p.error().code(), o.method, string::concat(t.to_string(), " (proxy ", redacted_proxy(chosen), ')')));
            }
            if (p->kind == ProxyKind::socks5 || p->kind == ProxyKind::socks5h) {
                r.kind = Route::Kind::socks;
                r.key = string::concat(p->key, '|', origin_key(t));
            } else if (https) {
                r.kind = Route::Kind::tunnel;
                r.key = string::concat(p->key, '|', origin_key(t));
            } else {
                r.kind = Route::Kind::forward;
                r.key = string::concat(p->key, '|');
            }
            r.via = std::move(*p);
            return r;
        }

        // A failure on the way through the proxy: the request named, and
        // the proxy (without its credentials) with what it answered
        inline io::error proxy_error(error_code code, const Outgoing& o, const Route& r, std::string_view answered = {}) noexcept {
            if (answered.empty()) {
                return io::error(code, o.method, string::concat(o.target->to_string(), " (through proxy ", r.via->shown, ')'));
            }
            return io::error(code, o.method, string::concat(o.target->to_string(), " (through proxy ", r.via->shown, ", which answered ", answered, ')'));
        }

        inline io::error client_error(net::errc e, const Outgoing& o) noexcept {
            return net::detail::net_error(e, o.method, o.target->to_string());
        }

        inline io::error client_error(const io::error& e, const Outgoing& o) noexcept {
            return io::error(e.code(), o.method, o.target->to_string());
        }

        // What of the request cannot be written as it is (RFC 9110): a
        // method that is not a token, a target or a host with a space, a
        // control or a byte past ASCII, a field name that is not a token,
        // a value with CR, LF, NUL or another control. Each would let the
        // program's input end a line and start another (request
        // splitting), so the send fails with std::errc::invalid_argument,
        // the part named, before a connection is dialed
        inline optional<io::error> unsendable(const Outgoing& o) {
            auto fail = [](const std::string& what) {
                return io::error(std::make_error_code(std::errc::invalid_argument), "send", string(what));
            };
            auto visible = [](std::string_view s) {
                for (unsigned char c : s) {
                    if (c <= 0x20 || c >= 0x7F) {
                        return false;
                    }
                }
                return true;
            };
            if (!is_token(o.method.view())) {
                return fail("invalid method: " + printable(o.method.view()));
            }
            auto target = o.target->request_target();
            if (target.empty() || !visible(target.view())) {
                return fail("invalid target: " + printable(target.view()));
            }
            if (!HeadersAccess::count(o.fields, "host") && !visible(o.target->host().view())) {
                return fail("invalid host: " + printable(o.target->host().view()));
            }
            if (auto e = invalid_field(o.fields)) {
                return fail(std::string(e->view()));
            }
            return nullopt;
        }

        inline async::task<expected<void, io::error>> write_all(net::connection c, std::string bytes) noexcept {
            slice<const byte> data(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
            auto r = co_await c.async_write(data);
            if (!r) {
                co_return io::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

        // A body in memory shorter than this goes in the head's buffer: a
        // piece of the write of its own is not worth it
        inline constexpr size_t BodyInPlaceMin = 1024;

        // The body when it is in memory, as it lies (the slice holds its
        // string or vector); empty for none and for a stream
        SGCL_INLINE_HOT slice<const byte> body_in_memory(const Outgoing& o) noexcept {
            if (o.body_kind == RequestImpl::BodyKind::text) {
                return as_bytes(o.text.as_slice());
            }
            if (o.body_kind == RequestImpl::BodyKind::bytes) {
                return o.bytes.as_slice();
            }
            return slice<const byte>();
        }

        // The head of a request, and a body in memory shorter than
        // BodyInPlaceMin; a longer one is written from where it lies
        // (write_head_and_body)
        // Through an HTTP proxy that forwards (`forward`), the target in
        // absolute-form (RFC 9112 §3.2.2: the scheme, the host and its port
        // as written, the path and the query; never the credentials) and
        // the proxy's credentials as Proxy-Authorization, unless the
        // request has that field of its own
        inline std::string request_bytes(const Outgoing& o, bool& chunked, const ProxyTarget* forward = nullptr) noexcept {
            std::string s;
            s.reserve(256);
            s += o.method.view();
            s += ' ';
            if (forward) {
                s += o.target->scheme().view();
                s += "://";
                s += o.target->host().view();
            }
            s += o.target->request_target().view();
            s += " HTTP/1.1\r\n";
            if (forward && forward->has_credentials() && !HeadersAccess::count(o.fields, "proxy-authorization")) {
                s += "Proxy-Authorization: ";
                s += forward->basic().view();
                s += "\r\n";
            }
            if (!HeadersAccess::count(o.fields, "host")) {
                s += "Host: ";
                s += o.target->host().view();   // with the port when one is written
                s += "\r\n";
            }
            for (auto& f : HeadersAccess::fields(o.fields)) {
                auto n = f.first.view();
                if (iequal(n, "content-length") || iequal(n, "transfer-encoding")) {
                    continue;
                }
                s += n;
                s += ": ";
                s += f.second.view();
                s += "\r\n";
            }
            chunked = false;
            auto m = o.method.view();
            bool expects_body = m == "POST" || m == "PUT" || m == "PATCH";
            switch (o.body_kind) {
                case RequestImpl::BodyKind::none:
                    if (expects_body) {
                        s += "Content-Length: 0\r\n";
                    }
                    break;
                case RequestImpl::BodyKind::text:
                    s += "Content-Length: " + std::to_string(o.text.size()) + "\r\n";
                    break;
                case RequestImpl::BodyKind::bytes:
                    s += "Content-Length: " + std::to_string(o.bytes.size()) + "\r\n";
                    break;
                case RequestImpl::BodyKind::stream:
                    if (o.stream_length) {
                        s += "Content-Length: " + std::to_string(*o.stream_length) + "\r\n";
                    } else {
                        s += "Transfer-Encoding: chunked\r\n";
                        chunked = true;
                    }
                    break;
                case RequestImpl::BodyKind::form:
                    s += "Content-Length: " + std::to_string(*o.stream_length) + "\r\n";
                    break;
            }
            s += "\r\n";
            if (auto body = body_in_memory(o); !body.empty() && body.size() < BodyInPlaceMin) {
                s.append(reinterpret_cast<const char*>(body.data()), body.size());
            }
            return s;
        }

        // Pieces as one write (net: ConnImpl::start_write_parts): a socket
        // takes them in one sendmsg, TLS seals its records from where they
        // lie. The pieces are held by the caller's frame across the wait
        inline async::task<expected<void, io::error>> write_parts(net::connection c, vector<slice<const byte>> parts) noexcept {
            auto s = net::detail::ConnectionAccess::impl(c).start_write_parts(parts);
            expected<size_t, io::error> r = std::move(s.done);
            if (s.rest) {
                r = co_await std::move(*s.rest);
            }
            if (!r) {
                co_return io::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

        // The head and a body in memory: one write, the body from where it
        // lies (its string or vector; no copy into the head's buffer). A
        // short body is already in the head (request_bytes)
        inline async::task<expected<void, io::error>> write_head_and_body(net::connection c, std::string head, slice<const byte> body) noexcept {
            if (body.size() < BodyInPlaceMin) {
                co_return co_await write_all(c, std::move(head));
            }
            vector<slice<const byte>> parts;
            parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(head.data()), head.size()));
            parts.push_back(body);
            co_return co_await write_parts(c, std::move(parts));
        }

        // A stream body: its length's worth, or chunked to its end
        inline async::task<expected<void, io::error>> write_stream(net::connection c, io::reader stream, optional<uint64_t> length, bool chunked) noexcept {
            tracked_ptr block = make_tracked<io::detail::CopyBlock>();   // managed: the stream's read may run on the pool, its slice holds the block
            uint64_t sent = 0;
            char size_line[24];
            vector<slice<const byte>> parts;
            for (;;) {
                size_t room = config::io_copy_buffer_size;
                if (length) {
                    if (sent == *length) {
                        break;
                    }
                    room = size_t(std::min<uint64_t>(room, *length - sent));
                }
                slice<byte> buf(block, block->data(), room);
                auto n = co_await stream.async_read(buf);
                if (!n) {
                    co_return io::detail::fail(n);
                }
                if (*n == 0) {
                    if (length) {
                        co_return io::detail::fail(io::error(io::errc::unexpected_eof, "write", "body"));
                    }
                    break;
                }
                if (chunked) {
                    // the size line, the bytes where they were read and the
                    // CRLF: one write (the line in this frame, the CRLF static)
                    int k = std::snprintf(size_line, sizeof(size_line), "%zx\r\n", *n);
                    parts.clear();
                    parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(size_line), size_t(k)));
                    parts.push_back(slice<const byte>(buf.first(*n)));
                    parts.push_back(slice<const byte>(reinterpret_cast<const byte*>("\r\n"), 2));
                    auto r = co_await write_parts(c, std::move(parts));
                    if (!r) {
                        co_return io::detail::fail(r);
                    }
                } else {
                    auto w = co_await c.async_write(buf.first(*n));
                    if (!w) {
                        co_return io::detail::fail(w);
                    }
                }
                sent += *n;
            }
            if (chunked) {
                co_return co_await write_all(c, "0\r\n\r\n");
            }
            co_return expected<void, io::error>();
        }

        struct Attempt {
            optional<response> result;
            optional<io::error> error;
            bool retry = false;             // failed before a byte of the response on a pooled connection
            bool retry_any = false;         // HTTP/2: never processed (REFUSED_STREAM, a stream never opened): any method again (RFC 9113 §8.7)
            bool retry_idempotent = false;  // HTTP/2: above the GOAWAY's last, or the connection lost before the head: an idempotent one again
        };

        // The TLS settings of a connection to u: the client's, the server
        // name the URL's host (an address verified as one), the handshake
        // within the connect's timeout when that is shorter
        inline net::tls::config tls_for(const net::tls::config& base, const net::url& u, duration timeout) noexcept {
            net::tls::config c = base;
            if (c.server_name.empty()) {
                std::string host(net::detail::UrlAccess::host_as_written(u).view());
                if (host.size() > 1 && host.front() == '[' && host.back() == ']') {
                    host = host.substr(1, host.size() - 2);   // an IPv6 address, verified by its bytes
                }
                c.server_name = string(std::string_view(host));
            }
            if (timeout > duration::zero() && timeout < c.handshake_timeout) {
                c.handshake_timeout = timeout;
            }
            return c;
        }

        inline async::task<expected<net::connection, io::error>> default_dial(const net::url& u, duration timeout, net::tls::config tls) noexcept {
            std::string address(net::detail::UrlAccess::host_as_written(u).view());
            address += ':';
            address += std::to_string(u.effective_port());
            if (u.scheme() == "https") {
                co_return co_await net::tls::async_connect(string(std::string_view(address)), tls_for(tls, u, timeout));
            }
            if (timeout > duration::zero()) {
                co_return co_await net::tcp::async_connect(string(std::string_view(address)), timeout);
            }
            co_return co_await net::tcp::async_connect(string(std::string_view(address)));
        }

        // A CONNECT's tunnel through the connection to the proxy: the
        // request written, the answer read to the end of its head and no
        // further (within max_head bytes); the code of a failure (the
        // connection then closed) and what the proxy answered, or none
        struct TunnelResult {
            error_code code;
            string answered;
        };

        inline async::task<TunnelResult> open_tunnel(net::connection c, string authority, string authorization, size_t max_head, time_point deadline) noexcept {
            TunnelResult out;
            c.set_deadline(deadline);
            auto w = co_await c.async_write(string(std::string_view(connect_request(authority, authorization))));
            if (!w) {
                (void)c.close();
                out.code = w.error().code();
                co_return out;
            }
            tracked_ptr block = make_tracked<io::detail::CopyBlock>();   // managed: the slice of a read holds it
            std::string got;
            for (;;) {
                size_t room = std::min<size_t>(config::io_copy_buffer_size, max_head + 1 - std::min(got.size(), max_head));
                auto n = co_await c.async_read(slice<byte>(block, block->data(), room));
                if (!n || *n == 0) {
                    (void)c.close();
                    out.code = n ? make_error_code(io::errc::unexpected_eof) : n.error().code();
                    co_return out;
                }
                got.append(reinterpret_cast<const char*>(block->data()), *n);
                auto a = read_connect_answer(got, max_head);
                if (a.result == ConnectAnswer::kind::more) {
                    continue;
                }
                if (a.result == ConnectAnswer::kind::established) {
                    c.set_deadline(time_point());
                    co_return out;
                }
                (void)c.close();
                if (a.status) {
                    out.answered = string::concat(std::to_string(a.status), a.reason.empty() ? "" : " ", a.reason);
                }
                out.code = a.result == ConnectAnswer::kind::auth_required ? make_error_code(net::errc::proxy_auth_required)
                           : a.result == ConnectAnswer::kind::refused   ? make_error_code(net::errc::proxy_refused)
                                                                         : make_error_code(net::errc::malformed_proxy_response);
                co_return out;
            }
        }

        // The origin's "host:port" as a CONNECT and SOCKS5 name it: the host
        // as the URL writes it, an IPv6 address in its brackets
        SGCL_INLINE_HOT string authority_of(const net::url& u) noexcept {
            return string::concat(net::detail::UrlAccess::host_as_written(u), ':', std::to_string(u.effective_port()));
        }

        // A connection to the origin through the route's proxy, within the
        // timeout: the proxy dialed (the program's dial given the proxy's
        // URL, as Go's DialContext is), TLS to an https:// proxy (ALPN
        // http/1.1 alone), then CONNECT's tunnel or SOCKS5's handshake (a
        // name resolved here for socks5://, by the proxy for socks5h://),
        // then TLS to an https:// origin over it. Forwarded requests stop
        // at the connection to the proxy. The errors name the request and
        // the proxy
        inline async::task<expected<net::connection, io::error>> dial_through_proxy(tracked_ptr<ClientSettings> cfg, const Outgoing& o, const Route& r, duration timeout) noexcept {
            const ProxyTarget& p = *r.via;
            const time_point deadline = timeout > duration::zero() ? sgcl::clock::now() + timeout : time_point();
            auto left = [&] {
                if (deadline == time_point()) {
                    return duration::zero();
                }
                auto d = duration(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - sgcl::clock::now()));
                return d > duration::zero() ? d : duration(std::chrono::nanoseconds(1));
            };
            expected<net::connection, io::error> c = net::connection();
            if (cfg->dial) {
                async::stop_source stop;
                if (timeout > duration::zero()) {
                    stop.stop_after(timeout);
                }
                auto at = net::url::parse(string::concat(p.kind == ProxyKind::https ? "https://" : "http://", p.host, ':', std::to_string(p.port), '/'));
                if (!at) {
                    co_return io::detail::fail(proxy_error(make_error_code(net::errc::invalid_url), o, r));
                }
                c = co_await cfg->dial(*at, stop.token());
            } else if (timeout > duration::zero()) {
                c = co_await net::tcp::async_connect(p.address(), timeout);
            } else {
                c = co_await net::tcp::async_connect(p.address());
            }
            if (!c) {
                co_return io::detail::fail(proxy_error(c.error().code(), o, r));
            }
            if (p.kind == ProxyKind::https) {
                net::tls::config tc = cfg->tls;
                std::string host(p.host.view());
                if (host.size() > 1 && host.front() == '[' && host.back() == ']') {
                    host = host.substr(1, host.size() - 2);
                }
                tc.server_name = string(std::string_view(host));
                tc.alpn = {string("http/1.1")};
                if (timeout > duration::zero() && left() < tc.handshake_timeout) {
                    tc.handshake_timeout = left();
                }
                c = co_await net::tls::async_client(*c, std::move(tc));
                if (!c) {
                    co_return io::detail::fail(proxy_error(c.error().code(), o, r));
                }
            }
            if (r.kind == Route::Kind::forward) {
                co_return c;
            }
            if (r.kind == Route::Kind::tunnel) {
                auto t = co_await open_tunnel(*c, authority_of(*o.target), p.has_credentials() ? p.basic() : string(), cfg->max_response_header_bytes, deadline);
                if (t.code) {
                    co_return io::detail::fail(proxy_error(t.code, o, r, t.answered.view()));
                }
            } else {
                string target = authority_of(*o.target);
                if (p.kind == ProxyKind::socks5 && !o.target->host_address()) {
                    // socks5://: the name resolved here, its first address sent
                    async::stop_source stop;
                    if (timeout > duration::zero()) {
                        stop.stop_after(left());
                    }
                    auto found = co_await net::dns::async_lookup(o.target->hostname(), stop.token());
                    if (!found || found->empty()) {
                        (void)c->close();
                        co_return io::detail::fail(client_error(found ? io::error(net::errc::no_suitable_address, "lookup", o.target->hostname()) : found.error(), o));
                    }
                    // one address goes in the request: the first IPv4 one,
                    // the family more networks reach, else the first
                    ip_address a = (*found)[0];
                    for (const auto& x : *found) {
                        if (x.unmap().is_v4()) {
                            a = x;
                            break;
                        }
                    }
                    target = a.unmap().is_v4() ? string::concat(a.unmap().to_string(), ':', std::to_string(o.target->effective_port()))
                                               : string::concat('[', a.to_string(), "]:", std::to_string(o.target->effective_port()));
                }
                net::socks5::options so;
                so.username = p.username;
                so.password = p.password;
                so.timeout = timeout > duration::zero() ? left() : duration::zero();
                c = co_await net::socks5::async_client(*c, target, std::move(so));
                if (!c) {
                    co_return io::detail::fail(proxy_error(c.error().code(), o, r));
                }
            }
            if (o.target->scheme() == "https") {
                auto secured = co_await net::tls::async_client(*c, tls_for(cfg->tls, *o.target, timeout > duration::zero() ? left() : duration::zero()));
                if (!secured) {
                    co_return io::detail::fail(client_error(secured.error(), o));
                }
                co_return secured;
            }
            co_return c;
        }

        // A new connection to the target's origin, TLS for https, within
        // the connect's timeout and the request's deadline; through the
        // route's proxy when it has one (its errors complete: the request
        // and the proxy named)
        inline async::task<expected<net::connection, io::error>> dial_origin(tracked_ptr<ClientSettings> cfg, const Outgoing& o, time_point deadline, const Route& route) noexcept {
            duration timeout = cfg->connect_timeout;
            if (deadline != time_point()) {
                auto left = duration(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - sgcl::clock::now()));
                if (left <= duration::zero()) {
                    co_return io::detail::fail(io::error(error_code(ETIMEDOUT, std::system_category()), "dial", ""));
                }
                if (timeout <= duration::zero() || left < timeout) {
                    timeout = left;
                }
            }
            if (route.kind != Route::Kind::direct) {
                co_return co_await dial_through_proxy(cfg, o, route, timeout);
            }
            if (cfg->dial) {
                async::stop_source stop;
                if (timeout > duration::zero()) {
                    stop.stop_after(timeout);
                }
                auto dialed = co_await cfg->dial(*o.target, stop.token());
                if (dialed && o.target->scheme() == "https") {
                    // the dial gives the transport, TLS goes over it (Go's DialContext)
                    dialed = co_await net::tls::async_client(*dialed, tls_for(cfg->tls, *o.target, timeout));
                }
                co_return dialed;
            }
            co_return co_await default_dial(*o.target, timeout, cfg->tls);
        }

        // A request's field block for HTTP/2 (RFC 9113 §8.3.1): the
        // pseudo-fields, then its fields in lower case without HTTP/1.1's
        // connection fields (§8.2.2), Host as :authority, the length when
        // it is known
        struct RequestBlock final : h2::FieldBlock {
            const Outgoing& o;

            SGCL_INLINE_HOT explicit RequestBlock(const Outgoing& o) noexcept
            : o(o) {
            }

            void encode(h2::Encoder& e, std::string& out) const noexcept override {
                e.encode(out, ":method", o.method.view());
                e.encode(out, ":scheme", o.target->scheme().view());
                auto host = o.fields.get("Host");
                e.encode(out, ":authority", host.empty() ? o.target->host().view() : host.view());
                e.encode(out, ":path", o.target->request_target().view());
                std::string name;
                for (auto& f : HeadersAccess::fields(o.fields)) {
                    auto n = f.first.view();
                    if (iequal(n, "host") || iequal(n, "connection") || iequal(n, "keep-alive") || iequal(n, "proxy-connection")
                        || iequal(n, "transfer-encoding") || iequal(n, "upgrade") || iequal(n, "content-length")) {
                        continue;
                    }
                    if (iequal(n, "te") && f.second.view() != "trailers") {
                        continue;
                    }
                    name.assign(n);
                    for (auto& c : name) {
                        if (c >= 'A' && c <= 'Z') {
                            c = char(c - 'A' + 'a');
                        }
                    }
                    e.encode(out, name, f.second.view());
                }
                auto m = o.method.view();
                switch (o.body_kind) {
                    case RequestImpl::BodyKind::none:
                        if (m == "POST" || m == "PUT" || m == "PATCH") {
                            e.encode(out, "content-length", "0");
                        }
                        break;
                    case RequestImpl::BodyKind::text:
                        e.encode(out, "content-length", std::to_string(o.text.size()));
                        break;
                    case RequestImpl::BodyKind::bytes:
                        e.encode(out, "content-length", std::to_string(o.bytes.size()));
                        break;
                    case RequestImpl::BodyKind::stream:
                    case RequestImpl::BodyKind::form:
                        if (o.stream_length) {
                            e.encode(out, "content-length", std::to_string(*o.stream_length));
                        }
                        break;
                }
            }
        };

        // A request's body as DATA within the windows; a stream's read a
        // block at a time
        // The request's own stream failing (its read, or its end before its
        // length) is kept in `own`: the stream's reset that follows is ours,
        // and the send reports the stream's error, as over HTTP/1.1
        inline async::task<expected<void, io::error>> send_body_h2(tracked_ptr<h2::ClientH2> h, uint32_t id, const Outgoing& o, optional<io::error>& own) noexcept {
            switch (o.body_kind) {
                case RequestImpl::BodyKind::none:
                    co_return expected<void, io::error>();
                case RequestImpl::BodyKind::text:
                case RequestImpl::BodyKind::bytes:
                    // in place: the slice holds the request's string or vector
                    // until the connection's write of it is done
                    co_return co_await h->send_data_held(id, body_in_memory(o), true);
                case RequestImpl::BodyKind::stream:
                case RequestImpl::BodyKind::form:
                    break;
            }
            io::reader stream = o.stream;
            if (o.body_kind == RequestImpl::BodyKind::form) {
                auto b = form_body(o.form);   // a fresh stream of the form, each attempt its own
                if (!b) {
                    own.emplace(b.error());
                    h->reset(id, h2::ErrorCode::cancel);
                    co_return io::detail::fail(b.error());
                }
                stream = b->first;
            }
            tracked_ptr block = make_tracked<io::detail::CopyBlock>();   // managed: the stream's read may run on the pool, its slice holds the block
            uint64_t sent = 0;
            for (;;) {
                size_t room = config::io_copy_buffer_size;
                if (o.stream_length) {
                    if (sent == *o.stream_length) {
                        break;
                    }
                    room = size_t(std::min<uint64_t>(room, *o.stream_length - sent));
                }
                slice<byte> buf(block, block->data(), room);
                auto n = co_await stream.async_read(buf);
                if (!n) {
                    own.emplace(n.error());
                    h->reset(id, h2::ErrorCode::cancel);
                    co_return io::detail::fail(n);
                }
                if (*n == 0) {
                    if (o.stream_length) {
                        own.emplace(io::error(io::errc::unexpected_eof, "write", "body"));
                        h->reset(id, h2::ErrorCode::cancel);
                        co_return io::detail::fail(*own);
                    }
                    break;
                }
                auto w = co_await h->send_data(id, buf.first(*n), false);
                if (!w) {
                    co_return io::detail::fail(w);
                }
                sent += *n;
            }
            co_return co_await h->send_data(id, slice<const byte>(), true);
        }

        // One exchange on an HTTP/2 connection, its place reserved: a
        // stream opened, the body sent, the head awaited
        inline async::task<Attempt> round_trip_h2(tracked_ptr<ClientSettings> cfg, const Outgoing& o, time_point deadline, tracked_ptr<h2::ClientH2> h) noexcept {
            using Fate = h2::ClientStream::Fate;
            Attempt a;
            tracked_ptr st = make_tracked<h2::ClientStream>(tracked_ptr<h2::StreamOwner>(h));
            st->head_request = o.method.view() == "HEAD";
            const bool has_body = o.body_kind == RequestImpl::BodyKind::stream || o.body_kind == RequestImpl::BodyKind::form
                                  || (o.body_kind == RequestImpl::BodyKind::text && !o.text.empty())
                                  || (o.body_kind == RequestImpl::BodyKind::bytes && !o.bytes.empty());
            st->wants_informational = (bool)o.informational;
            if (o.stop.stop_requested()) {
                a.error = client_error(canceled_error(), o);
                co_return a;
            }
            RequestBlock block(o);
            if (!h->open(st, block, !has_body)) {
                a.error = client_error(h2::stream_reset_error("write", h2::ErrorCode::refused_stream), o);
                a.retry_any = true;   // never sent
                co_return a;
            }
            h->arm(st, deadline, time_point());
            // a stop resets the stream until its body has ended
            tracked_ptr<CancelWatch> watch = CancelWatch::arm(o.stop, net::connection(), h, st->id);
            optional<io::error> own;
            if (has_body) {
                (void)co_await send_body_h2(h, st->id, o, own);   // a failure shows as the stream's fate; an early answer wins
            }
            if (cfg->response_header_timeout > duration::zero()) {
                h->arm(st, time_point(), sgcl::clock::now() + cfg->response_header_timeout);
            }
            Fate fate = Fate::open;
            for (;;) {
                if (o.informational) {
                    for (auto& x : h->take_informational(st)) {
                        co_await o.informational(x.first, x.second);
                    }
                }
                fate = h->fate_of(st);
                if (fate != Fate::open) {
                    break;
                }
                (void)co_await st->headed.receive();
            }
            if (fate == Fate::headed) {
                tracked_ptr<ResponseImpl> impl = st->head;
                impl->url = *o.target;
                impl->body->set_on_end([st, watch](bool) {
                    st->cancel_timers();
                    if (watch) {
                        watch->finish();
                    }
                });
                a.result = ResponseAccess::make(impl);
                co_return a;
            }
            st->cancel_timers();
            if (watch) {
                watch->finish();
            }
            if (o.stop.stop_requested()) {
                a.error = client_error(canceled_error(), o);
                co_return a;
            }
            if (own) {
                a.error = client_error(*own, o);   // our stream failed and we reset: its error, not the reset's
                co_return a;
            }
            switch (fate) {
                case Fate::refused:
                    a.retry_any = true;
                    a.error = client_error(h2::stream_reset_error("read", h2::ErrorCode::refused_stream), o);
                    break;
                case Fate::unprocessed:
                case Fate::lost:
                    a.retry_idempotent = true;
                    a.error = client_error(io::error(std::make_error_code(std::errc::connection_reset), "read", ""), o);
                    break;
                case Fate::timed_out:
                    a.error = client_error(io::error(error_code(ETIMEDOUT, std::system_category()), "read", ""), o);
                    break;
                case Fate::too_large:
                    a.error = client_error(net::errc::header_too_large, o);
                    break;
                case Fate::malformed:
                    a.error = client_error(net::errc::malformed_response, o);
                    break;
                default:
                    a.error = client_error(h2::stream_reset_error("read", h2::ErrorCode::cancel), o);
                    break;
            }
            co_return a;
        }

        inline async::task<> await_event(async::event e) noexcept {
            co_await e;
        }

        // The machine's settings of the client's HTTP/2 connections
        SGCL_INLINE_HOT h2::TransportSettings transport_settings(const ClientSettings& cfg, uint32_t known_limit) noexcept {
            h2::TransportSettings t;
            t.machine.max_header_list_size = uint32_t(std::min<size_t>(cfg.max_response_header_bytes, 0xFFFFFFFFu));
            if (known_limit) {
                t.machine.initial_concurrent_streams = known_limit;   // the origin's server already told another connection
            }
            t.idle_timeout = cfg.idle_timeout;
            return t;
        }

        inline async::task<Attempt> round_trip(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, const Outgoing& o, time_point deadline, bool may_reuse) noexcept {
            Attempt a;
            auto routed = route_for(*cfg, o);
            if (!routed) {
                a.error = routed.error();
                co_return a;
            }
            const Route& route = *routed;
            const string& key = route.key;
            const bool https = o.target->scheme() == "https";
            const bool forward = route.kind == Route::Kind::forward;
            // a dial's error: complete when it came through a proxy
            auto dial_error = [&](const io::error& e) {
                return route.kind == Route::Kind::direct ? client_error(e, o) : e;
            };
            optional<IdleConnection> idle = may_reuse ? pool->take(key, cfg->idle_timeout) : optional<IdleConnection>();
            optional<net::connection> fresh;
            if (!idle && !forward && (https ? cfg->http2 : cfg->h2c)) {
                // HTTP/2: a connection of the origin with room, or the one
                // being dialed waited for, or a dial of this request's
                for (;;) {
                    if (auto h = pool->take_h2(key)) {
                        co_return co_await round_trip_h2(cfg, o, deadline, h);
                    }
                    async::event mine;
                    if (auto other = pool->begin_dial(key, mine)) {
                        if (deadline == time_point()) {
                            co_await *other;
                        } else if (!co_await async::with_deadline(await_event(*other), deadline)) {
                            a.error = client_error(io::error(error_code(ETIMEDOUT, std::system_category()), "dial", ""), o);
                            co_return a;
                        }
                        if (may_reuse) {
                            idle = pool->take(key, cfg->idle_timeout);   // the server chose HTTP/1.1
                            if (idle) {
                                break;
                            }
                        }
                        continue;
                    }
                    const uint32_t known_limit = pool->h2_limit(key);
                    auto dialed = co_await dial_origin(cfg, o, deadline, route);
                    tracked_ptr<h2::ClientH2> h;
                    if (dialed) {
                        const bool h2 = !https || [&] {
                            auto st = net::tls::state_of(*dialed);
                            return st && st->alpn == "h2";
                        }();
                        if (h2) {
                            h = make_tracked<h2::ClientH2>(*dialed, transport_settings(*cfg, known_limit));
                            h->set_on_closed([pool, key, h] {
                                pool->remove_h2(key, h);
                            });
                            h2::ClientH2::start(h);
                            (void)h->reserve();
                            pool->add_h2(key, h);
                        }
                    }
                    pool->end_dial(key, mine);
                    if (!dialed) {
                        a.error = dial_error(dialed.error());
                        co_return a;
                    }
                    if (h) {
                        co_return co_await round_trip_h2(cfg, o, deadline, h);
                    }
                    fresh = *dialed;   // HTTP/1.1 (ALPN)
                    break;
                }
            }
            bool reused = (bool)idle;
            net::connection c;
            tracked_ptr<Wire> wire;
            if (idle) {
                c = idle->c;
                wire = idle->wire;
            } else {
                if (!fresh) {
                    auto dialed = co_await dial_origin(cfg, o, deadline, route);
                    if (!dialed) {
                        a.error = dial_error(dialed.error());
                        co_return a;
                    }
                    fresh = *dialed;
                }
                c = *fresh;
                wire = make_tracked<Wire>(c);
            }
            // a stop closes the connection until the body has ended
            if (o.stop.stop_requested()) {
                (void)c.close();
                a.error = client_error(canceled_error(), o);
                co_return a;
            }
            tracked_ptr<CancelWatch> watch = CancelWatch::arm(o.stop, c, nullptr, 0);
            // a failure past here: the watch ended, a stop's the error
            auto failed = [&](const io::error& e) {
                if (watch) {
                    watch->finish();
                }
                a.error = o.stop.stop_requested() ? client_error(canceled_error(), o) : e;
            };
            c.set_write_deadline(deadline);
            bool chunked = false;
            auto bytes = request_bytes(o, chunked, forward ? &*route.via : nullptr);
            auto written = co_await write_head_and_body(c, std::move(bytes), body_in_memory(o));
            if (written && o.body_kind == RequestImpl::BodyKind::stream) {
                written = co_await write_stream(c, o.stream, o.stream_length, chunked);
            } else if (written && o.body_kind == RequestImpl::BodyKind::form) {
                auto b = form_body(o.form);   // a fresh stream of the form, each attempt its own
                written = b ? co_await write_stream(c, b->first, o.stream_length, false) : expected<void, io::error>(io::detail::fail(b.error()));
            }
            if (!written) {
                (void)co_await c.async_close();
                a.retry = reused && !o.stop.stop_requested();
                failed(client_error(written.error(), o));
                co_return a;
            }
            auto header_deadline = cfg->response_header_timeout > duration::zero() ? sgcl::clock::now() + cfg->response_header_timeout : time_point();
            c.set_read_deadline(header_deadline == time_point() ? deadline : (deadline == time_point() || header_deadline < deadline ? header_deadline : deadline));
            tracked_ptr impl = make_tracked<ResponseImpl>();
            StatusLine line;
            bool first = true;
            for (;;) {
                auto head = co_await wire->read_head(cfg->max_response_header_bytes);
                if (!head || !*head) {
                    (void)co_await c.async_close();
                    a.retry = reused && first && !o.stop.stop_requested() && (!head ? head.error().code() == std::errc::connection_reset || head.error().code() == std::errc::broken_pipe : true);
                    if (!head) {
                        failed(client_error(head.error().code() == net::errc::header_too_large ? net::detail::net_error(net::errc::header_too_large, "read", "") : head.error(), o));
                    } else {
                        failed(client_error(io::error(io::errc::unexpected_eof, "read", ""), o));
                    }
                    co_return a;
                }
                first = false;
                impl->head = **head;
                impl->fields = http::headers();
                if (parse_response_head(impl->head, line, impl->fields)) {
                    (void)co_await c.async_close();
                    failed(client_error(net::errc::malformed_response, o));
                    co_return a;
                }
                if (line.status >= 100 && line.status < 200 && line.status != 101) {
                    // 100 Continue, 103 Early Hints: the final response follows
                    if (o.informational) {
                        co_await o.informational(line.status, impl->fields);
                    }
                    continue;
                }
                break;
            }
            BodyFraming framing;
            if (!response_framing(impl->fields, line.status, o.method.view() == "HEAD", framing)) {
                (void)co_await c.async_close();
                failed(client_error(net::errc::malformed_response, o));
                co_return a;
            }
            c.set_read_deadline(deadline);
            impl->status = line.status;
            impl->minor = line.minor;
            impl->url = *o.target;
            if (framing.kind == Framing::length) {
                impl->content_length.emplace(framing.length);
            } else if (framing.kind == Framing::none && HeadersAccess::count(impl->fields, "content-length")) {
                optional<uint64_t> cl;
                content_length(impl->fields, cl);
                set_optional(impl->content_length, cl);
            }
            bool keep = line.minor == 1 ? !HeadersAccess::has_token(impl->fields, "connection", "close")
                                        : HeadersAccess::has_token(impl->fields, "connection", "keep-alive");
            keep = keep && framing.kind != Framing::until_close && line.status != 101;
            impl->body = make_tracked<Body>(wire, framing, 0, true);
            size_t most = cfg->max_idle_per_host;
            duration idle_timeout = cfg->idle_timeout;
            impl->body->set_on_end([pool, key, c, wire, keep, most, idle_timeout, watch](bool clean) {
                if (watch) {
                    watch->finish();
                }
                if (clean && keep && wire->buffered() == 0) {
                    c.set_deadline(time_point());
                    pool->put(pool, key, IdleConnection{c, wire, sgcl::clock::now()}, most, idle_timeout);
                } else {
                    (void)c.close();
                }
            });
            a.result = ResponseAccess::make(impl);
            co_return a;
        }

        // Whether the headers of the credentials go with a redirect: to the
        // same host or to one under it, as Go decides
        SGCL_INLINE_HOT bool same_or_sub_host(std::string_view from, std::string_view to) noexcept {
            if (from == to) {
                return true;
            }
            return to.size() > from.size() && to.substr(to.size() - from.size()) == from && to[to.size() - from.size() - 1] == '.';
        }

        // The Cookie field of a request with a jar: the request's own pairs
        // (each Cookie field's), less those a response of this exchange set
        // again (the jar holds what it set now), then the jar's, in one
        // field as RFC 6265 §5.4 asks; Go's client merges the same way
        inline string merged_cookie(const http::headers& fields, const vector<string>& reset, const string& from_jar) noexcept {
            std::string out;
            for (auto& f : HeadersAccess::fields(fields)) {
                if (!iequal(f.first.view(), "cookie")) {
                    continue;
                }
                std::string_view v = f.second.view();
                while (!v.empty()) {
                    auto semi = v.find(';');
                    auto pair = trim_ows(v.substr(0, semi));
                    v = semi == std::string_view::npos ? std::string_view() : v.substr(semi + 1);
                    if (pair.empty()) {
                        continue;
                    }
                    auto name = trim_ows(pair.substr(0, pair.find('=')));
                    bool again = false;
                    for (auto& r : reset) {
                        again = again || r.view() == name;
                    }
                    if (!again) {
                        if (!out.empty()) {
                            out += "; ";
                        }
                        out += pair;
                    }
                }
            }
            if (!from_jar.empty()) {
                if (!out.empty()) {
                    out += "; ";
                }
                out += from_jar.view();
            }
            return string(std::string_view(out));
        }

        // A request's fields with the jar's cookies for its target, when the
        // jar has any or a response of the exchange set some again; nullopt
        // when the request goes as it is
        inline optional<http::headers> with_jar(const cookie_jar& jar, const Outgoing& o, const vector<string>& reset) noexcept {
            string from_jar = jar.header(*o.target);
            if (from_jar.empty() && (reset.empty() || !HeadersAccess::count(o.fields, "cookie"))) {
                return nullopt;
            }
            http::headers fields = o.fields;
            string merged = merged_cookie(o.fields, reset, from_jar);
            if (merged.empty()) {
                fields.erase("Cookie");
            } else {
                fields.set("Cookie", merged);
            }
            return fields;
        }

        inline async::task<expected<response, io::error>> send_request(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, tracked_ptr<RequestImpl> req) noexcept {
            Outgoing o;
            o.method = req->method;
            if (!req->url) {
                co_return io::detail::fail(net::detail::net_error(net::errc::invalid_url, req->method, req->url_text));
            }
            o.target = *req->url;
            o.fields = req->fields;
            o.body_kind = req->body_kind;
            o.text = req->text;
            o.bytes = req->bytes;
            o.stream = req->stream;
            o.stream_length = req->stream_length;
            o.form = req->form;
            o.stop = req->stop;
            o.informational = req->on_informational;
            o.no_redirects = req->no_redirects;
            if (o.body_kind == RequestImpl::BodyKind::form) {
                auto b = form_body(o.form);   // its length now, a file that cannot be read the send's error
                if (!b) {
                    co_return io::detail::fail(client_error(b.error(), o));
                }
                o.stream_length = b->second;
            }
            auto deadline = cfg->timeout > duration::zero() ? sgcl::clock::now() + cfg->timeout : time_point();
            vector<string> reset;   // the names of the cookies the exchange's responses set
            for (int redirects = 0;; ++redirects) {
                if (o.target->scheme() != "http" && o.target->scheme() != "https") {
                    co_return io::detail::fail(client_error(net::errc::unsupported_scheme, o));
                }
                if (auto e = unsendable(o)) {
                    co_return io::detail::fail(*e);
                }
                if (o.stop.stop_requested()) {
                    co_return io::detail::fail(client_error(canceled_error(), o));
                }
                // with a jar, this hop's cookies: the request's own and the
                // jar's, the request's own fields put back after the hop
                optional<http::headers> own;
                if (cfg->jar) {
                    if (auto fields = with_jar(*cfg->jar, o, reset)) {
                        own.emplace(std::move(o.fields));
                        o.fields = std::move(*fields);
                        if (auto e = unsendable(o)) {
                            co_return io::detail::fail(*e);
                        }
                    }
                }
                auto a = co_await round_trip(cfg, pool, o, deadline, true);
                if (a.error && a.retry && (o.idempotent() || o.replayable()) && o.body_kind != RequestImpl::BodyKind::stream) {
                    a = co_await round_trip(cfg, pool, o, deadline, false);
                }
                // HTTP/2: a stream never processed goes again (REFUSED_STREAM
                // whatever the method, above a GOAWAY's last or lost with its
                // connection only an idempotent one), twice at most
                for (int again = 0; again < 2 && a.error && o.replayable()
                                    && (a.retry_any || (a.retry_idempotent && o.idempotent())); ++again) {
                    a = co_await round_trip(cfg, pool, o, deadline, true);
                }
                if (own) {
                    o.fields = std::move(*own);
                }
                if (a.error) {
                    co_return io::detail::fail(*a.error);
                }
                auto res = *a.result;
                if (cfg->jar && HeadersAccess::count(ResponseAccess::impl(res)->fields, "set-cookie")) {
                    // every hop's Set-Cookie into the jar, as Go's client does
                    auto set = res.cookies();
                    cfg->jar->set_cookies(*o.target, set);
                    for (auto& c : set) {
                        reset.push_back(c.name);
                    }
                }
                int st = res.status();
                if (o.no_redirects || (st != 301 && st != 302 && st != 303 && st != 307 && st != 308)) {
                    co_return res;
                }
                auto location = res.header("Location");
                if (location.empty()) {
                    co_return res;
                }
                auto next = o.target->resolve(location);
                if (!next) {
                    co_return res;
                }
                if (st == 307 || st == 308) {
                    if (!o.replayable()) {
                        co_return res;   // a stream cannot be sent again
                    }
                } else if (o.method.view() != "GET" && o.method.view() != "HEAD") {
                    o.method = "GET";
                    o.body_kind = RequestImpl::BodyKind::none;
                    o.text = string();
                    o.bytes = vector<byte>();
                    o.fields.erase("Content-Type");
                }
                if (redirects + 1 > cfg->max_redirects) {
                    res.close();
                    co_return io::detail::fail(client_error(net::errc::too_many_redirects, o));
                }
                if (!same_or_sub_host(o.target->hostname().view(), next->hostname().view())) {
                    o.fields.erase("Authorization");
                    o.fields.erase("Cookie");
                    o.fields.erase("Proxy-Authorization");
                    o.fields.erase("WWW-Authenticate");
                }
                res.close();
                o.target = next->without_fragment();
            }
        }
    }

    // An HTTP/1.1 and HTTP/2 client with a pool of connections (Go's Client and
    // Transport in one). A handle of one word: copies share the pool. The
    // settings are public fields, read by each request when it starts.
    //
    // The pool is keyed by the origin (scheme, host, port); idle
    // connections are taken last in first out and dropped past
    // idle_timeout. A connection goes back when the body of its response
    // has been read to its end, and is closed when anything went wrong,
    // the server said Connection: close, or the body was not read. A
    // connection from the pool the server had closed (the request failed
    // before a byte of the response) is tried once more on a new one, for
    // an idempotent method or a body that can be sent again (RFC 9110
    // §9.2.2), as Go does.
    //
    // Redirects are followed up to max_redirects (too_many_redirects past
    // it): 301, 302 and 303 as a GET without a body (HEAD stays HEAD),
    // 307 and 308 with the method and the body, unless the body is a
    // stream (then the 307 is the response). Authorization, Cookie and
    // Proxy-Authorization do not follow to a host that is neither the same
    // nor under it; a redirect may go from http to https and back, as in
    // Go.
    //
    // https:// is over TLS 1.3 (net/tls.h): the connection made by
    // net::tls::connect with the `tls` settings (the system's roots), the
    // server name the URL's host (an IP address is checked against the
    // certificate's addresses), port 443 by default. A dial function given
    // makes the transport and TLS goes over it.
    //
    // HTTP/2 (RFC 9113) is offered first by ALPN ("h2", then "http/1.1")
    // while http2 is on, as Go does; a server that picks it gets one
    // connection per origin shared by the requests, a stream each (a
    // second connection only when the first has as many streams as its
    // server allows), the others HTTP/1.1 as before. h2c sends http:// as
    // HTTP/2 by prior knowledge (no Upgrade), for servers known to speak
    // it. A request's timeouts hold for its stream alone: past one it is
    // reset and the connection goes on. A stream the server never
    // processed goes again on its own: REFUSED_STREAM for any method, one
    // above a GOAWAY's last or lost with its connection for an idempotent
    // one (a body in memory, never a stream's). response::proto() tells
    // which protocol answered.
    //
    // Proxies (proxy.h): the environment's as the client is made, or the
    // one in `proxy`. An http:// request goes to an HTTP proxy in
    // absolute-form with Basic Proxy-Authorization from the proxy's URL,
    // an https:// one through CONNECT's tunnel with TLS (and HTTP/2) to
    // the origin over it, any request through SOCKS5's tunnel; an https://
    // proxy is reached over TLS first. The pool keys a connection by its
    // route: the proxy and the origin for a tunnel, the proxy alone for
    // forwarded requests.
    namespace detail {
        struct ClientAccess;
    }

    class client {
    public:
        SGCL_INLINE_HOT client() noexcept
        : _pool(make_tracked<detail::Pool>()) {
        }

        // A copy shares the pool and copies the settings. There is no move
        // of its own: a moved-from client is the same client, as a moved-
        // from tracked_ptr still points (a member-wise move left its dial
        // and its TLS settings empty, ALPN without http/1.1 among them)
        client(const client&) = default;
        client& operator=(const client&) = default;

        // The request sent, redirects followed: the response, whose body is
        // read next; the error of the connection, or of net: invalid_url,
        // unsupported_scheme, malformed_response, header_too_large,
        // too_many_redirects. A 4xx or 5xx is a response. send() blocks the
        // thread (the exchange runs on the scheduler and the thread waits:
        // never from a worker); in a task `co_await client.async_send(req)`.
        SGCL_INLINE_HOT expected<response, io::error> send(const request& req) const {
            return async_send(req).wait();
        }

        SGCL_INLINE_HOT async::task<expected<response, io::error>> async_send(const request& req) const noexcept {
            return detail::send_request(_settings(), _pool, detail::RequestAccess::impl(req));
        }

        SGCL_INLINE_HOT expected<response, io::error> get(const string& url) const {
            return send(request("GET", url));
        }

        SGCL_INLINE_HOT async::task<expected<response, io::error>> async_get(const string& url) const noexcept {
            return async_send(request("GET", url));
        }

        SGCL_INLINE_HOT expected<response, io::error> head(const string& url) const {
            return send(request("HEAD", url));
        }

        SGCL_INLINE_HOT async::task<expected<response, io::error>> async_head(const string& url) const noexcept {
            return async_send(request("HEAD", url));
        }

        SGCL_INLINE_HOT expected<response, io::error> post(const string& url, const string& content_type, const string& body) const {
            return send(_post(url, content_type, body));
        }

        SGCL_INLINE_HOT async::task<expected<response, io::error>> async_post(const string& url, const string& content_type, const string& body) const noexcept {
            return async_send(_post(url, content_type, body));
        }

        // A form, multipart/form-data (form.h): its fields and its files,
        // each file read from disk as the body goes out
        SGCL_INLINE_HOT expected<response, io::error> post(const string& url, const http::form& f) const {
            return send(request("POST", url).set_body(f));
        }

        SGCL_INLINE_HOT async::task<expected<response, io::error>> async_post(const string& url, const http::form& f) const noexcept {
            return async_send(request("POST", url).set_body(f));
        }

        // The file at url saved to path (curl -fo path url): the body
        // streamed through path + ".part" renamed at its end, a status other
        // than 2xx the error net::errc::http_status and no file; the
        // response, its body read (download.h)
        expected<response, io::error> download(const string& url, const string& path) const;
        async::task<expected<response, io::error>> async_download(string url, string path) const noexcept;

        // A WebSocket to url (ws:// or wss://) through this client's dial:
        // its proxy (as a CONNECT tunnel), its TLS settings with ALPN
        // http/1.1 alone, its dial function (websocket.h)
        // `websocket(...)` on this thread, `co_await async_websocket(...)` in a task
        expected<http::websocket, io::error> websocket(const string& url) const;
        expected<http::websocket, io::error> websocket(const string& url, const http::websocket::options& o) const;
        async::task<expected<http::websocket, io::error>> async_websocket(string url) const noexcept;
        async::task<expected<http::websocket, io::error>> async_websocket(string url, http::websocket::options o) const noexcept;

        // The idle connections of the pool closed now
        SGCL_INLINE_HOT void close_idle_connections() const noexcept {
            _pool->close_all();
        }

        duration timeout = duration::zero();                     // the whole exchange, the body's reading included; zero: none
        duration connect_timeout = std::chrono::seconds(30);
        duration response_header_timeout = duration::zero();     // from the request sent to the head of the response
        duration idle_timeout = std::chrono::seconds(90);        // a connection in the pool
        size_t max_idle_per_host = 16;                           // Go's default is 2
        int max_redirects = 10;
        size_t max_response_header_bytes = 1 << 20;
        // How a connection is made; tcp::connect by default (and TLS over
        // it for https://). A unix socket (Docker's API), a test's
        // connection in memory
        dial_function dial;
        // https://: the TLS settings (roots, groups, cipher suites,
        // insecure_skip_verify, handshake_timeout, a client certificate in
        // identities); the server name is the URL's host when none is set;
        // a session cache of the client's own, shared by its connections
        // (tls.session_cache = nullopt: no resumption)
        net::tls::config tls = _default_tls();
        bool http2 = true;   // https: "h2" offered first by ALPN
        bool h2c = false;    // http:// as HTTP/2 by prior knowledge
        // The proxy of each request: the environment's (http_proxy,
        // HTTPS_PROXY, ALL_PROXY, NO_PROXY) as the client is made, or one
        // set; http::proxy() for none
        http::proxy proxy = http::proxy::from_environment();
        // The cookies of the requests (cookie_jar.h): the Set-Cookie of
        // every response, redirects among them, kept in it, and the Cookie
        // field of every request taken from it, after the request's own
        // pairs; none by default, as in Go
        optional<cookie_jar> jar;

    private:
        friend struct detail::ClientAccess;

        SGCL_INLINE_HOT static net::tls::config _default_tls() noexcept {
            net::tls::config c;
            c.alpn = {string("http/1.1")};
            c.session_cache = net::tls::session_cache();   // the client's connections resume each other's sessions
            return c;
        }

        tracked_ptr<detail::ClientSettings> _settings() const noexcept {
            tracked_ptr cfg = make_tracked<detail::ClientSettings>();
            cfg->timeout = timeout;
            cfg->connect_timeout = connect_timeout;
            cfg->response_header_timeout = response_header_timeout;
            cfg->idle_timeout = idle_timeout;
            cfg->max_idle_per_host = max_idle_per_host;
            cfg->max_redirects = max_redirects;
            cfg->max_response_header_bytes = max_response_header_bytes ? max_response_header_bytes : 1;
            cfg->dial = dial;
            cfg->tls = tls;
            cfg->http2 = http2;
            cfg->h2c = h2c;
            cfg->proxy = proxy;
            cfg->jar = jar;
            // "h2" first in ALPN while http2 is on, out of it while off
            vector<string> alpn;
            if (http2) {
                alpn.push_back(string("h2"));
            }
            for (auto& p : tls.alpn) {
                if (p != "h2") {
                    alpn.push_back(p);
                }
            }
            cfg->tls.alpn = std::move(alpn);
            return cfg;
        }

        SGCL_INLINE_HOT static request _post(const string& url, const string& content_type, const string& body) noexcept {
            request r("POST", url);
            r.set_header("Content-Type", content_type);
            r.set_body(body);
            return r;
        }

        tracked_ptr<detail::Pool> _pool;
    };

    namespace detail {
        // What the library's own users of a client reach (a reverse
        // proxy: the settings taken once, the pool shared)
        struct ClientAccess {
            SGCL_INLINE_HOT static tracked_ptr<ClientSettings> settings(const client& c) noexcept {
                return c._settings();
            }

            SGCL_INLINE_HOT static const tracked_ptr<Pool>& pool(const client& c) noexcept {
                return c._pool;
            }
        };
    }
}

#include "download.h"   // download and the response's json and save: they need JSON and files
#include "detail/doh.h"      // DNS over HTTPS for net::dns: the resolver's transport of an "https://" server
