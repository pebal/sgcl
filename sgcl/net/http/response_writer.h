//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "headers.h"
#include "status.h"
#include "detail/chunks.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <climits>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace sgcl::net::http {
    namespace detail {
        // "Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n" of now, appended to a
        // head (RFC 9110 §6.6.1: a server with a clock sends one); the
        // time is time::now()'s, so a test's manual clock moves it.
        //
        // The line is made once a second, by the first to see the second
        // change, and kept in a sequence lock: a counter, odd while the
        // line is written, and the line in words of its own. A reader takes
        // the counter (acquire), the second and the words (relaxed), a
        // fence (acquire), and the counter again: the same even count, the
        // words are one line whole, never half of one second's and half of
        // the next's. The writer takes the counter odd with a compare-
        // exchange (one writer; another that finds it odd makes its own
        // line this once), fences (release), stores, and makes it even with
        // a release. No lock and no allocation on a response: a mutex and
        // a string copied out were 120 samples of a 24-worker server's
        // profile (measured). A buffer on the managed heap swapped in an
        // atomic word would cost a hazard pointer per read and an
        // allocation per second; the words cost five relaxed loads.
        struct DateCache {
            static constexpr size_t Words = 5;   // 40 bytes: the line is 37
            std::atomic<uint64_t> sequence = {0};
            std::atomic<int64_t> second = {INT64_MIN};
            std::atomic<uint32_t> size = {0};
            std::atomic<uint64_t> words[Words] = {};
        };

        // Called by the writer of the line half way through its stores,
        // when set: a test's way to hold a reader over a half-written line
        inline std::atomic<void (*)()> date_line_test_hook = {nullptr};

        inline DateCache& date_cache() noexcept {
            static DateCache cache;
            return cache;
        }

        // The line of a moment, made: its bytes and their count
        inline size_t make_date_line(const time::datetime& now, char (&out)[DateCache::Words * 8]) {
            auto text = now.format(time::http);
            auto v = text.view();
            size_t n = 0;
            for (char c : std::string_view("Date: ")) {
                out[n++] = c;
            }
            for (size_t i = 0; i < v.size() && n < sizeof(out) - 2; ++i) {
                out[n++] = v[i];
            }
            out[n++] = '\r';
            out[n++] = '\n';
            return n;
        }

        inline void append_date_line(std::string& head) {
            auto now = time::now();
            int64_t second = now.unix();
            auto& c = date_cache();
            uint64_t s = c.sequence.load(std::memory_order_acquire);
            if (!(s & 1) && c.second.load(std::memory_order_relaxed) == second) {
                uint64_t w[DateCache::Words];
                for (size_t i = 0; i < DateCache::Words; ++i) {
                    w[i] = c.words[i].load(std::memory_order_relaxed);
                }
                auto n = c.size.load(std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_acquire);   // the loads above before the count's second look
                if (c.sequence.load(std::memory_order_relaxed) == s && n <= sizeof(w)) {
                    head.append(reinterpret_cast<const char*>(w), n);
                    return;
                }
            }
            char line[DateCache::Words * 8] = {};
            size_t n = make_date_line(now, line);
            head.append(line, n);
            if (!(s & 1) && c.sequence.compare_exchange_strong(s, s + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                std::atomic_thread_fence(std::memory_order_release);   // the odd count before the stores below, for a reader's second look
                uint64_t w[DateCache::Words];
                std::memcpy(w, line, sizeof(w));
                c.second.store(second, std::memory_order_relaxed);   // any order: the count makes the stores one
                c.size.store(uint32_t(n), std::memory_order_relaxed);
                for (size_t i = 0; i < DateCache::Words; ++i) {
                    c.words[i].store(w[i], std::memory_order_relaxed);
                    if (i == DateCache::Words / 2) {
                        if (auto hook = date_line_test_hook.load(std::memory_order_relaxed)) [[unlikely]] {
                            hook();
                        }
                    }
                }
                c.sequence.store(s + 2, std::memory_order_release);
            }
        }

        // The fields of an HTTP/2 response as HPACK encodes them (RFC 9113
        // §8.3.2, §8.2.2): :status first, the names in lower case, the
        // fields of a connection (Connection, Keep-Alive, Proxy-Connection,
        // Transfer-Encoding, Upgrade) left out, the length the server's,
        // Date added when the handler gave none. Made on the stack of the
        // send, encoded under the connection's lock
        struct ResponseFields final : h2::FieldBlock {
            int status;
            const http::headers& fields;
            optional<uint64_t> length;

            ResponseFields(int status, const http::headers& fields, optional<uint64_t> length)
            : status(status), fields(fields), length(length) {
            }

            static bool connection_field(std::string_view n) {
                return iequal(n, "connection") || iequal(n, "keep-alive") || iequal(n, "proxy-connection") || iequal(n, "transfer-encoding") ||
                       iequal(n, "upgrade") || iequal(n, "content-length");
            }

            void encode(h2::Encoder& e, std::string& out) const override {
                char code[4] = {char('0' + status / 100 % 10), char('0' + status / 10 % 10), char('0' + status % 10), 0};
                e.encode(out, ":status", std::string_view(code, 3));
                thread_local std::string lower;   // scratch: the names' lower case, kept with its capacity
                bool dated = false;
                for (auto& f : HeadersAccess::fields(fields)) {
                    auto n = f.first.view();
                    if (connection_field(n)) {
                        continue;
                    }
                    lower.assign(n);
                    for (auto& c : lower) {
                        c = (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
                    }
                    dated = dated || lower == "date";
                    e.encode(out, lower, f.second.view());
                }
                if (length) {
                    char digits[24];
                    int k = std::snprintf(digits, sizeof(digits), "%llu", (unsigned long long)*length);
                    e.encode(out, "content-length", std::string_view(digits, size_t(k)));
                }
                if (!dated) {
                    thread_local std::string line;
                    line.clear();
                    append_date_line(line);   // "Date: ...\r\n"
                    if (line.size() > 8) {
                        e.encode(out, "date", std::string_view(line).substr(6, line.size() - 8));
                    }
                }
            }
        };

        // The server's side of one exchange: the response as the handler
        // builds it, and how it goes out (over the wire of HTTP/1.1, or on
        // an HTTP/2 stream when h2 is set)
        struct WriterImpl {
            tracked_ptr<Wire> wire;
            tracked_ptr<h2::StreamState> h2;
            int status = 200;
            http::headers fields;
            BodyBuffer body;                  // written, not yet sent: 64 B in place, then managed blocks (DESIGN 277)
            bool head_request = false;
            int request_minor = 1;
            bool keep_alive_asked = false;    // an HTTP/1.0 request's "Connection: keep-alive"
            bool close_after = false;         // the connection ends with this response
            bool head_sent = false;
            bool chunked = false;             // after a flush: the body goes chunked
            bool until_close = false;         // after a flush to HTTP/1.0: the body ends with the connection
            optional<uint64_t> declared;      // the handler's Content-Length, after a flush
            uint64_t sent = 0;                // body bytes sent after a flush
            bool touched = false;             // a status set or a byte written
            bool hijacked = false;
            optional<io::error> failed;       // a write that failed

            static bool bodiless(int status) noexcept {
                return (status >= 100 && status < 200) || status == 204 || status == 304;
            }

            // The head, with the framing decided, at the end of `h` (the
            // wire's buffer): `length` for a whole body known now, nullopt
            // for a flush (chunked, the handler's length, or to the close)
            void head_to(std::string& h, optional<uint64_t> length) {
                if (h.capacity() < 256) {
                    h.reserve(256);   // once a connection: the buffer is kept
                }
                h += "HTTP/1.1 ";
                h += std::to_string(status);
                h += ' ';
                h += reason(status);
                h += "\r\n";
                bool close = close_after;
                for (auto& f : HeadersAccess::fields(fields)) {
                    auto n = f.first.view();
                    // the framing is the server's: the handler's length is
                    // honoured through `declared`, its Connection: close by
                    // ending the connection
                    if (iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "connection")) {
                        if (iequal(n, "connection") && HeadersAccess::has_token(fields, "connection", "close")) {
                            close = true;
                        }
                        continue;
                    }
                    h += n;
                    h += ": ";
                    h += f.second.view();
                    h += "\r\n";
                }
                if (!HeadersAccess::count(fields, "date")) {
                    append_date_line(h);
                }
                if (!bodiless(status)) {
                    if (length) {
                        h += "Content-Length: ";
                        h += std::to_string(*length);
                        h += "\r\n";
                    } else if (declared) {
                        h += "Content-Length: ";
                        h += std::to_string(*declared);
                        h += "\r\n";
                    } else if (request_minor >= 1) {
                        chunked = true;
                        h += "Transfer-Encoding: chunked\r\n";
                    } else {
                        until_close = true;
                        close = true;
                    }
                }
                if (close) {
                    close_after = true;
                    h += "Connection: close\r\n";
                } else if (request_minor == 0 && keep_alive_asked) {
                    h += "Connection: keep-alive\r\n";
                }
                h += "\r\n";
            }

            // The handler's Content-Length, when it gave one that is a number
            optional<uint64_t> handler_length() const {
                optional<uint64_t> cl;
                if (!HeadersAccess::count(fields, "content-length") || !content_length(fields, cl)) {
                    return nullopt;
                }
                return cl;
            }

            async::task<expected<void, io::error>> send() {
                expected<void, io::error> now;
                if (auto rest = send_start(now)) {
                    co_return co_await *rest;
                }
                co_return now;
            }

            // The wire's buffer (wire->out) sent, begun without a frame
            // (net: ConnImpl::start_write): the buffer held by the wire
            // while it goes, the result in `now` when the connection took
            // it at once or failed, a task for the rest only when it would
            // wait
            optional<async::task<expected<void, io::error>>> send_start(expected<void, io::error>& now) {
                if (failed) {
                    now = io::detail::fail(*failed);
                    return nullopt;
                }
                const std::string& out = wire->out;
                if (out.empty()) {
                    now = expected<void, io::error>();
                    return nullopt;
                }
                slice<const byte> data(reinterpret_cast<const byte*>(out.data()), out.size());
                auto s = net::detail::ConnectionAccess::impl(wire->connection()).start_write(data);
                if (s.rest) {
                    return _sent(tracked_ptr<WriterImpl>(this), std::move(*s.rest));
                }
                now = sent_result(s.done);
                return nullopt;
            }

            expected<void, io::error> sent_result(const expected<size_t, io::error>& r) {
                if (!r) {
                    failed = r.error();
                    close_after = true;
                    return io::detail::fail(r);
                }
                return expected<void, io::error>();
            }

            // The rest of a send that would wait; the writer (and its bytes)
            // held by the task
            static async::task<expected<void, io::error>> _sent(tracked_ptr<WriterImpl> self, async::task<expected<size_t, io::error>> rest) {
                auto r = co_await rest;
                co_return self->sent_result(r);
            }

            // The body written so far, in the framing of a flushed response,
            // at the end of `out`
            // (HTTP/1.1: the body copied into the wire's buffer after the
            // head, one send for both, as before the blocks; a gathered
            // write would need a writev in ConnImpl, and TLS copies into its
            // records anyway)
            void framed_to(std::string& out, const BodyBuffer& data) {
                if (head_request || bodiless(status) || data.empty()) {
                    return;
                }
                const size_t before = out.size();
                if (chunked) {
                    char size[20];
                    int n = std::snprintf(size, sizeof(size), "%zx\r\n", data.size());
                    out.append(size, size_t(n));
                    data.copy_to(out);
                    out += "\r\n";
                } else if (declared) {
                    uint64_t room = *declared > sent ? *declared - sent : 0;
                    data.copy_to(out, size_t(std::min<uint64_t>(room, data.size())));
                    if (data.size() > room) {
                        close_after = true;   // more than it said: the rest is dropped
                    }
                } else {
                    data.copy_to(out);
                }
                sent += out.size() - before;
            }

            // The handler's fields checked before a head of them goes out:
            // a name that is not a token or a value with CR, LF, NUL or
            // another control (a user's text splitting the response) is the
            // writer's first error, kept and given back by every flush
            // after it, and nothing of the head is sent
            bool fields_writable() {
                if (auto e = invalid_field(fields)) {
                    if (!failed) {
                        failed = io::error(std::make_error_code(std::errc::invalid_argument), "response", *e);
                    }
                    close_after = true;
                    return false;
                }
                return true;
            }

            async::task<expected<void, io::error>> flush() {
                if (failed) {
                    co_return io::detail::fail(*failed);
                }
                if (h2) {
                    co_return co_await _h2_flush();
                }
                std::string& out = wire->out;
                out.clear();
                if (!head_sent) {
                    if (!fields_writable()) {
                        co_return io::detail::fail(*failed);
                    }
                    set_optional(declared, handler_length());
                    head_to(out, nullopt);
                    head_sent = true;
                }
                framed_to(out, body);
                body.release();   // copied into the wire's buffer: the blocks back to the worker's pool
                co_return co_await send();
            }

            // After the handler: the rest of the response
            async::task<expected<void, io::error>> finish() {
                expected<void, io::error> now;
                if (auto rest = finish_start(now)) {
                    co_return co_await *rest;
                }
                co_return now;
            }

            // finish() begun without a frame: the rest of the response made
            // and sent as far as the connection takes it at once (the
            // result in `now`), a task only for what would wait
            optional<async::task<expected<void, io::error>>> finish_start(expected<void, io::error>& now) {
                if (hijacked) {
                    now = expected<void, io::error>();
                    return nullopt;
                }
                if (h2) {
                    return _h2_finish_start(now);
                }
                std::string& out = wire->out;
                out.clear();
                if (!head_sent) {
                    if (!fields_writable()) {
                        now = io::detail::fail(*failed);
                        return nullopt;
                    }
                    head_to(out, uint64_t(body.size()));
                    head_sent = true;
                    if (!head_request && !bodiless(status)) {
                        body.copy_to(out);
                    }
                } else {
                    framed_to(out, body);
                    if (chunked && !head_request && !bodiless(status)) {
                        out += "0\r\n\r\n";
                    }
                    if (declared && sent != *declared) {
                        close_after = true;
                    }
                }
                body.release();   // copied into the wire's buffer: the blocks back to the worker's pool
                return send_start(now);
            }

            // --- HTTP/2 ---------------------------------------------------------

            expected<void, io::error> _h2_head(optional<uint64_t> length, bool end_stream) {
                ResponseFields block(status, fields, bodiless(status) ? nullopt : length);
                auto r = h2->owner->send_headers(h2->id, block, end_stream);
                head_sent = true;
                if (!r) {
                    failed = r.error();
                }
                return r;
            }

            // The body's bytes as DATA, block by block (each slice owned by
            // its block; the in-place bytes are this frame's copy), END_STREAM
            // with the last when asked; the first `skip` bytes already sent
            async::task<expected<void, io::error>> _h2_send(BodyBuffer data, bool end_stream, size_t skip = 0) {
                size_t left = data.size() - skip;
                expected<void, io::error> r;
                if (left == 0) {
                    r = co_await h2->owner->send_data(h2->id, slice<const byte>(), end_stream);
                } else {
                    // the in-place bytes first, then the blocks; each awaited
                    // here (no frame of a helper's per piece)
                    auto small = data.small();
                    if (!small.empty()) {
                        const size_t cut = std::min(skip, small.size());
                        skip -= cut;
                        if (small.size() > cut) {
                            left -= small.size() - cut;
                            r = co_await h2->owner->send_data(h2->id, slice<const byte>(reinterpret_cast<const byte*>(small.data()) + cut, small.size() - cut), end_stream && left == 0);
                        }
                    }
                    for (tracked_ptr<ByteChunk> c = data.chunks().head(); r && c; c = c->next) {
                        size_t n = c->end - c->begin;
                        const size_t cut = std::min(skip, n);
                        skip -= cut;
                        n -= cut;
                        if (n) {
                            left -= n;
                            r = co_await h2->owner->send_data(h2->id, slice<const byte>(tracked_ptr<const void>(c), c->bytes + c->begin + cut, n), end_stream && left == 0);
                        }
                    }
                }
                data.release();   // every piece queued (the connection copied it): the blocks back to the worker's pool
                if (!r) {
                    failed = r.error();
                }
                co_return r;
            }

            // The head (unless it has gone) and what is buffered in one step
            // of the connection (StreamOwner::send_now: one write, over TLS
            // one record, when the windows take it all); END_STREAM with
            // the last byte when `end`. Whatever the windows did not take is
            // left for _h2_send: the bytes taken in `taken`
            expected<void, io::error> _h2_now(bool end, optional<uint64_t> length, size_t& taken) {
                taken = 0;
                const bool bodyless = head_request || bodiless(status);
                const bool data = !bodyless && !body.empty();
                optional<ResponseFields> block;
                if (!head_sent) {
                    if (!fields_writable()) {
                        return io::detail::fail(*failed);
                    }
                    block.emplace(status, fields, bodiless(status) ? nullopt : length);
                    head_sent = true;
                }
                if (!block && !data && !end) {
                    return expected<void, io::error>();   // a flush of nothing
                }
                auto r = h2->owner->send_now(h2->id, block ? &*block : nullptr, block && !data && end, data ? &body : nullptr, end);
                if (!r) {
                    failed = r.error();
                    return io::detail::fail(r);
                }
                taken = *r;
                if (!data || taken == body.size()) {
                    body.release();   // all of it queued (the connection copied it): the blocks back to the worker's pool
                    taken = SIZE_MAX;
                }
                return expected<void, io::error>();
            }

            // A flush: the head (with the handler's length, if it set one)
            // and what is buffered, as DATA without END_STREAM
            async::task<expected<void, io::error>> _h2_flush() {
                if (!head_sent) {
                    set_optional(declared, handler_length());
                }
                size_t taken = 0;
                if (auto r = _h2_now(false, declared, taken); !r) {
                    co_return r;
                }
                if (taken == SIZE_MAX) {
                    co_return expected<void, io::error>();
                }
                BodyBuffer data = std::move(body);
                body.clear();
                co_return co_await _h2_send(std::move(data), false, taken);
            }

            // After the handler: the head with the length of a body known
            // whole, or the rest after a flush, and END_STREAM; at once
            // when the windows take it (no frame), a task only for the rest
            optional<async::task<expected<void, io::error>>> _h2_finish_start(expected<void, io::error>& now) {
                if (failed) {
                    now = io::detail::fail(*failed);
                    return nullopt;
                }
                size_t taken = 0;
                now = _h2_now(true, uint64_t(body.size()), taken);
                if (!now || taken == SIZE_MAX) {
                    return nullopt;
                }
                BodyBuffer rest = std::move(body);
                body.clear();
                return _h2_send(std::move(rest), true, taken);
            }
        };

        struct WriterAccess;
    }

    // The response a handler writes (Go's http.ResponseWriter). write()
    // does not wait: it adds to a buffer in memory, and the server sends
    // the whole response once the handler returns, with an exact
    // Content-Length, so a handler that never waits is a plain function.
    // Streaming (a large body, server-sent events) is flush(): it sends the
    // head and what is buffered, and the body goes on chunked from there
    // (to an HTTP/1.0 client: to the end of the connection); a
    // Content-Length the handler set before the first flush is kept then.
    // This is simplicity bought with memory: a body built whole is held
    // whole until it is sent.
    //
    // The server writes the framing itself: a Transfer-Encoding or a
    // Content-Length of the handler's is not sent as it stands (a length
    // set before a flush is honoured, and a body that does not match it
    // closes the connection after it). "Connection: close" among the
    // handler's fields ends the connection after the response.
    class response_writer {
    public:
        // 200 unless set; 100 to 199 are not the handler's (invalid_argument),
        // nor anything outside 200 to 999; after the head has gone, ignored
        response_writer& set_status(int code) {
            if (code < 200 || code > 999) {
                throw invalid_argument("http::response_writer: a status is 200 to 999");
            }
            if (!_impl->head_sent) {
                _impl->status = code;
                _impl->touched = true;
            }
            return *this;
        }

        int status() const noexcept {
            return _impl->status;
        }

        response_writer& set_header(const string& name, const string& value) {
            _impl->fields.set(name, value);
            return *this;
        }

        response_writer& add_header(const string& name, const string& value) {
            _impl->fields.add(name, value);
            return *this;
        }

        // A Set-Cookie field
        response_writer& set_cookie(const cookie& c) {
            _impl->fields.add("Set-Cookie", c.to_string());
            return *this;
        }

        // The fields of the response; changed after the head has gone, they
        // go nowhere
        http::headers& headers() const noexcept {
            return _impl->fields;
        }

        response_writer& write(const string& text) {
            _impl->body.append(text.view());
            _impl->touched = true;
            return *this;
        }

        response_writer& write(const slice<const byte>& data) {
            _impl->body.append(data.data(), data.size());
            _impl->touched = true;
            return *this;
        }

        // A literal, a character array, a std::string_view: as a string
        // (an exact match, else the conversions to a string and to bytes tie)
        template<sgcl::detail::TextArgument T>
        response_writer& write(const T& text) {
            return write(slice<const byte>(text));
        }

        // Sends now: the head, if it has not gone, and what is buffered.
        // A handler runs on a worker, so it flushes as a task does, `co_await
        // w.async_flush()`; flush() blocks a thread (a response written from
        // one of the program's threads, never a worker)
        expected<void, io::error> flush() const {
            return _co_flush(_impl).wait();
        }

        async::task<expected<void, io::error>> async_flush() const {
            return _co_flush(_impl);
        }

        // The status with its reason as text/plain ("404 Not Found" gives
        // "Not Found\n"), what was buffered dropped: Go's http.Error
        void error(int code) {
            error(code, reason(code));
        }

        void error(int code, const string& message) {
            set_status(code);
            if (!_impl->head_sent) {
                _impl->body.release();
                _impl->fields.erase("Content-Length");
                _impl->fields.set("Content-Type", "text/plain; charset=utf-8");
                _impl->fields.set("X-Content-Type-Options", "nosniff");
            }
            _impl->body.append(message.view());
            _impl->body.append("\n", 1);
        }

        // A redirect: the status (302 unless given; a 3xx) and Location as given
        void redirect(const string& location, int code = status::found) {
            if (code < 300 || code > 399) {
                throw invalid_argument("http::response_writer: a redirect's status is 3xx");
            }
            set_status(code);
            _impl->fields.set("Location", location);
        }

        // The connection taken over (a WebSocket, a protocol of the
        // program's): the connection and a reader of what is left of it
        // (the bytes the server had read past this request first). The
        // server sends nothing more on it and does not close it. Only
        // before the head has gone: io::errc::closed after.
        expected<pair<net::connection, io::reader>, io::error> hijack() {
            if (_impl->h2) {
                // an HTTP/2 stream is not a connection (Go: no Hijacker in HTTP/2)
                return io::detail::fail(io::error(std::make_error_code(std::errc::operation_not_supported), "hijack", "HTTP/2 response"));
            }
            if (_impl->head_sent || _impl->hijacked) {
                return io::detail::fail(io::error(io::errc::closed, "hijack", "response"));
            }
            _impl->hijacked = true;
            _impl->close_after = true;
            return pair<net::connection, io::reader>(_impl->wire->connection(), io::reader(_impl->wire));
        }

        // Whether the head has gone (a flush was made)
        bool header_sent() const noexcept {
            return _impl->head_sent;
        }

    private:
        friend struct detail::WriterAccess;

        explicit response_writer(const tracked_ptr<detail::WriterImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::WriterImpl> _impl;

        static async::task<expected<void, io::error>> _co_flush(tracked_ptr<detail::WriterImpl> impl) {
            if (impl->hijacked) {
                co_return io::detail::fail(io::error(io::errc::closed, "flush", "response"));
            }
            co_return co_await impl->flush();
        }
    };

    namespace detail {
        struct WriterAccess {
            static response_writer make(const tracked_ptr<WriterImpl>& impl) {
                return response_writer(impl);
            }
        };
    }
}
