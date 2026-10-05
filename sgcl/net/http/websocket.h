//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response_writer.h"
#include "status.h"
#include "detail/websocket.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timeout.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// WebSocket (RFC 6455), both sides: a client (ws:// and wss://, through the
// HTTP client's dial, so its proxies, TLS settings and dial function
// apply) and a server's upgrade from a handler, over one connection type
// that sends and receives messages. permessage-deflate (RFC 7692) when both
// sides agree to it. Not over HTTP/2 (RFC 8441): the client asks for
// HTTP/1.1 by ALPN, and the server's accept of an HTTP/2 request fails.
//
//   auto ws = net::http::websocket::connect("wss://example.com/chat");
//   ws->send("hello");
//   auto m = ws->receive();
namespace sgcl::net::http {
    class websocket;

    namespace detail {
        // How long a close waits for the peer's close frame before it
        // closes the connection anyway
        inline constexpr duration WsCloseWait = 5 * second;

        // The read buffer: two to a page
        inline constexpr size_t WsReadBuffer = 32768;

        struct WsImpl {
            net::connection c;
            bool client = false;              // masks its frames, expects them unmasked
            size_t max_message = 0;
            string subprotocol;
            std::unique_ptr<WsDeflate> deflate;

            // what has come and not been read: a managed vector, so that a
            // read's slice holds it
            vector<byte> in;
            size_t at = 0, end = 0;

            async::mutex read_lock;
            async::mutex write_lock;          // a frame's write, and the compressor's order
            std::vector<uint8_t> scratch;     // a client's masked payload, a compressed one (under write_lock)

            std::atomic<bool> close_sent{false};
            std::atomic<bool> close_received{false};
            std::atomic<bool> tcp_closed{false};
            std::atomic<int64_t> last_heard{0};   // the clock's nanoseconds of the last frame read
            async::detail::ChannelState<void> done{0};   // closed with the connection: the watchers' end

            // the end, once known (set before tcp_closed): the peer's close
            // code and reason, or the failure
            std::mutex end_lock;
            uint16_t close_status = 0;
            std::string close_reason;
            optional<io::error> failure;

            // the assembly of a message across frames
            bool assembling = false;
            bool assembling_binary = false;
            bool assembling_compressed = false;
            vector<byte> message;
            Utf8Check utf8;

            SGCL_INLINE_HOT void heard() noexcept {
                last_heard.store(sgcl::clock::now().time_since_epoch().count(), std::memory_order_relaxed);
            }

            // The connection closed, once; what waits on `done` woken
            void close_tcp() noexcept {
                if (!tcp_closed.exchange(true)) {
                    (void)c.close();
                    done.close();
                }
            }

            void fail(const io::error& e) noexcept {
                {
                    std::lock_guard g(end_lock);
                    if (!failure) {
                        failure = e;
                    }
                }
                close_tcp();
            }

            io::error ended() noexcept {
                std::lock_guard g(end_lock);
                if (failure) {
                    return *failure;
                }
                return io::error(io::errc::closed, "websocket", "connection");
            }
        };

        inline io::error ws_error(net::errc e, const char* what) noexcept {
            return net::detail::net_error(e, "websocket", string(what));
        }

