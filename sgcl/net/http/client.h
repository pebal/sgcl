//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response.h"
#include "status.h"
#include "detail/wire.h"
#include "detail/h2/transport.h"
#include "../connection.h"
#include "../tls.h"
#include "../error.h"
#include "../socket.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/event.h"
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

            void add_h2(const string& key, const tracked_ptr<h2::ClientH2>& h) noexcept {
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
            optional<async::event> begin_dial(const string& key, const async::event& mine) noexcept {
                std::lock_guard<std::mutex> g(lock);
                auto it = h2_dialing.find(key);
                if (it != h2_dialing.end()) {
                    return it->second;
                }
                h2_dialing.insert_or_assign(key, mine);
                return nullopt;
            }

            void end_dial(const string& key, const async::event& mine) {
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
            optional<uint64_t> stream_length;

            bool replayable() const noexcept {
                return body_kind != RequestImpl::BodyKind::stream;
            }

            bool idempotent() const noexcept {
                auto m = method.view();
                return m == "GET" || m == "HEAD" || m == "OPTIONS" || m == "TRACE" || m == "PUT" || m == "DELETE";
            }
        };

        // The pool's key of a connection: scheme://host:port, one string of
        // its pieces (string::concat), the port's digits written on the stack
        inline string origin_key(const net::url& u) noexcept {
            char port[8];
            auto end = std::to_chars(port, port + sizeof port, u.effective_port()).ptr;
            return string::concat(u.scheme(), "://", net::detail::UrlAccess::host_as_written(u), ':', std::string_view(port, size_t(end - port)));
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
        inline slice<const byte> body_in_memory(const Outgoing& o) noexcept {
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
        inline std::string request_bytes(const Outgoing& o, bool& chunked) noexcept {
            std::string s;
            s.reserve(256);
            s += o.method.view();
            s += ' ';
            s += o.target->request_target().view();
            s += " HTTP/1.1\r\n";
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

        // A new connection to the target's origin, TLS for https, within
        // the connect's timeout and the request's deadline
        inline async::task<expected<net::connection, io::error>> dial_origin(tracked_ptr<ClientSettings> cfg, const Outgoing& o, time_point deadline) noexcept {
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

            explicit RequestBlock(const Outgoing& o) noexcept
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
                    break;
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
                auto n = co_await o.stream.async_read(buf);
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
            const bool has_body = o.body_kind == RequestImpl::BodyKind::stream || (o.body_kind == RequestImpl::BodyKind::text && !o.text.empty())
                                  || (o.body_kind == RequestImpl::BodyKind::bytes && !o.bytes.empty());
            RequestBlock block(o);
            if (!h->open(st, block, !has_body)) {
                a.error = client_error(h2::stream_reset_error("write", h2::ErrorCode::refused_stream), o);
                a.retry_any = true;   // never sent
                co_return a;
            }
            h->arm(st, deadline, time_point());
            optional<io::error> own;
            if (has_body) {
                (void)co_await send_body_h2(h, st->id, o, own);   // a failure shows as the stream's fate; an early answer wins
            }
            if (cfg->response_header_timeout > duration::zero()) {
                h->arm(st, time_point(), sgcl::clock::now() + cfg->response_header_timeout);
            }
            Fate fate = Fate::open;
            for (;;) {
                fate = h->fate_of(st);
                if (fate != Fate::open) {
                    break;
                }
                (void)co_await st->headed.receive();
            }
            if (fate == Fate::headed) {
                tracked_ptr<ResponseImpl> impl = st->head;
                impl->url = *o.target;
                impl->body->set_on_end([st](bool) {
                    st->cancel_timers();
                });
                a.result = ResponseAccess::make(impl);
                co_return a;
            }
            st->cancel_timers();
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
        inline h2::TransportSettings transport_settings(const ClientSettings& cfg, uint32_t known_limit) noexcept {
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
            auto key = origin_key(*o.target);
            const bool https = o.target->scheme() == "https";
            optional<IdleConnection> idle = may_reuse ? pool->take(key, cfg->idle_timeout) : optional<IdleConnection>();
            optional<net::connection> fresh;
            if (!idle && (https ? cfg->http2 : cfg->h2c)) {
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
                    auto dialed = co_await dial_origin(cfg, o, deadline);
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
                        a.error = client_error(dialed.error(), o);
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
                    auto dialed = co_await dial_origin(cfg, o, deadline);
                    if (!dialed) {
                        a.error = client_error(dialed.error(), o);
                        co_return a;
                    }
                    fresh = *dialed;
                }
                c = *fresh;
                wire = make_tracked<Wire>(c);
            }
            c.set_write_deadline(deadline);
            bool chunked = false;
            auto bytes = request_bytes(o, chunked);
            auto written = co_await write_head_and_body(c, std::move(bytes), body_in_memory(o));
            if (written && o.body_kind == RequestImpl::BodyKind::stream) {
                written = co_await write_stream(c, o.stream, o.stream_length, chunked);
            }
            if (!written) {
                (void)co_await c.async_close();
                a.retry = reused;
                a.error = client_error(written.error(), o);
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
                    a.retry = reused && first && (!head ? head.error().code() == std::errc::connection_reset || head.error().code() == std::errc::broken_pipe : true);
                    if (!head) {
                        a.error = client_error(head.error().code() == net::errc::header_too_large ? net::detail::net_error(net::errc::header_too_large, "read", "") : head.error(), o);
                    } else {
                        a.error = client_error(io::error(io::errc::unexpected_eof, "read", ""), o);
                    }
                    co_return a;
                }
                first = false;
                impl->head = **head;
                impl->fields = http::headers();
                if (parse_response_head(impl->head, line, impl->fields)) {
                    (void)co_await c.async_close();
                    a.error = client_error(net::errc::malformed_response, o);
                    co_return a;
                }
                if (line.status >= 100 && line.status < 200 && line.status != 101) {
                    continue;   // 100 Continue, 103 Early Hints: the final response follows
                }
                break;
            }
            BodyFraming framing;
            if (!response_framing(impl->fields, line.status, o.method.view() == "HEAD", framing)) {
                (void)co_await c.async_close();
                a.error = client_error(net::errc::malformed_response, o);
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
            impl->body->set_on_end([pool, key, c, wire, keep, most, idle_timeout](bool clean) {
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
        inline bool same_or_sub_host(std::string_view from, std::string_view to) noexcept {
            if (from == to) {
                return true;
            }
            return to.size() > from.size() && to.substr(to.size() - from.size()) == from && to[to.size() - from.size() - 1] == '.';
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
            auto deadline = cfg->timeout > duration::zero() ? sgcl::clock::now() + cfg->timeout : time_point();
            for (int redirects = 0;; ++redirects) {
                if (o.target->scheme() != "http" && o.target->scheme() != "https") {
                    co_return io::detail::fail(client_error(net::errc::unsupported_scheme, o));
                }
                if (auto e = unsendable(o)) {
                    co_return io::detail::fail(*e);
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
                if (a.error) {
                    co_return io::detail::fail(*a.error);
                }
                auto res = *a.result;
                int st = res.status();
                if (st != 301 && st != 302 && st != 303 && st != 307 && st != 308) {
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

    // An HTTP/1.1 client with a pool of connections (Go's Client and
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
    class client {
    public:
        client() noexcept
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
        expected<response, io::error> send(const request& req) const {
            return async_send(req).wait();
        }

        async::task<expected<response, io::error>> async_send(const request& req) const noexcept {
            return detail::send_request(_settings(), _pool, detail::RequestAccess::impl(req));
        }

        expected<response, io::error> get(const string& url) const {
            return send(request("GET", url));
        }

        async::task<expected<response, io::error>> async_get(const string& url) const noexcept {
            return async_send(request("GET", url));
        }

        expected<response, io::error> head(const string& url) const {
            return send(request("HEAD", url));
        }

        async::task<expected<response, io::error>> async_head(const string& url) const noexcept {
            return async_send(request("HEAD", url));
        }

        expected<response, io::error> post(const string& url, const string& content_type, const string& body) const {
            return send(_post(url, content_type, body));
        }

        async::task<expected<response, io::error>> async_post(const string& url, const string& content_type, const string& body) const noexcept {
            return async_send(_post(url, content_type, body));
        }

        // The file at url saved to path (curl -fo path url): the body
        // streamed through path + ".part" renamed at its end, a status other
        // than 2xx the error net::errc::http_status and no file; the
        // response, its body read (download.h)
        expected<response, io::error> download(const string& url, const string& path) const;
        async::task<expected<response, io::error>> async_download(string url, string path) const noexcept;

        // The idle connections of the pool closed now
        void close_idle_connections() const noexcept {
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
        // insecure_skip_verify, handshake_timeout); the server name is the
        // URL's host when none is set
        net::tls::config tls = _default_tls();
        bool http2 = true;   // https: "h2" offered first by ALPN
        bool h2c = false;    // http:// as HTTP/2 by prior knowledge

    private:
        static net::tls::config _default_tls() noexcept {
            net::tls::config c;
            c.alpn = {string("http/1.1")};
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

        static request _post(const string& url, const string& content_type, const string& body) noexcept {
            request r("POST", url);
            r.set_header("Content-Type", content_type);
            r.set_body(body);
            return r;
        }

        tracked_ptr<detail::Pool> _pool;
    };
}

#include "download.h"   // download and the response's json and save: they need JSON and files
