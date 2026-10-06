//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie_jar.h"
#include "proxy.h"
#include "cache.h"
#include "../../compress/gzip.h"
#include "../../compress/brotli.h"
#include "../../compress/limits.h"
#include "../../compress/zlib.h"
#include "../../compress/zstd.h"
#include "detail/auth.h"
#include "../../crypto/random.h"
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
#include <atomic>
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

        // A protection space a 401 of the client's was answered in: the
        // origin, the paths at or under `path` (RFC 7617 §2.2), the scheme
        // and, for Digest, what the challenge gave and the count of its
        // nonce. The next requests under it send Authorization at once
        struct AuthSpace {
            string origin;
            string realm;
            string path;
            bool digest = false;
            DigestAlgorithm algorithm;
            string nonce;
            string opaque;
            string qop;                       // "auth", "auth-int", or "" (RFC 2069's form)
            bool userhash = false;
            std::atomic<uint32_t> nc = {0};
            http::credentials creds;
        };

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
            // the protection spaces the client's 401s were answered in (RFC
            // 9110 §11.5), for the next requests under them (auth spaces)
            vector<tracked_ptr<AuthSpace>> auth_spaces;

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
            bool follow_redirects = true;
            size_t max_response_header_bytes = 1 << 20;
            dial_function dial;
            net::tls::config tls;
            bool http2 = true;
            bool h2c = false;
            http::proxy proxy;
            optional<cookie_jar> jar;
            optional<http::credentials> credentials;
            bool decompress = true;
            optional<http::cache> cache;
            // the Authorization of every request to the origin of its own URL,
            // a token source's (oauth2.h); true asks for a new one (after a
            // 401 with error="invalid_token")
            function<async::task<expected<string, io::error>>(bool)> authorization;
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

        // The space of the pool that covers a target: its origin, a path at
        // or under the space's, the longest of them
        inline tracked_ptr<AuthSpace> find_space(Pool& pool, const string& origin, std::string_view path) noexcept {
            std::lock_guard<std::mutex> g(pool.lock);
            tracked_ptr<AuthSpace> best;
            for (auto& s : pool.auth_spaces) {
                if (s->origin == origin && path.starts_with(s->path.view()) && (!best || s->path.size() > best->path.size())) {
                    best = s;
                }
            }
            return best;
        }

        // A space kept, in the place of one of the same origin and realm
        inline void keep_space(Pool& pool, const tracked_ptr<AuthSpace>& sp) noexcept {
            std::lock_guard<std::mutex> g(pool.lock);
            for (auto& s : pool.auth_spaces) {
                if (s->origin == sp->origin && s->realm == sp->realm) {
                    s = sp;
                    return;
                }
            }
            if (pool.auth_spaces.size() >= 256) {
                pool.auth_spaces.erase(pool.auth_spaces.begin());   // the oldest dropped: a client of many realms
            }
            pool.auth_spaces.push_back(sp);
        }

        // The request-target of origin-form, Digest's uri
        inline std::string target_of(const net::url& u) {
            std::string out(u.path().view());
            if (out.empty()) {
                out = "/";
            }
            if (u.has_query()) {
                out += '?';
                out += u.query().view();
            }
            return out;
        }

        // The body of an attempt as it goes, when it is in memory, for
        // auth-int's hash; nullopt for a stream or a form
        inline optional<std::string_view> auth_body_of(const Outgoing& o) noexcept {
            switch (o.body_kind) {
                case RequestImpl::BodyKind::none:
                    return std::string_view();
                case RequestImpl::BodyKind::text:
                    return o.text.view();
                case RequestImpl::BodyKind::bytes:
                    return std::string_view(reinterpret_cast<const char*>(o.bytes.data()), o.bytes.size());
                default:
                    return nullopt;
            }
        }

        // The Authorization of a space for an attempt: Basic's, or Digest's
        // response with the next count of its nonce and a fresh cnonce
        // (RFC 7616 §3.4)
        inline string space_authorization(AuthSpace& sp, const Outgoing& o) {
            if (!sp.digest) {
                return basic_credentials(sp.creds.user.view(), sp.creds.password.view());
            }
            const uint32_t nc = sp.nc.fetch_add(1, std::memory_order_relaxed) + 1;
            char ncs[9];
            std::snprintf(ncs, sizeof ncs, "%08x", nc);
            byte raw[16];
            crypto::random::fill(slice<byte>(raw, sizeof raw));
            std::string cnonce;
            append_hex_bytes(cnonce, reinterpret_cast<const unsigned char*>(raw), sizeof raw);
            const std::string uri = target_of(*o.target);
            std::string body_hash;
            if (sp.qop == "auth-int") {
                body_hash = digest_hex(sp.algorithm.hash, auth_body_of(o).value_or(std::string_view()));
            }
            DigestInput in;
            in.algorithm = sp.algorithm;
            in.user = sp.creds.user.view();
            in.realm = sp.realm.view();
            in.password = sp.creds.password.view();
            in.method = o.method.view();
            in.uri = uri;
            in.nonce = sp.nonce.view();
            in.cnonce = cnonce;
            in.nc = std::string_view(ncs, 8);
            in.qop = sp.qop.view();
            in.body_hash = body_hash;
            std::string h = "Digest ";
            const std::string_view user = sp.creds.user.view();
            bool ascii = true;
            for (unsigned char c : user) {
                ascii = ascii && c >= 0x20 && c < 0x7F;
            }
            if (sp.userhash) {
                h += "username=\"";
                h += digest_hex(sp.algorithm.hash, std::string(user) + ":" + std::string(sp.realm.view()));
                h += "\"";
            } else if (ascii) {
                h += "username=";
                append_quoted(h, user);
            } else {
                h += "username*=";
                h += encode_ext_value(user);
            }
            h += ", realm=";
            append_quoted(h, sp.realm.view());
            h += ", uri=";
            append_quoted(h, uri);
            h += ", algorithm=";
            h += digest_algorithm_name(sp.algorithm);
            h += ", nonce=";
            append_quoted(h, sp.nonce.view());
            if (!sp.qop.empty()) {
                h += ", qop=";
                h += sp.qop.view();
                h += ", nc=";
                h.append(ncs, 8);
                h += ", cnonce=\"";
                h += cnonce;
                h += "\"";
            }
            h += ", response=\"";
            h += digest_response(in);
            h += "\"";
            if (!sp.opaque.empty()) {
                h += ", opaque=";
                append_quoted(h, sp.opaque.view());
            }
            if (sp.userhash) {
                h += ", userhash=true";
            }
            return string(std::string_view(h));
        }

        // The space a 401 is answered in, made of its best challenge: Digest
        // of the strongest algorithm of ours (SHA-512/256, SHA-256, MD5; a
        // -sess form after its plain one) with qop auth (auth-int when it is
        // the only one and the body is in memory; RFC 2069's form when no
        // qop is offered), else Basic; null when it offers neither
        inline tracked_ptr<AuthSpace> space_of_challenges(const http::headers& fields, const Outgoing& o, const string& origin,
                                                          const http::credentials& creds) {
            tracked_ptr<AuthSpace> best;
            int best_rank = -1;
            for (auto& value : fields.get_all("WWW-Authenticate")) {
                for (auto& c : parse_challenges(value.view())) {
                    int rank = -1;
                    tracked_ptr sp = make_tracked<AuthSpace>();
                    if (iequal(c.scheme, "basic")) {
                        rank = 0;
                    } else if (iequal(c.scheme, "digest") && c.param("nonce")) {
                        auto a = digest_algorithm(c.param("algorithm") ? std::string_view(*c.param("algorithm")) : std::string_view("MD5"));
                        if (!a) {
                            continue;
                        }
                        std::string qop;
                        if (auto q = c.param("qop")) {
                            bool auth = false, integrity = false;
                            std::string_view list = *q;
                            while (!list.empty()) {
                                const size_t comma = list.find(',');
                                const std::string_view item = trim_ows(list.substr(0, comma));
                                list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
                                auth = auth || item == "auth";
                                integrity = integrity || item == "auth-int";
                            }
                            if (auth) {
                                qop = "auth";
                            } else if (integrity && auth_body_of(o)) {
                                qop = "auth-int";
                            } else {
                                continue;
                            }
                        }
                        rank = 10 + (a->hash == DigestHash::sha512_256 ? 30 : a->hash == DigestHash::sha256 ? 20 : 10) - (a->sess ? 1 : 0);
                        sp->digest = true;
                        sp->algorithm = *a;
                        sp->nonce = string(std::string_view(*c.param("nonce")));
                        if (auto op = c.param("opaque")) {
                            sp->opaque = string(std::string_view(*op));
                        }
                        sp->qop = string(std::string_view(qop));
                        auto uh = c.param("userhash");
                        sp->userhash = uh && iequal(*uh, "true");
                    } else {
                        continue;
                    }
                    if (rank > best_rank) {
                        if (auto realm = c.param("realm")) {
                            sp->realm = string(std::string_view(*realm));
                        }
                        best = sp;
                        best_rank = rank;
                    }
                }
            }
            if (best) {
                best->origin = origin;
                best->creds = creds;
                // the space: the paths at or under the directory of the target
                std::string_view path = o.target->path().view();
                const size_t slash = path.rfind('/');
                best->path = string(slash == std::string_view::npos ? std::string_view("/") : path.substr(0, slash + 1));
            }
            return best;
        }

        // Whether a 401 refuses a Bearer token as invalid_token (RFC 6750 §3.1)
        inline bool bearer_refused(const http::headers& fields) {
            for (auto& value : fields.get_all("WWW-Authenticate")) {
                for (auto& c : parse_challenges(value.view())) {
                    if (iequal(c.scheme, "bearer")) {
                        if (auto e = c.param("error"); e && *e == "invalid_token") {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        // Whether a 401's best challenge says the nonce we sent is stale
        inline bool challenge_stale(const http::headers& fields) {
            for (auto& value : fields.get_all("WWW-Authenticate")) {
                for (auto& c : parse_challenges(value.view())) {
                    if (iequal(c.scheme, "digest")) {
                        if (auto s = c.param("stale"); s && iequal(*s, "true")) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        // The codings the client asks for by itself (client::decompress)
        inline constexpr std::string_view DecodedCodings = "gzip, deflate, br, zstd";

        // A response in the coding the client asked for, decoded as it is
        // read: a reader of gzip, of deflate (zlib's framing, RFC 9110
        // §8.4.1.2), of br or of zstd over the body; Content-Encoding and Content-Length gone.
        // A coding of another name, or several, leaves the response as it
        // came, as does a response without a body
        inline void decode_body(const response& res, std::string_view method) {
            auto& impl = *ResponseAccess::impl(res);
            if (method == "HEAD" || impl.status == 204 || impl.status == 304 || (impl.status >= 100 && impl.status < 200) || !impl.body) {
                return;
            }
            auto coding = HeadersAccess::find(impl.fields, "content-encoding");
            if (!coding || HeadersAccess::count(impl.fields, "content-encoding") != 1) {
                return;
            }
            const std::string_view c = trim_ows(*coding);
            if (iequal(c, "gzip") || iequal(c, "x-gzip")) {
                impl.decoded = io::reader(make_tracked<compress::gzip::reader>(io::reader(impl.body)));
            } else if (iequal(c, "deflate")) {
                impl.decoded = io::reader(make_tracked<compress::zlib::reader>(io::reader(impl.body)));
            } else if (iequal(c, "br")) {
                impl.decoded = io::reader(make_tracked<compress::brotli::reader>(io::reader(impl.body)));
            } else if (iequal(c, "zstd")) {
                // RFC 9659 §3: a window past 8 MB may be refused; twice the window held, so 16 MB and a margin
                compress::limits l;
                l.max_memory = uint64_t(32) << 20;
                impl.decoded = io::reader(make_tracked<compress::zstd::reader>(io::reader(impl.body), l));
            } else {
                return;
            }
            impl.uncompressed = true;
            impl.reads_decoded = true;
            impl.fields.erase("Content-Encoding");
            impl.fields.erase("Content-Length");
            impl.content_length.reset();
        }

        // --- the cache (cache.h) ----------------------------------------------

        inline int64_t cache_now() noexcept {
            return time::now().unix();
        }

        // A stored response as a response: its head with Age, its body from
        // memory (or its file); HEAD gets the head alone
        // A stored response's body, from memory or from its file (read on the
        // scheduler's I/O); nothing for HEAD
        inline async::task<std::string> cached_bytes(tracked_ptr<CacheEntry> e, bool head) noexcept {
            std::string out;
            if (head) {
                co_return out;
            }
            if (e->file.empty()) {
                out.assign(reinterpret_cast<const char*>(e->body.data()), e->body.size());
                co_return out;
            }
            if (auto data = co_await io::async_read_file(e->file + ".body")) {
                out.assign(reinterpret_cast<const char*>(data->data()), data->size());
            }
            co_return out;
        }

        // A disk entry's head written again (after a 304 renewed it)
        inline async::task<> save_head(tracked_ptr<CacheState> st, tracked_ptr<CacheEntry> e) noexcept {
            const string tmp = e->file + ".head.tmp";
            if (co_await io::async_write_file(tmp, string(std::string_view(st->head_of(*e))))) {
                (void)io::rename(tmp, e->file + ".head");
            }
        }

        inline response cached_response(const CacheEntry& e, const Outgoing& o, int64_t age, response::cache_status how, const std::string& bytes) {
            tracked_ptr impl = make_tracked<ResponseImpl>();
            impl->status = e.status;
            impl->fields = e.fields;
            impl->fields.set("Age", string(std::to_string(age < 0 ? 0 : age)));
            impl->url = *o.target;
            impl->uncompressed = e.uncompressed;
            impl->cache = uint8_t(how);
            impl->age = age < 0 ? 0 : age;
            auto ends = net::connection::in_memory();
            tracked_ptr wire = make_tracked<Wire>(ends.first);
            wire->preload(bytes);
            const bool none = bytes.empty();
            impl->body = make_tracked<Body>(wire, BodyFraming{none ? Framing::none : Framing::length, bytes.size()}, 0, true);
            if (o.method.view() != "HEAD") {
                impl->content_length = uint64_t(bytes.size());
            }
            return ResponseAccess::make(impl);
        }

        // The 304 of a revalidation merged into the stored head (RFC 9111
        // §3.2): each field of the 304 replaces the stored ones of its name,
        // but the framing's; the times renewed
        //
        // A stored entry is never changed in place (a response may be reading
        // it): the renewed one is a copy that takes its place
        inline tracked_ptr<CacheEntry> refresh_entry(CacheState& st, const tracked_ptr<CacheEntry>& old, const http::headers& fresh, int64_t request_time,
                                                     int64_t response_time) {
            tracked_ptr renewed = make_tracked<CacheEntry>(*old);
            CacheEntry& e = *renewed;
            e.revalidating = false;
            for (auto& f : HeadersAccess::fields(fresh)) {
                const std::string_view n = f.first.view();
                if (iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "content-encoding") || iequal(n, "connection")) {
                    continue;
                }
                e.fields.erase(string(n));
            }
            for (auto& f : HeadersAccess::fields(fresh)) {
                const std::string_view n = f.first.view();
                if (iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "content-encoding") || iequal(n, "connection")) {
                    continue;
                }
                e.fields.add(string(n), string(f.second.view()));
            }
            e.request_time = request_time;
            e.response_time = response_time;
            st.store(renewed);
            return renewed;
        }

        // What a response's body is read through to be stored as it goes: a
        // copy kept to its end (within max_entry_bytes), then the entry put in
        // the cache; a body broken off, or past the limit, stores nothing
        struct CacheTee : io::mixin::reader<CacheTee> {
            io::reader inner;
            tracked_ptr<CacheState> st;
            tracked_ptr<CacheEntry> entry;
            std::string kept;
            bool dropped = false;
            bool stored = false;

            expected<size_t, io::error> read(const slice<byte>& out) {
                return async_read(out).wait();
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
                auto n = co_await inner.async_read(out);
                if (!n) {
                    dropped = true;
                    co_return n;
                }
                if (*n == 0) {
                    if (!dropped && !stored) {
                        stored = true;
                        entry->size = kept.size();
                        if (st->directory.empty()) {
                            entry->body.assign(reinterpret_cast<const byte*>(kept.data()), reinterpret_cast<const byte*>(kept.data()) + kept.size());
                            st->store(entry);
                        } else {
                            // the body, then the head, each written whole and renamed into place
                            const string file = st->file_of(*entry);
                            const string body_tmp = file + ".body.tmp";
                            const string head_tmp = file + ".head.tmp";
                            const bool body_ok = co_await io::async_write_file(body_tmp, slice<const byte>(reinterpret_cast<const byte*>(kept.data()), kept.size())) &&
                                                 io::rename(body_tmp, file + ".body");
                            if (body_ok && co_await io::async_write_file(head_tmp, string(std::string_view(st->head_of(*entry)))) &&
                                io::rename(head_tmp, file + ".head")) {
                                entry->file = file;
                                st->store(entry);
                            }
                        }
                        kept = std::string();
                    }
                    co_return n;
                }
                if (!dropped) {
                    if (kept.size() + *n > st->max_entry_bytes) {
                        dropped = true;
                        kept = std::string();
                    } else {
                        kept.append(reinterpret_cast<const char*>(out.data()), *n);
                    }
                }
                co_return n;
            }
        };

        // A response stored as its body is read (or at once, for one without
        // a body); the request's Vary values kept
        inline void store_response(const tracked_ptr<CacheState>& st, const response& res, const Outgoing& o, const string& key,
                                   int64_t request_time, int64_t response_time) {
            auto& impl = *ResponseAccess::impl(res);
            tracked_ptr e = make_tracked<CacheEntry>();
            e->url = key;
            e->vary = CacheState::vary_of(impl.fields, o.fields);
            e->status = impl.status;
            e->fields = impl.fields;
            e->request_time = request_time;
            e->response_time = response_time;
            e->uncompressed = impl.uncompressed;
            if (o.method.view() == "HEAD") {
                return;   // a HEAD's response has no body to store: only a GET's is kept
            }
            tracked_ptr tee = make_tracked<CacheTee>();
            tee->inner = impl.reads_decoded ? impl.decoded : io::reader(impl.body);
            tee->st = st;
            tee->entry = e;
            impl.decoded = io::reader(tee);
            impl.reads_decoded = true;
        }

        // A successful unsafe request drops what the cache holds of its URL,
        // and of its Location and Content-Location of the same origin (§4.4)
        inline void invalidate(CacheState& st, const net::url& target, const response& res) {
            st.erase_url(target.without_fragment().to_string());
            for (const char* name : {"Location", "Content-Location"}) {
                auto v = res.header(name);
                if (v.empty()) {
                    continue;
                }
                if (auto u = target.resolve(v); u && u->scheme() == target.scheme() && u->host() == target.host()) {
                    st.erase_url(u->without_fragment().to_string());
                }
            }
        }

        // A stale response revalidated in the background (stale-while-
        // revalidate): the request sent again as a validation, its answer
        // stored or merged by the exchange itself
        inline async::task<expected<response, io::error>> send_request(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, tracked_ptr<RequestImpl> req) noexcept;

        inline async::task<> revalidate_in_background(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, tracked_ptr<RequestImpl> req,
                                                      tracked_ptr<CacheEntry> e) noexcept {
            auto res = co_await send_request(cfg, pool, req);
            if (res) {
                (void)co_await res->async_bytes();   // the body read: a new response stored by the tee
            }
            std::atomic_ref<bool>(e->revalidating).store(false);
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
            // the credentials of the exchange, for the origin of its first
            // URL alone: the request's own, a URL's user:password@ (sent at
            // once as Basic, as Go does), or the client's
            const string auth_origin = origin_key(*o.target);
            optional<http::credentials> creds = req->credentials;
            bool preemptive = false;
            if (!creds && !o.target->username().empty()) {
                creds.emplace(http::credentials{string(std::string_view(net::detail::url_unescape(o.target->username().view()))),
                                                string(std::string_view(net::detail::url_unescape(o.target->password().view())))});
                preemptive = true;
            }
            if (!creds) {
                creds = cfg->credentials;
            }
            // the codings asked for by the client itself: decoded when they come
            const bool asked_coding = cfg->decompress && !HeadersAccess::count(o.fields, "accept-encoding") && !HeadersAccess::count(o.fields, "range");
            if (asked_coding) {
                o.fields.set("Accept-Encoding", string(DecodedCodings));
            }
            // the cache: a stored response of the URL served, revalidated or
            // asked again with its validators (RFC 9111 §4)
            tracked_ptr<CacheState> cst = cfg->cache ? CacheAccess::state(*cfg->cache) : tracked_ptr<CacheState>();
            const bool cache_method = o.method.view() == "GET" || o.method.view() == "HEAD";
            const string cache_key = cst ? o.target->without_fragment().to_string() : string();
            const int64_t request_time = cst ? cache_now() : 0;
            tracked_ptr<CacheEntry> cached;
            CacheVerdict verdict;
            bool conditional = false;
            if (cst && cache_method) {
                const CacheControl req_cc = cache_control(o.fields);
                if (!req_cc.no_store) {
                    cached = cst->find(cache_key, o.fields);
                }
                if (cached) {
                    const CacheControl res_cc = cache_control(cached->fields);
                    const CacheTimes times = cache_times(cached->fields, cached->request_time, cached->response_time);
                    const int64_t age = current_age(times, request_time);
                    const int64_t lifetime = freshness_lifetime(cached->fields, res_cc, cached->status, times, cst->heuristic, cst->heuristic_max);
                    verdict = cache_verdict(req_cc, res_cc, lifetime, age);
                    if (req->cache_validate && verdict.use != CacheUse::validate) {
                        verdict.use = CacheUse::validate;   // a background revalidation validates
                    }
                    if (verdict.use == CacheUse::fresh) {
                        co_return cached_response(*cached, o, age, response::cache_status::hit, co_await cached_bytes(cached, o.method.view() == "HEAD"));
                    }
                    if (verdict.use == CacheUse::stale_revalidating) {
                        if (!std::atomic_ref<bool>(cached->revalidating).exchange(true)) {
                            // the request again, without a body (a GET or a HEAD), validating
                            tracked_ptr again = make_tracked<RequestImpl>();
                            again->method = req->method;
                            again->url_text = req->url_text;
                            again->url = req->url;
                            again->fields = req->fields;
                            again->credentials = req->credentials;
                            again->cache_validate = true;
                            async::go(revalidate_in_background(cfg, pool, again, cached));
                        }
                        co_return cached_response(*cached, o, age, response::cache_status::stale, co_await cached_bytes(cached, o.method.view() == "HEAD"));
                    }
                    const bool own_conditions = HeadersAccess::count(o.fields, "if-none-match") || HeadersAccess::count(o.fields, "if-modified-since");
                    if (!own_conditions) {
                        if (auto tag = HeadersAccess::find(cached->fields, "etag")) {
                            o.fields.set("If-None-Match", string(*tag));
                            conditional = true;
                        }
                        if (auto lm = HeadersAccess::find(cached->fields, "last-modified")) {
                            o.fields.set("If-Modified-Since", string(*lm));
                            conditional = true;
                        }
                    }
                } else if (req_cc.only_if_cached) {
                    // §5.2.1.7: nothing stored, nothing asked: 504
                    tracked_ptr impl = make_tracked<ResponseImpl>();
                    impl->status = 504;
                    impl->url = *o.target;
                    auto ends = net::connection::in_memory();
                    impl->body = make_tracked<Body>(make_tracked<Wire>(ends.first), BodyFraming{Framing::none, 0}, 0, true);
                    co_return ResponseAccess::make(impl);
                }
            }
            // the final response: decoded when the client asked for a coding,
            // stored when the cache may keep it (a first hop's: a response
            // reached through a redirect is not kept), the cache's entries
            // of the URL dropped after an unsafe request
            auto finish = [&](response res, int redirects) -> response {
                if (asked_coding) {
                    decode_body(res, o.method.view());
                }
                auto& impl = *ResponseAccess::impl(res);
                if (cst) {
                    impl.cache = uint8_t(response::cache_status::miss);
                    if (!cache_method && res.status() < 400) {
                        invalidate(*cst, *o.target, res);
                    } else if (cache_method && redirects == 0 &&
                               storable(o.method.view(), res.status(), o.fields, cache_control(o.fields), impl.fields, cache_control(impl.fields))) {
                        store_response(cst, res, o, cache_key, request_time, cache_now());
                    }
                }
                return res;
            };
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
                // credentials for this hop: at once for a space remembered (or
                // a URL's user:password@), after a 401 otherwise
                const bool may_auth = creds && origin_key(*o.target) == auth_origin && !HeadersAccess::count(o.fields, "authorization");
                const bool bearer = cfg->authorization && origin_key(*o.target) == auth_origin && !HeadersAccess::count(o.fields, "authorization");
                if (bearer) {
                    auto a = co_await cfg->authorization(false);
                    if (!a) {
                        co_return io::detail::fail(a.error());
                    }
                    o.fields.set("Authorization", *a);
                }
                bool ours = false;
                string sent_realm;   // the realm of the space our Authorization was of
                if (may_auth) {
                    if (preemptive) {
                        o.fields.set("Authorization", basic_credentials(creds->user.view(), creds->password.view()));
                        ours = true;
                    } else if (auto sp = find_space(*pool, auth_origin, o.target->path().view())) {
                        o.fields.set("Authorization", space_authorization(*sp, o));
                        ours = true;
                        sent_realm = sp->realm;
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
                // a 401 answered: once with the credentials, again for a
                // stale nonce; ours refused (not stale), the 401 is the answer
                for (int tries = 0; may_auth && !a.error && a.result->status() == 401 && tries < 2 && o.replayable(); ++tries) {
                    const http::headers& got = ResponseAccess::impl(*a.result)->fields;
                    const bool stale = challenge_stale(got);
                    auto sp = space_of_challenges(got, o, auth_origin, *creds);
                    if (!sp || (ours && !stale && sp->realm == sent_realm)) {
                        break;   // nothing of ours offered, or our credentials refused in their own realm
                    }
                    keep_space(*pool, sp);
                    sent_realm = sp->realm;
                    a.result->close();
                    o.fields.set("Authorization", space_authorization(*sp, o));
                    ours = true;
                    a = co_await round_trip(cfg, pool, o, deadline, true);
                    if (a.error && a.retry && o.replayable()) {
                        a = co_await round_trip(cfg, pool, o, deadline, false);
                    }
                }
                // RFC 6750 §3.1: the token refused as invalid_token, a new one
                // asked for and the request sent again, once
                if (bearer && !a.error && a.result->status() == 401 && o.replayable() && bearer_refused(ResponseAccess::impl(*a.result)->fields)) {
                    auto fresh = co_await cfg->authorization(true);
                    if (fresh) {
                        a.result->close();
                        o.fields.set("Authorization", *fresh);
                        a = co_await round_trip(cfg, pool, o, deadline, true);
                        if (a.error && a.retry && o.replayable()) {
                            a = co_await round_trip(cfg, pool, o, deadline, false);
                        }
                    }
                }
                if (ours || bearer) {
                    o.fields.erase("Authorization");   // the next hop makes its own
                }
                if (own) {
                    o.fields = std::move(*own);
                }
                if (redirects == 0 && cached && (a.error || (a.result->status() >= 500 && a.result->status() <= 504 && a.result->status() != 501))
                    && verdict.stale_on_error) {
                    if (!a.error) {
                        a.result->close();
                    }
                    co_return cached_response(*cached, o, verdict.age, response::cache_status::stale,
                                              co_await cached_bytes(cached, o.method.view() == "HEAD"));   // stale-if-error
                }
                if (a.error) {
                    co_return io::detail::fail(*a.error);
                }
                auto res = *a.result;
                if (redirects == 0 && cached && conditional && res.status() == 304) {
                    res.close();
                    cached = refresh_entry(*cst, cached, ResponseAccess::impl(res)->fields, request_time, cache_now());
                    if (!cached->file.empty()) {
                        co_await save_head(cst, cached);
                    }
                    co_return cached_response(*cached, o, 0, response::cache_status::revalidated, co_await cached_bytes(cached, o.method.view() == "HEAD"));
                }
                if (cfg->jar && HeadersAccess::count(ResponseAccess::impl(res)->fields, "set-cookie")) {
                    // every hop's Set-Cookie into the jar, as Go's client does
                    auto set = res.cookies();
                    cfg->jar->set_cookies(*o.target, set);
                    for (auto& c : set) {
                        reset.push_back(c.name);
                    }
                }
                int st = res.status();
                if (o.no_redirects || !cfg->follow_redirects || (st != 301 && st != 302 && st != 303 && st != 307 && st != 308)) {
                    co_return finish(res, redirects);
                }
                auto location = res.header("Location");
                if (location.empty()) {
                    co_return finish(res, redirects);
                }
                auto next = o.target->resolve(location);
                if (!next) {
                    co_return finish(res, redirects);
                }
                if (st == 307 || st == 308) {
                    if (!o.replayable()) {
                        co_return finish(res, redirects);   // a stream cannot be sent again
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
    // Go. With follow_redirects off the 3xx itself is the response (Go's
    // CheckRedirect returning ErrUseLastResponse), its Location unread.
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

    // How a download goes on after a transfer stopped (download.h)
    struct download_options {
        // A path + ".part" left by an earlier download of the same URL
        // continued: Range from its size with If-Range and the validator
        // kept beside it in path + ".part.meta". A failure of the transfer
        // then keeps both for the next call (a status error never does)
        bool resume = false;
        // A body broken off in this call continued from where it stopped
        // (Range with If-Range), this many times, when the server named a
        // strong validator (a strong ETag, or Last-Modified)
        int retries = 2;
    };

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
        // A body broken off is continued from where it stopped (Range
        // with If-Range) o.retries times, when the server named a strong
        // validator; o.resume continues a part an earlier call left
        expected<response, io::error> download(const string& url, const string& path, const download_options& o = {}) const;
        async::task<expected<response, io::error>> async_download(string url, string path, download_options o = {}) const noexcept;

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
        // false: no redirect followed, a 3xx is the response as it came
        // (its Location, its body), Go's CheckRedirect returning
        // ErrUseLastResponse; its Set-Cookie still goes into the jar
        bool follow_redirects = true;
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
        // The credentials of the requests (auth): a 401 of the origin of a
        // request's own URL (and of its redirects within that origin)
        // answered once with them, Digest when the server offers it (the
        // strongest algorithm: SHA-512/256, SHA-256, MD5), else Basic; the
        // protection space then remembered in the pool, so that the next
        // requests under it send Authorization at once (Digest: the nonce
        // again, its count grown; stale=true answered with the new nonce).
        // A request's own (request::set_credentials) wins, and so does a
        // URL's user:password@, which goes as Basic at once, as Go sends it.
        // None by default
        optional<http::credentials> credentials;
        // Accept-Encoding: gzip, deflate sent by the client itself on a
        // request that has no Accept-Encoding of its own and no Range, and
        // the body decoded as it is read: the response's Content-Encoding and
        // Content-Length removed, response::uncompressed() true (Go's
        // transparent gzip, with deflate too). false: nothing sent, nothing
        // decoded (Go's DisableCompression). A request that sets its own
        // Accept-Encoding gets the body as it came
        bool decompress = true;
        // The private cache of the client's GET and HEAD (cache.h, RFC 9111):
        // a fresh stored response served with no request, a stale one asked
        // again with its validators, stale-while-revalidate and
        // stale-if-error; none by default
        optional<http::cache> cache;

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
            cfg->follow_redirects = follow_redirects;
            cfg->max_response_header_bytes = max_response_header_bytes ? max_response_header_bytes : 1;
            cfg->dial = dial;
            cfg->tls = tls;
            cfg->http2 = http2;
            cfg->h2c = h2c;
            cfg->proxy = proxy;
            cfg->jar = jar;
            cfg->credentials = credentials;
            cfg->decompress = decompress;
            cfg->cache = cache;
            cfg->authorization = _authorization;
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
        function<async::task<expected<string, io::error>>(bool)> _authorization;   // oauth2::token_source::client's
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

            // The Authorization every request gets (oauth2::token_source::client)
            static void set_authorization(client& c, function<async::task<expected<string, io::error>>(bool)> f) noexcept {
                c._authorization = std::move(f);
            }
        };
    }
}

#include "download.h"   // download and the response's json and save: they need JSON and files
#include "detail/doh.h"      // DNS over HTTPS for net::dns: the resolver's transport of an "https://" server