        // One frame out: the header, the payload (masked by a client into
        // the scratch), as one write; under the write lock
        inline async::task<expected<void, io::error>> ws_write_frame(tracked_ptr<WsImpl> s, WsOpcode op, slice<const byte> payload, bool data_frame) noexcept {
            auto guard = co_await s->write_lock.scoped_lock();
            if (s->tcp_closed.load() || (s->close_sent.load() && op != WsOpcode::close)) {
                co_return io::detail::fail(s->ended());
            }
            if (op == WsOpcode::close) {
                if (s->close_sent.exchange(true)) {
                    co_return expected<void, io::error>();   // sent once
                }
            }
            const uint8_t* p = reinterpret_cast<const uint8_t*>(payload.data());
            size_t n = payload.size();
            bool rsv1 = false;
            if (data_frame && s->deflate) {
                s->deflate->compress(p, n, s->scratch);
                p = s->scratch.data();
                n = s->scratch.size();
                rsv1 = true;
            }
            uint32_t mask = 0;
            if (s->client) {
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(&mask), sizeof mask));
                if (p != s->scratch.data()) {
                    s->scratch.resize(n);
                    if (n) {
                        sgcl::detail::copy_bytes(s->scratch.data(), p, n);
                    }
                    p = s->scratch.data();
                }
                ws_mask(s->scratch.data(), n, mask, 0);
            }
            uint8_t header[14];
            size_t h = write_ws_header(header, true, rsv1, op, n, s->client, mask);
            expected<size_t, io::error> w = size_t(0);
            if (n <= 1024) {
                // small: one buffer, one write
                std::string one(reinterpret_cast<const char*>(header), h);
                one.append(reinterpret_cast<const char*>(p), n);
                w = co_await s->c.async_write(string(std::string_view(one)));
            } else {
                vector<slice<const byte>> parts;
                parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(header), h));
                // the scratch is the connection's, under the write lock to the write's end; the
                // program's payload is held by its slice
                parts.push_back(p == reinterpret_cast<const uint8_t*>(payload.data()) ? payload : slice<const byte>(reinterpret_cast<const byte*>(p), n));
                auto st = net::detail::ConnectionAccess::impl(s->c).start_write_parts(parts);
                w = std::move(st.done);
                if (st.rest) {
                    w = co_await std::move(*st.rest);
                }
            }
            if (!w) {
                s->fail(io::error(w.error().code(), "websocket", "write"));
                co_return io::detail::fail(s->ended());
            }
            co_return expected<void, io::error>();
        }

        // More bytes into the read buffer: false at the end or an error
        // (the failure kept)
        inline async::task<bool> ws_fill(tracked_ptr<WsImpl> s) noexcept {
            if (s->at) {
                size_t n = s->end - s->at;
                if (n) {
                    sgcl::detail::move_bytes(s->in.data(), s->in.data() + s->at, n);
                }
                s->at = 0;
                s->end = n;
            }
            auto r = co_await s->c.async_read(s->in.as_slice().subslice(s->end, s->in.size() - s->end));
            if (!r || *r == 0) {
                if (s->close_sent.load() && s->tcp_closed.load()) {
                    co_return false;
                }
                s->fail(r ? io::error(io::errc::unexpected_eof, "websocket", "read") : io::error(r.error().code(), "websocket", "read"));
                co_return false;
            }
            s->end += *r;
            co_return true;
        }

        // The connection failed by this side (§7.1.7): a close frame with
        // the code, the connection closed, the error kept
        inline async::task<io::error> ws_fail(tracked_ptr<WsImpl> s, uint16_t code, io::error e) noexcept {
            uint8_t payload[2] = {uint8_t(code >> 8), uint8_t(code)};
            (void)co_await ws_write_frame(s, WsOpcode::close, slice<const byte>(reinterpret_cast<const byte*>(payload), 2), false);
            s->fail(e);
            co_return s->ended();
        }

        // n payload bytes into dst: what the buffer holds, then straight
        // from the connection
        inline async::task<bool> ws_payload(tracked_ptr<WsImpl> s, slice<byte> dst) noexcept {
            size_t take = std::min(dst.size(), s->end - s->at);
            if (take) {
                sgcl::detail::copy_bytes(dst.data(), s->in.data() + s->at, take);
                s->at += take;
            }
            if (take < dst.size()) {
                auto r = co_await s->c.async_read_full(dst.subslice(take, dst.size() - take));
                if (!r || *r == 0) {
                    s->fail(r ? io::error(io::errc::unexpected_eof, "websocket", "read") : io::error(r.error().code(), "websocket", "read"));
                    co_return false;
                }
            }
            co_return true;
        }

        // A message: frames read, control frames answered on the way (a
        // ping by its pong, a close by its close), data frames put together
        inline async::task<expected<pair<bool, vector<byte>>, io::error>> ws_receive(tracked_ptr<WsImpl> s) noexcept {
            auto guard = co_await s->read_lock.scoped_lock();
            for (;;) {
                if (s->tcp_closed.load() && s->at == s->end) {
                    co_return io::detail::fail(s->ended());
                }
                WsFrame f;
                const char* why = "";
                auto st = parse_ws_frame(reinterpret_cast<const uint8_t*>(s->in.data()) + s->at, s->end - s->at, !s->client, (bool)s->deflate, f, why);
                if (st == WsParse::more) {
                    if (!co_await ws_fill(s)) {
                        co_return io::detail::fail(s->ended());
                    }
                    continue;
                }
                if (st == WsParse::invalid) {
                    co_return io::detail::fail(co_await ws_fail(s, 1002, ws_error(net::errc::websocket_protocol, why)));
                }
                s->at += f.header;
                s->heard();
                if (f.control()) {
                    while (s->end - s->at < f.length) {
                        if (!co_await ws_fill(s)) {
                            co_return io::detail::fail(s->ended());
                        }
                    }
                    uint8_t payload[125];
                    sgcl::detail::copy_bytes(payload, s->in.data() + s->at, size_t(f.length));
                    s->at += size_t(f.length);
                    if (f.masked) {
                        ws_mask(payload, size_t(f.length), f.mask, 0);
                    }
                    if (f.opcode == WsOpcode::ping) {
                        if (!s->close_sent.load()) {
                            (void)co_await ws_write_frame(s, WsOpcode::pong, slice<const byte>(reinterpret_cast<const byte*>(payload), size_t(f.length)), false);
                        }
                        continue;
                    }
                    if (f.opcode == WsOpcode::pong) {
                        continue;
                    }
                    // close (§5.5.1): its code kept, echoed, the connection closed
                    uint16_t code = 0;
                    std::string reason;
                    if (!parse_ws_close(payload, size_t(f.length), code, reason)) {
                        bool utf8 = f.length >= 2 && sendable_close_code(uint16_t((payload[0] << 8) | payload[1]));
                        co_return io::detail::fail(co_await ws_fail(s, utf8 ? 1007 : 1002, ws_error(net::errc::websocket_protocol, "an invalid close frame")));
                    }
                    {
                        std::lock_guard g(s->end_lock);
                        s->close_status = code;
                        s->close_reason = reason;
                        if (!s->failure) {
                            s->failure = net::detail::net_error(net::errc::websocket_closed, "websocket",
                                                                 string(std::to_string(code) + (reason.empty() ? std::string() : " " + reason)));
                        }
                    }
                    s->close_received.store(true);
                    if (!s->close_sent.load()) {
                        uint8_t echo[2] = {uint8_t(code >> 8), uint8_t(code)};
                        (void)co_await ws_write_frame(s, WsOpcode::close, slice<const byte>(reinterpret_cast<const byte*>(echo), code == 1005 ? 0 : 2), false);
                    }
                    s->close_tcp();
                    co_return io::detail::fail(s->ended());
                }
                // a data frame (§5.4): a start only when none is under way,
                // a continuation only when one is
                if (f.opcode == WsOpcode::continuation) {
                    if (!s->assembling) {
                        co_return io::detail::fail(co_await ws_fail(s, 1002, ws_error(net::errc::websocket_protocol, "a continuation with no message")));
                    }
                    if (f.rsv1) {
                        co_return io::detail::fail(co_await ws_fail(s, 1002, ws_error(net::errc::websocket_protocol, "a continuation compressed")));
                    }
                } else {
                    if (s->assembling) {
                        co_return io::detail::fail(co_await ws_fail(s, 1002, ws_error(net::errc::websocket_protocol, "a message inside another")));
                    }
                    s->assembling = true;
                    s->assembling_binary = f.opcode == WsOpcode::binary;
                    s->assembling_compressed = f.rsv1;
                    s->message = vector<byte>();
                    s->utf8.reset();
                }
                const size_t have = s->message.size();
                if (f.length > s->max_message || have + f.length > s->max_message) {
                    co_return io::detail::fail(co_await ws_fail(s, 1009, net::detail::net_error(net::errc::body_too_large, "websocket", "message")));
                }
                if (f.length) {
                    VectorOverwrite::resize(s->message, have + size_t(f.length));
                    if (!co_await ws_payload(s, s->message.as_slice().subslice(have, size_t(f.length)))) {
                        co_return io::detail::fail(s->ended());
                    }
                    uint8_t* p = reinterpret_cast<uint8_t*>(s->message.data()) + have;
                    if (f.masked) {
                        ws_mask(p, size_t(f.length), f.mask, 0);
                    }
                    if (!s->assembling_binary && !s->assembling_compressed && !s->utf8.feed(p, size_t(f.length))) {
                        co_return io::detail::fail(co_await ws_fail(s, 1007, ws_error(net::errc::websocket_protocol, "a text message not UTF-8")));
                    }
                }
                if (!f.fin) {
                    continue;
                }
                s->assembling = false;
                vector<byte> m = std::move(s->message);
                s->message = vector<byte>();
                if (s->assembling_compressed) {
                    vector<byte> plain;
                    bool too_large = false;
                    if (!s->deflate->decompress(reinterpret_cast<const uint8_t*>(m.data()), m.size(), plain, s->max_message, too_large)) {
                        if (too_large) {
                            co_return io::detail::fail(co_await ws_fail(s, 1009, net::detail::net_error(net::errc::body_too_large, "websocket", "message")));
                        }
                        co_return io::detail::fail(co_await ws_fail(s, 1007, ws_error(net::errc::websocket_protocol, "a message not DEFLATE")));
                    }
                    m = std::move(plain);
                    if (!s->assembling_binary && !valid_utf8(reinterpret_cast<const uint8_t*>(m.data()), m.size())) {
                        co_return io::detail::fail(co_await ws_fail(s, 1007, ws_error(net::errc::websocket_protocol, "a text message not UTF-8")));
                    }
                } else if (!s->assembling_binary && !s->utf8.complete()) {
                    co_return io::detail::fail(co_await ws_fail(s, 1007, ws_error(net::errc::websocket_protocol, "a text message not UTF-8")));
                }
                co_return pair<bool, vector<byte>>(s->assembling_binary, std::move(m));
            }
        }

        // The close handshake of this side (§7.1.2): the close frame, then
        // the peer's awaited (read here when no receive is under way,
        // else by that receive) for at most WsCloseWait, then the
        // connection closed
        inline async::task<expected<void, io::error>> ws_close(tracked_ptr<WsImpl> s, uint16_t code, string reason) noexcept {
            if (s->tcp_closed.load()) {
                co_return expected<void, io::error>();
            }
            std::string payload;
            if (code != 1005) {
                payload += char(code >> 8);
                payload += char(code & 0xFF);
                payload += reason.view();
            }
            {
                std::lock_guard g(s->end_lock);
                if (!s->failure) {
                    s->failure = io::error(io::errc::closed, "websocket", "connection");
                }
            }
            auto sent = co_await ws_write_frame(s, WsOpcode::close, slice<const byte>(reinterpret_cast<const byte*>(payload.data()), payload.size()), false);
            if (!sent || s->close_received.load()) {
                s->close_tcp();
                co_return expected<void, io::error>();
            }
            const time_point deadline = sgcl::clock::now() + WsCloseWait;
            if (s->read_lock.try_lock()) {
                async::mutex::guard g(s->read_lock);
                s->c.set_read_deadline(deadline);
                // the peer's close awaited, what comes before it dropped
                for (;;) {
                    WsFrame f;
                    const char* why = "";
                    auto st = parse_ws_frame(reinterpret_cast<const uint8_t*>(s->in.data()) + s->at, s->end - s->at, !s->client, (bool)s->deflate, f, why);
                    if (st == WsParse::invalid) {
                        break;
                    }
                    if (st == WsParse::more) {
                        if (!co_await ws_fill(s)) {
                            break;
                        }
                        continue;
                    }
                    s->at += f.header;
                    if (f.opcode != WsOpcode::close) {
                        // dropped as it comes, the buffer's bytes first
                        uint64_t left = f.length;
                        size_t here = size_t(std::min<uint64_t>(left, s->end - s->at));
                        s->at += here;
                        left -= here;
                        bool ok = true;
                        while (left && ok) {
                            auto r = co_await s->c.async_read(s->in.as_slice().subslice(0, size_t(std::min<uint64_t>(left, s->in.size()))));
                            ok = r && *r;
                            left -= ok ? *r : 0;
                        }
                        s->at = s->end = left ? 0 : s->at;
                        if (!ok) {
                            break;
                        }
                        continue;
                    }
                    while (s->end - s->at < f.length) {
                        if (!co_await ws_fill(s)) {
                            break;
                        }
                    }
                    if (s->end - s->at >= f.length) {
                        uint8_t body[125];
                        sgcl::detail::copy_bytes(body, s->in.data() + s->at, size_t(f.length));
                        if (f.masked) {
                            ws_mask(body, size_t(f.length), f.mask, 0);
                        }
                        uint16_t got = 0;
                        std::string why_text;
                        if (parse_ws_close(body, size_t(f.length), got, why_text)) {
                            std::lock_guard eg(s->end_lock);
                            s->close_status = got;
                            s->close_reason = why_text;
                        }
                        s->close_received.store(true);
                    }
                    break;
                }
                s->close_tcp();
                co_return expected<void, io::error>();
            }
            // a receive under way reads the peer's close and closes
            co_await sgcl::async::select(s->done.on_receive([] {}), sgcl::async::timeout(deadline, [] {}));
            s->close_tcp();
            co_return expected<void, io::error>();
        }

        // Keep-alive: a ping each interval; nothing heard for two of them
        // and the connection is closed (ETIMEDOUT for what reads it)
        inline async::task<void> ws_keep_alive(tracked_ptr<WsImpl> s, duration interval) noexcept {
            for (;;) {
                bool ended = false;
                co_await sgcl::async::select(s->done.on_receive([&] { ended = true; }), sgcl::async::timeout(sgcl::clock::now() + interval, [] {}));
                if (ended || s->tcp_closed.load()) {
                    co_return;
                }
                const int64_t silent = sgcl::clock::now().time_since_epoch().count() - s->last_heard.load(std::memory_order_relaxed);
                if (silent > 2 * std::chrono::nanoseconds(interval).count()) {
                    s->fail(io::error(error_code(ETIMEDOUT, std::system_category()), "websocket", "keep-alive"));
                    co_return;
                }
                if (!s->close_sent.load()) {
                    (void)co_await ws_write_frame(s, WsOpcode::ping, slice<const byte>(), false);
                }
            }
        }

        // The stop: the connection closed with 1001 (going away)
        inline async::task<void> ws_watch_stop(tracked_ptr<WsImpl> s, async::stop_token stop) noexcept {
            bool stopped = false;
            co_await sgcl::async::select(s->done.on_receive([] {}), stop.on_stop([&] { stopped = true; }));
            if (stopped && !s->tcp_closed.load()) {
                uint8_t payload[2] = {uint8_t(1001 >> 8), uint8_t(1001 & 0xFF)};
                (void)co_await ws_write_frame(s, WsOpcode::close, slice<const byte>(reinterpret_cast<const byte*>(payload), 2), false);
                s->fail(io::error(error_code(ECANCELED, std::system_category()), "websocket", "stop"));
            }
        }

        struct WebSocketAccess;
    }

    // A WebSocket connection (RFC 6455): messages of text or bytes both
    // ways over one connection, made by websocket::connect (a client) or
    // websocket::accept (a server's handler). A handle of one word: copies
    // are the same connection. One receive at a time and any number of
    // sends (each message one frame, written whole); a ping is answered by
    // receive on its way, and so is the peer's close, after which every
    // call is the error net::errc::websocket_closed with the peer's code
    // in close_status(). A text message is checked to be UTF-8; a message
    // past options::max_message_bytes ends the connection (close 1009).
    class websocket {
    public:
        // The settings of a connection
        struct options {
            vector<string> subprotocols;            // offered by a client, in order; taken by a server, its preference first
            http::headers headers;                  // a client's request fields (Origin, Authorization, Cookie), a server's 101 fields
            vector<string> origins;                 // a server's: the Origin hosts taken beside the request's own; "*" every one
            size_t max_message_bytes = size_t(32) << 20;
            duration ping_interval = duration::zero();    // keep-alive pings, nothing heard for two closing it; zero: none
            duration handshake_timeout = 30 * second;      // a client's dial and handshake
            bool compression = false;               // permessage-deflate offered (a client) or taken (a server)
            async::stop_token stop;                 // closes the connection (1001) when stopped
        };

        // A message received: text (UTF-8, checked) or bytes
        struct message {
            bool binary = false;
            vector<byte> data;

            SGCL_INLINE_HOT string text() const noexcept {
                return string(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
            }
        };

        // No connection; an operation on it is a contract violation
        websocket() noexcept = default;

        // A client's connection to url, ws:// or wss:// (http:// and
        // https:// taken as the same), through a client of the process's
        // default settings (http::download's): the environment's proxy, the
        // system's roots. client::websocket goes through a client's own
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<websocket, io::error> connect(const string& url);
        static expected<websocket, io::error> connect(const string& url, const options& o);
        static async::task<expected<websocket, io::error>> async_connect(string url) noexcept;
        static async::task<expected<websocket, io::error>> async_connect(string url, options o) noexcept;

        // A server's: the request's upgrade answered with 101 and the
        // connection taken over from the server, after the checks of
        // §4.2.1, the origin and the subprotocol; a request refused is
        // answered (400, 403, 405, 426) and the error returned
        // `accept(...)` on this thread, `co_await async_accept(...)` in a task
        static expected<websocket, io::error> accept(const request& r, const response_writer& w);
        static expected<websocket, io::error> accept(const request& r, const response_writer& w, const options& o);
        static async::task<expected<websocket, io::error>> async_accept(request r, response_writer w) noexcept;
        static async::task<expected<websocket, io::error>> async_accept(request r, response_writer w, options o) noexcept;

        // A message of text
        // `send(...)` on this thread, `co_await async_send(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> send(const string& text) const {
            return async_send(text).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send(const string& text) const noexcept {
            return _co_send(_s, text, detail::WsOpcode::text);
        }

        // A message of bytes
        // `send(...)` on this thread, `co_await async_send(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> send(const slice<const byte>& data) const {
            return async_send(data).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send(const slice<const byte>& data) const noexcept {
            return detail::ws_write_frame(_s, detail::WsOpcode::binary, data, true);
        }

        // A literal, a character array, a std::string_view: text
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT expected<void, io::error> send(const T& text) const {
            return send(string(slice<const byte>(text)));
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send(const T& text) const noexcept {
            return async_send(string(slice<const byte>(text)));
        }

        // The next message, its frames put together
        // `receive(...)` on this thread, `co_await async_receive(...)` in a task
        SGCL_INLINE_HOT expected<message, io::error> receive() const {
            return async_receive().wait();
        }

        SGCL_INLINE_HOT async::task<expected<message, io::error>> async_receive() const noexcept {
            return _co_receive(_s);
        }

        // A ping, with up to 125 bytes; its pong is read by receive
        // `ping(...)` on this thread, `co_await async_ping(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> ping(const slice<const byte>& data = {}) const {
            return async_ping(data).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_ping(const slice<const byte>& data = {}) const noexcept {
            if (data.size() > 125) {
                return _co_error(io::error(std::make_error_code(std::errc::invalid_argument), "websocket", "a ping of more than 125 bytes"));
            }
            return detail::ws_write_frame(_s, detail::WsOpcode::ping, data, false);
        }

        // The close handshake (§7.1.2): a close frame of the code (1000,
        // normal, by default) and reason, the peer's awaited (5 s at most),
        // the connection closed. A code that may not be sent or a reason
        // past 123 bytes or not UTF-8 is EINVAL, and nothing is sent
        // `close(...)` on this thread, `co_await async_close(...)` in a task
        SGCL_INLINE_HOT expected<void, io::error> close() const {
            return async_close().wait();
        }

        SGCL_INLINE_HOT expected<void, io::error> close(uint16_t code, const string& reason) const {
            return async_close(code, reason).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close() const noexcept {
            return detail::ws_close(_s, 1000, string());
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close(uint16_t code, const string& reason) const noexcept {
            if (!detail::sendable_close_code(code) || reason.size() > 123 || !detail::valid_utf8(reinterpret_cast<const uint8_t*>(reason.data()), reason.size())) {
                return _co_error(io::error(std::make_error_code(std::errc::invalid_argument), "websocket", "close"));
            }
            return detail::ws_close(_s, code, reason);
        }

        // The subprotocol the server chose; empty for none
        SGCL_INLINE_HOT string subprotocol() const noexcept {
            return _s->subprotocol;
        }

        // Whether permessage-deflate was agreed
        SGCL_INLINE_HOT bool compression() const noexcept {
            return (bool)_s->deflate;
        }

        // The code of the peer's close frame, 1005 for one without; 0 while
        // none has come
        SGCL_INLINE_HOT uint16_t close_status() const noexcept {
            std::lock_guard g(_s->end_lock);
            return _s->close_status;
        }

        SGCL_INLINE_HOT string close_reason() const noexcept {
            std::lock_guard g(_s->end_lock);
            return string(std::string_view(_s->close_reason));
        }

        // Whether the connection under it is closed
        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _s->tcp_closed.load();
        }

        // The connection under it: the endpoints, the deadlines
        SGCL_INLINE_HOT net::connection connection() const noexcept {
            return _s->c;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

        SGCL_INLINE_HOT friend bool operator==(const websocket& a, const websocket& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::WebSocketAccess;

        SGCL_INLINE_HOT explicit websocket(const tracked_ptr<detail::WsImpl>& s) noexcept
        : _s(s) {
        }

        static async::task<expected<void, io::error>> _co_send(tracked_ptr<detail::WsImpl> s, string text, detail::WsOpcode op) noexcept {
            co_return co_await detail::ws_write_frame(s, op, as_bytes(text.as_slice()), true);
        }

        static async::task<expected<message, io::error>> _co_receive(tracked_ptr<detail::WsImpl> s) noexcept {
            auto r = co_await detail::ws_receive(s);
            if (!r) {
                co_return unexpected(r.error());
            }
            message m;
            m.binary = r->first;
            m.data = std::move(r->second);
            co_return m;
        }

        static async::task<expected<void, io::error>> _co_error(io::error e) noexcept {
            co_return unexpected(e);
        }

        tracked_ptr<detail::WsImpl> _s;
    };

    namespace detail {
        struct WebSocketAccess {
            // A connection after its handshake: the bytes read past it
            // first, the watchers started
            static websocket make(const net::connection& c, bool client, std::string_view rest, const websocket::options& o, string subprotocol,
                                  const optional<WsDeflateParams>& deflate) noexcept {
                tracked_ptr s = make_tracked<WsImpl>();
                s->c = c;
                s->client = client;
                s->max_message = o.max_message_bytes;
                s->subprotocol = subprotocol;
                if (deflate) {
                    // a client's compressor resets when client_no_context_takeover, a server's when server_...
                    bool out_reset = client ? deflate->client_no_context_takeover : deflate->server_no_context_takeover;
                    bool in_reset = client ? deflate->server_no_context_takeover : deflate->client_no_context_takeover;
                    s->deflate = std::make_unique<WsDeflate>(out_reset, in_reset);
                }
                s->in = vector<byte>(std::max(WsReadBuffer, rest.size()));
                if (!rest.empty()) {
                    sgcl::detail::copy_bytes(s->in.data(), rest.data(), rest.size());
                }
                s->end = rest.size();
                s->heard();
                if (o.ping_interval > duration::zero()) {
                    async::go(ws_keep_alive(s, o.ping_interval));
                }
                if (o.stop.stop_possible()) {
                    async::go(ws_watch_stop(s, o.stop));
                }
                return websocket(s);
            }
        };

        inline async::task<expected<websocket, io::error>> ws_upgrade(request r, response_writer w, websocket::options o) noexcept {
            auto& impl = *RequestAccess::impl(r);
            auto refuse = [&](int status, const char* why) {
                if (status == 426) {
                    w.set_header("Sec-WebSocket-Version", "13");
                }
                w.error(status);
                return net::detail::net_error(net::errc::websocket_handshake, "websocket", string(why));
            };
            if (impl.h2) {
                // RFC 8441 (WebSocket over HTTP/2) is not served
                co_return unexpected(refuse(status::http_version_not_supported, "an HTTP/2 request (RFC 8441 is not served)"));
            }
            const char* why = "";
            if (int status = check_ws_request(impl.method.view(), impl.minor, impl.fields, why)) {
                co_return unexpected(refuse(status, why));
            }
            std::vector<std::string> allowed;
            for (auto& a : o.origins) {
                allowed.emplace_back(a.view());
            }
            if (!ws_origin_allowed(impl.fields, allowed)) {
                co_return unexpected(refuse(status::forbidden, "an Origin not allowed"));
            }
            string protocol;
            auto offered = ws_tokens(impl.fields, "sec-websocket-protocol");
            for (auto& mine : o.subprotocols) {
                for (auto& theirs : offered) {
                    if (protocol.empty() && mine.view() == theirs) {
                        protocol = mine;
                    }
                }
            }
            optional<WsDeflateParams> deflate;
            if (o.compression) {
                deflate = choose_ws_deflate(ws_tokens(impl.fields, "sec-websocket-extensions"));
            }
            if (auto e = invalid_field(o.headers)) {
                co_return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "websocket", *e));
            }
            std::string rest;
            auto taken = WriterAccess::take_over(w, rest);
            if (!taken) {
                co_return unexpected(taken.error());
            }
            net::connection c = *taken;
            std::string head = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
            head += ws_accept(trim_ows(*HeadersAccess::find(impl.fields, "sec-websocket-key"))).view();
            head += "\r\n";
            if (!protocol.empty()) {
                head += "Sec-WebSocket-Protocol: ";
                head += protocol.view();
                head += "\r\n";
            }
            if (deflate) {
                head += "Sec-WebSocket-Extensions: ";
                head += ws_deflate_answer(*deflate);
                head += "\r\n";
            }
            for (auto& f : HeadersAccess::fields(o.headers)) {
                head += f.first.view();
                head += ": ";
                head += f.second.view();
                head += "\r\n";
            }
            head += "\r\n";
            c.set_deadline(time_point());
            auto sent = co_await c.async_write(string(std::string_view(head)));
            if (!sent) {
                (void)c.close();
                co_return unexpected(io::error(sent.error().code(), "websocket", "101"));
            }
            co_return WebSocketAccess::make(c, false, rest, o, protocol, deflate);
        }

    }

    inline expected<websocket, io::error> websocket::accept(const request& r, const response_writer& w) {
        return detail::ws_upgrade(r, w, options()).wait();
    }

    inline expected<websocket, io::error> websocket::accept(const request& r, const response_writer& w, const options& o) {
        return detail::ws_upgrade(r, w, o).wait();
    }

    inline async::task<expected<websocket, io::error>> websocket::async_accept(request r, response_writer w) noexcept {
        return detail::ws_upgrade(std::move(r), std::move(w), options());
    }

    inline async::task<expected<websocket, io::error>> websocket::async_accept(request r, response_writer w, options o) noexcept {
        return detail::ws_upgrade(std::move(r), std::move(w), std::move(o));
    }

}
