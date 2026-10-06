//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

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
        inline size_t make_date_line(const time::datetime& now, char (&out)[DateCache::Words * 8]) noexcept {
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

        inline void append_date_line(std::string& head) noexcept {
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

            SGCL_INLINE_HOT ResponseFields(int status, const http::headers& fields, optional<uint64_t> length) noexcept
            : status(status), fields(fields), length(length) {
            }

            SGCL_INLINE_HOT static bool connection_field(std::string_view n) noexcept {
                return iequal(n, "connection") || iequal(n, "keep-alive") || iequal(n, "proxy-connection") || iequal(n, "transfer-encoding") ||
                       iequal(n, "upgrade") || iequal(n, "content-length");
            }

            void encode(h2::Encoder& e, std::string& out) const noexcept override {
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

        // A field of a trailer section the library writes: not one that
        // frames or routes the message (RFC 9110 §6.5.1), which the parsers
        // of both sides refuse in trailers (parser.h), nor one of the
        // connection's
        SGCL_INLINE_HOT bool trailer_allowed(std::string_view n) noexcept {
            return !(iequal(n, "content-length") || iequal(n, "transfer-encoding") || iequal(n, "host") || iequal(n, "trailer")
                     || iequal(n, "connection") || iequal(n, "keep-alive") || iequal(n, "te") || iequal(n, "upgrade")
                     || iequal(n, "proxy-connection") || iequal(n, "content-type") || iequal(n, "content-encoding")
                     || iequal(n, "content-range") || iequal(n, "cache-control") || iequal(n, "expect") || iequal(n, "authorization")
                     || iequal(n, "set-cookie") || iequal(n, "cookie"));
        }

        // The trailers fit to go: every field a token's name and a value
        // without CR, LF or another control, its name allowed in a trailer
        // section. Ones that are not are left out (the writer's trailers are
        // the program's, often copied from another message)
        SGCL_INLINE_HOT bool trailer_writable(std::string_view n, std::string_view v) noexcept {
            if (!is_token(n) || !trailer_allowed(n)) {
                return false;
            }
            for (unsigned char c : v) {
                if (!field_value_char(c)) {
                    return false;
                }
            }
            return true;
        }

        // A trailer section of HTTP/2 (RFC 9113 §8.1): the fields in lower
        // case, no pseudo-field, the ones not fit to go left out
        struct TrailerFields final : h2::FieldBlock {
            const http::headers& fields;

            SGCL_INLINE_HOT explicit TrailerFields(const http::headers& fields) noexcept
            : fields(fields) {
            }

            void encode(h2::Encoder& e, std::string& out) const noexcept override {
                thread_local std::string lower;
                for (auto& f : HeadersAccess::fields(fields)) {
                    if (!trailer_writable(f.first.view(), f.second.view())) {
                        continue;
                    }
                    lower.assign(f.first.view());
                    for (auto& c : lower) {
                        c = ascii_lower(c);
                    }
                    e.encode(out, lower, f.second.view());
                }
            }
        };

        // An informational response of HTTP/2 (103 Early Hints): :status
        // and the fields, none of a connection's
        struct InformationalFields final : h2::FieldBlock {
            int status;
            const http::headers& fields;

            SGCL_INLINE_HOT InformationalFields(int status, const http::headers& fields) noexcept
            : status(status), fields(fields) {
            }

            void encode(h2::Encoder& e, std::string& out) const noexcept override {
                char code[4] = {char('0' + status / 100 % 10), char('0' + status / 10 % 10), char('0' + status % 10), 0};
                e.encode(out, ":status", std::string_view(code, 3));
                thread_local std::string lower;
                for (auto& f : HeadersAccess::fields(fields)) {
                    if (ResponseFields::connection_field(f.first.view())) {
                        continue;
                    }
                    lower.assign(f.first.view());
                    for (auto& c : lower) {
                        c = ascii_lower(c);
                    }
                    e.encode(out, lower, f.second.view());
                }
            }
        };

        // What a response recorder (test.h) keeps of a writer that sends
        // nowhere: the head as it went at the first flush, the bytes the
        // flushes took, the informational responses, the count of flushes
        struct RecordState {
            bool head_sent = false;
            int status = 200;
            http::headers fields;
            vector<byte> body;
            vector<pair<int, http::headers>> informational;
            int flushes = 0;
        };

        // A transform of a body's bytes as they go out (a compression
        // middleware's coding): what is buffered handed to it before each
        // flush and at the end, replaced by what it encodes to; `last` the
        // end of the body
        struct BodyFilter {
            virtual ~BodyFilter() = default;
            virtual void transform(BodyBuffer& body, bool last) = 0;
        };

        // The server's side of one exchange: the response as the handler
        // builds it, and how it goes out (over the wire of HTTP/1.1, or on
        // an HTTP/2 stream when h2 is set, or into a record when record is
        // set: a response recorder's)
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
            uint64_t flushed = 0;             // body bytes handed to flushes (the access log's count; sent counts the framing too)
            bool touched = false;             // a status set or a byte written
            bool hijacked = false;
            bool ended = false;               // the response finished (finish_start): a writer kept past it writes nothing
            optional<io::error> failed;       // a write that failed
            io::file file;                    // write(file) with nothing else written: the body, sent after the head by sendfile
            uint64_t file_at = 0;             // its bytes: from file_at, file_n of them
            uint64_t file_n = 0;
            bool has_file = false;
            bool aborted = false;             // the response broken off (a reverse proxy's backend failed half-way): no end is sent
            http::headers trailers;           // sent after the body (a chunked body's trailer section, HTTP/2's trailing HEADERS)
            std::string trailer_tail;         // the last chunk and the trailer section of HTTP/1.1, while they are written
            tracked_ptr<RecordState> record;  // a response recorder's writer: nothing sent, everything kept
            // the hooks of middlewares: before_head once, when the head is
            // about to be made (a flush, or the end: the fields' last
            // changes, a session's cookie); after_end once the response has
            // gone, with the body's bytes (a log's record). Each chains the
            // one set before it
            function<void(WriterImpl&)> before_head;
            function<void(const WriterImpl&, uint64_t)> after_end;
            tracked_ptr<BodyFilter> filter;   // the body's coding, when a middleware set one at the head
            bool finishing = false;            // the end of the response is being made (finish_start): a hook sees the whole body

            // before_head run, once, as the head is about to be made; then
            // the filter, when one is set, given what is buffered (a file
            // kept for sendfile read in first: the coding goes through
            // memory)
            SGCL_INLINE_HOT void head_coming() {
                if (before_head && !head_sent && !hijacked) [[unlikely]] {
                    auto f = std::move(before_head);
                    before_head = nullptr;
                    f(*this);
                }
                if (filter && !hijacked) [[unlikely]] {
                    take_file();
                    filter->transform(body, finishing);
                }
            }

            SGCL_INLINE_HOT static bool bodiless(int status) noexcept {
                return (status >= 100 && status < 200) || status == 204 || status == 304;
            }

            // The bytes of the body the handler gave: what went by flushes,
            // what is buffered and a file kept for sendfile; none for HEAD
            // and a status without a body (the access log's `bytes`)
            SGCL_INLINE_HOT uint64_t body_bytes() const noexcept {
                if (head_request || bodiless(status)) {
                    return 0;
                }
                return flushed + body.size() + (has_file ? file_n : 0);
            }

            // write(file): the file from its position to its end is the
            // body, its position moved to the end. Kept as the file, to be
            // sent by sendfile after the head, when it is all the body will
            // be (HTTP/1.1, nothing written before it, no flush); else its
            // bytes read into the body now
            void write_file(const io::file& f) {
                touched = true;
                if (wire && !h2 && !head_sent && !has_file && body.empty()) {
                    if (auto rest = net::detail::file_rest(f)) {
                        file = f;
                        file_at = rest->first;
                        file_n = rest->second;
                        has_file = true;
                        ::lseek(f.fd(), off_t(file_at + file_n), SEEK_SET);
                        return;
                    }
                }
                take_file();
                // read straight into the body's blocks: no buffer on the
                // stack (a deep one leaves words of this request on the
                // worker's dead stack, which keep it alive)
                body.append_read([&](std::byte* at, size_t room) -> size_t {
                    auto got = f.read(slice<byte>(at, room));
                    if (!got) {
                        if (!failed) {
                            failed = got.error();
                        }
                        return 0;
                    }
                    return *got;
                });
            }

            // A file kept by write(file) read into the body: something else
            // is written after it, or it goes out by a flush
            void take_file() noexcept {
                if (!has_file) {
                    return;
                }
                has_file = false;
                append_region(file.fd(), file_at, file_n);
                file = io::file();
            }

            // Bytes [at, at + n) of the file read into the body by pread
            // (the file's position untouched): how many there were, fewer
            // when the file shrank
            uint64_t append_region(int fd, uint64_t at, uint64_t n) noexcept {
                uint64_t from = at;
                const uint64_t end = at + n;
                body.append_read([&](std::byte* to, size_t room) -> size_t {
                    while (from < end) {
                        ssize_t got = ::pread(fd, to, size_t(std::min<uint64_t>(room, end - from)), off_t(from));
                        if (got < 0 && errno == EINTR) {
                            continue;
                        }
                        if (got <= 0) {
                            return 0;   // the file shrank: the body is what there was
                        }
                        from += uint64_t(got);
                        return size_t(got);
                    }
                    return 0;
                });
                return from - at;
            }

            // Bytes [at, at + n) of the file as the body's next bytes
            // (serve.h's ranges): kept as the file, to go by sendfile after
            // the head, when they are all the body will be (as write_file),
            // else read into the body now
            void write_region(const io::file& f, uint64_t at, uint64_t n) {
                touched = true;
                if (wire && !h2 && !head_sent && !has_file && body.empty()) {
                    file = f;
                    file_at = at;
                    file_n = n;
                    has_file = true;
                    return;
                }
                take_file();
                append_region(f.fd(), at, n);
            }

            // The head (in the wire's buffer), then the file by sendfile
            // (over TLS its blocks sealed where they lie); a file that
            // shrank since is short of the Content-Length sent, and the
            // connection ends after it
            static async::task<expected<void, io::error>> _sent_file(tracked_ptr<WriterImpl> self) noexcept {
                auto& c = net::detail::ConnectionAccess::impl(self->wire->connection());
                const std::string& out = self->wire->out;
                auto head = co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
                if (!head) {
                    co_return self->sent_result(head);
                }
                auto r = co_await c.async_send_file(self->file.fd(), self->file_at, self->file_n);
                self->file = io::file();
                if (r && *r != self->file_n) {
                    self->close_after = true;
                    co_return self->sent_result(io::detail::fail(io::error(std::make_error_code(std::errc::io_error), "write", "a file shorter than its Content-Length")));
                }
                co_return self->sent_result(r);
            }

            // The head, with the framing decided, at the end of `h` (the
            // wire's buffer): `length` for a whole body known now, nullopt
            // for a flush (chunked, the handler's length, or to the close)
            void head_to(std::string& h, optional<uint64_t> length) noexcept {
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
                if (chunked && !trailers.empty() && !HeadersAccess::count(fields, "trailer")) {
                    // the names of the trailers known now announced (RFC
                    // 9110 §6.6.2); ones set later still go
                    bool first = true;
                    for (auto& f : HeadersAccess::fields(trailers)) {
                        if (!trailer_writable(f.first.view(), f.second.view())) {
                            continue;
                        }
                        h += first ? "Trailer: " : ", ";
                        h += f.first.view();
                        first = false;
                    }
                    if (!first) {
                        h += "\r\n";
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

            // The length of a body sent whole: what is buffered; to a HEAD,
            // the handler's Content-Length when it gave one (the length a
            // GET would have had: a reverse proxy's, a handler of its own)
            SGCL_INLINE_HOT uint64_t whole_length() const noexcept {
                if (head_request && body.empty()) {
                    if (auto h = handler_length()) {
                        return *h;
                    }
                }
                return uint64_t(body.size());
            }

            // The handler's Content-Length, when it gave one that is a number
            SGCL_INLINE_HOT optional<uint64_t> handler_length() const noexcept {
                optional<uint64_t> cl;
                if (!HeadersAccess::count(fields, "content-length") || !content_length(fields, cl)) {
                    return nullopt;
                }
                return cl;
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

            // The head in the wire's buffer and the body's blocks after it as
            // one write (net: ConnImpl::start_write_parts): a socket takes
            // them in one sendmsg, TLS seals its records straight from the
            // blocks, and the body is not copied into the wire's buffer
            // first. The blocks go back to the worker's pool when the write
            // is done: at once when the connection took all of it, else at
            // the end of the task that writes the rest
            optional<async::task<expected<void, io::error>>> send_parts(expected<void, io::error>& now) {
                const std::string& out = wire->out;
                vector<slice<const byte>> parts;
                parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
                body.each([&](const slice<const byte>& part) {
                    parts.push_back(part);
                });
                return _write_parts(parts, now);
            }

            // The body written so far in the framing of a flushed response,
            // after what the wire's buffer holds (the head, when it goes
            // now), as the pieces of one write: the buffer with a chunk's
            // size line, the body's blocks where they lie, and the chunk's
            // CRLF (with the last chunk when `last`), a static piece. The
            // bytes held in the body buffer itself (64 at most, no blocks)
            // are copied into the wire's buffer instead. With the
            // handler's Content-Length, what goes past it is dropped and
            // the connection ends after the response, as before
            optional<async::task<expected<void, io::error>>> send_framed(expected<void, io::error>& now, bool last) {
                if (failed) {
                    now = io::detail::fail(*failed);
                    return nullopt;
                }
                std::string& out = wire->out;
                const bool bodyful = !head_request && !bodiless(status);
                size_t n = bodyful ? body.size() : 0;
                std::string_view tail;
                if (n) {
                    if (chunked) {
                        char size[20];
                        int k = std::snprintf(size, sizeof(size), "%zx\r\n", n);
                        out.append(size, size_t(k));
                        sent += size_t(k) + n + 2;
                        tail = last ? _last_chunk(true) : std::string_view("\r\n");
                    } else if (declared) {
                        const uint64_t room = *declared > sent ? *declared - sent : 0;
                        if (n > room) {
                            n = size_t(room);
                            close_after = true;   // more than it said: the rest is dropped
                        }
                        sent += n;
                    } else {
                        sent += n;
                    }
                } else if (bodyful && chunked && last) {
                    tail = _last_chunk(false);
                }
                if (last && aborted) {
                    // broken off: no last chunk, the connection ends with
                    // what went (a client reads a body cut short, never a
                    // whole one)
                    tail = std::string_view();
                    close_after = true;
                }
                if (last && declared && sent != *declared) {
                    close_after = true;
                }
                if (n == 0 || body.chunks().empty()) {
                    if (n) {
                        body.copy_to(out, n);
                    }
                    out.append(tail);
                    body.release();
                    return send_start(now);
                }
                vector<slice<const byte>> parts;
                parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
                size_t left = n;
                body.chunks().each([&](const slice<const byte>& part) {
                    if (left == 0) {
                        return;
                    }
                    const size_t k = std::min(left, part.size());
                    parts.push_back(slice<const byte>(part.owner(), part.data(), k));
                    left -= k;
                });
                if (!tail.empty()) {
                    parts.push_back(slice<const byte>(reinterpret_cast<const byte*>(tail.data()), tail.size()));
                }
                return _write_parts(parts, now);
            }

            // The last chunk of a chunked body with the trailer section
            // (RFC 9112 §7.1.2), after the CRLF of the chunk before it when
            // `after_data`: static text without trailers, else made in
            // trailer_tail, which the writer holds while it goes
            std::string_view _last_chunk(bool after_data) noexcept {
                bool any = false;
                for (auto& f : HeadersAccess::fields(trailers)) {
                    any = any || trailer_writable(f.first.view(), f.second.view());
                }
                if (!any) {
                    return after_data ? std::string_view("\r\n0\r\n\r\n") : std::string_view("0\r\n\r\n");
                }
                trailer_tail.clear();
                if (after_data) {
                    trailer_tail += "\r\n";
                }
                trailer_tail += "0\r\n";
                for (auto& f : HeadersAccess::fields(trailers)) {
                    if (trailer_writable(f.first.view(), f.second.view())) {
                        trailer_tail += f.first.view();
                        trailer_tail += ": ";
                        trailer_tail += f.second.view();
                        trailer_tail += "\r\n";
                    }
                }
                trailer_tail += "\r\n";
                return trailer_tail;
            }

            // The pieces as one write begun without a frame; the body's
            // blocks back to the pool when it is done
            SGCL_INLINE_HOT optional<async::task<expected<void, io::error>>> _write_parts(const vector<slice<const byte>>& parts, expected<void, io::error>& now) {
                auto s = net::detail::ConnectionAccess::impl(wire->connection()).start_write_parts(parts);
                if (s.rest) {
                    return _sent_parts(tracked_ptr<WriterImpl>(this), std::move(*s.rest));
                }
                body.release();
                now = sent_result(s.done);
                return nullopt;
            }

            static async::task<expected<void, io::error>> _sent_parts(tracked_ptr<WriterImpl> self, async::task<expected<size_t, io::error>> rest) noexcept {
                auto r = co_await rest;
                self->body.release();
                co_return self->sent_result(r);
            }

            SGCL_INLINE_HOT expected<void, io::error> sent_result(const expected<size_t, io::error>& r) noexcept {
                if (!r) {
                    failed = r.error();
                    close_after = true;
                    return io::detail::fail(r);
                }
                return expected<void, io::error>();
            }

            // The rest of a send that would wait; the writer (and its bytes)
            // held by the task
            static async::task<expected<void, io::error>> _sent(tracked_ptr<WriterImpl> self, async::task<expected<size_t, io::error>> rest) noexcept {
                auto r = co_await rest;
                co_return self->sent_result(r);
            }

            // The handler's fields checked before a head of them goes out:
            // a name that is not a token or a value with CR, LF, NUL or
            // another control (a user's text splitting the response) is the
            // writer's first error, kept and given back by every flush
            // after it, and nothing of the head is sent
            SGCL_INLINE_HOT bool fields_writable() {
                if (auto e = invalid_field(fields)) {
                    if (!failed) {
                        failed = io::error(std::make_error_code(std::errc::invalid_argument), "response", *e);
                    }
                    close_after = true;
                    return false;
                }
                return true;
            }

            async::task<expected<void, io::error>> flush() noexcept {
                expected<void, io::error> now;
                if (auto rest = flush_start(now)) {
                    co_return co_await *rest;
                }
                co_return now;
            }

            // flush() begun without a frame: what is written sent as far as
            // the connection takes it at once (the result in `now`), a task
            // only for what would wait (and for HTTP/2's stream). A server's
            // event (events.h) flushed this way costs its own frame alone
            optional<async::task<expected<void, io::error>>> flush_start(expected<void, io::error>& now) {
                head_coming();
                if (failed) {
                    now = io::detail::fail(*failed);
                    return nullopt;
                }
                if (record) {
                    record_flush();
                    now = expected<void, io::error>();
                    return nullopt;
                }
                if (h2) {
                    flushed += body.size();
                    return _h2_flush();
                }
                take_file();   // a flush sends what is written: the file's bytes among them
                flushed += body.size();
                std::string& out = wire->out;
                out.clear();
                if (!head_sent) {
                    if (!fields_writable()) {
                        now = io::detail::fail(*failed);
                        return nullopt;
                    }
                    set_optional(declared, handler_length());
                    head_to(out, nullopt);
                    head_sent = true;
                }
                return send_framed(now, false);
            }

            // After the handler: the rest of the response
            async::task<expected<void, io::error>> finish() noexcept {
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
                finishing = true;
                head_coming();
                ended = true;
                if (hijacked) {
                    now = expected<void, io::error>();
                    return nullopt;
                }
                if (record) {
                    record_flush();
                    now = expected<void, io::error>();
                    return nullopt;
                }
                if (h2) {
                    return _h2_finish_start(now);
                }
                std::string& out = wire->out;
                out.clear();
                if (aborted && head_sent) {
                    // broken off after the head: the end left unsent, the
                    // connection ended (send_framed sends no last chunk)
                    return send_framed(now, true);
                }
                if (!head_sent && !trailers.empty() && request_minor >= 1 && !head_request && !bodiless(status)) {
                    // trailers go after a chunked body only: the head as a
                    // flush makes it (no length of the handler's: the body
                    // is chunked), then the body, the last chunk and them
                    take_file();
                    if (!fields_writable()) {
                        now = io::detail::fail(*failed);
                        return nullopt;
                    }
                    declared.reset();
                    head_to(out, nullopt);
                    head_sent = true;
                    if (chunked) {
                        flushed += body.size();
                        return send_framed(now, true);
                    }
                    body.copy_to(out);   // not chunked after all (a length the handler set): the body whole, the trailers dropped
                    body.release();
                    return send_start(now);
                }
                if (has_file && !head_sent) {
                    // the body is the file alone: its length from fstat
                    // (write_file), the head, then sendfile
                    if (!fields_writable()) {
                        file = io::file();
                        has_file = false;
                        now = io::detail::fail(*failed);
                        return nullopt;
                    }
                    head_to(out, file_n);
                    head_sent = true;
                    if (head_request || bodiless(status) || file_n == 0) {
                        file = io::file();
                        has_file = false;
                        return send_start(now);
                    }
                    has_file = false;
                    return _sent_file(tracked_ptr<WriterImpl>(this));
                }
                if (!head_sent) {
                    if (!fields_writable()) {
                        now = io::detail::fail(*failed);
                        return nullopt;
                    }
                    head_to(out, whole_length());
                    head_sent = true;
                    if (!head_request && !bodiless(status)) {
                        if (!body.chunks().empty() && !failed) {
                            return send_parts(now);   // the blocks written where they are
                        }
                        body.copy_to(out);
                    }
                } else {
                    return send_framed(now, true);   // the rest after a flush: its blocks where they lie
                }
                body.release();   // copied into the wire's buffer: the blocks back to the worker's pool
                return send_start(now);
            }

            // --- HTTP/2 ---------------------------------------------------------

            SGCL_INLINE_HOT expected<void, io::error> _h2_head(optional<uint64_t> length, bool end_stream) {
                ResponseFields block(status, fields, bodiless(status) ? nullopt : length);
                auto r = h2->owner->send_headers(h2->id, block, end_stream);
                head_sent = true;
                if (!r) {
                    failed = r.error();
                }
                return r;
            }

            // The body's bytes as DATA, block by block (each slice owned by
            // its block and sent in place by the connection, then retired;
            // the bytes held inside the buffer itself are copied), END_STREAM
            // with the last when asked; the first `skip` bytes already sent
            async::task<expected<void, io::error>> _h2_send(BodyBuffer data, bool end_stream, size_t skip = 0) noexcept {
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
                            r = co_await h2->owner->send_block(h2->id, slice<const byte>(tracked_ptr<const void>(c), c->bytes + c->begin + cut, n), end_stream && left == 0);
                        }
                    }
                }
                h2->owner->retire(data);   // every piece queued (in place): the blocks back to the pool once written
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
                    h2->owner->retire(body);   // all of it queued (in place): the blocks back to the pool once written
                    taken = SIZE_MAX;
                }
                return expected<void, io::error>();
            }

            // A flush: the head (with the handler's length, if it set one)
            // and what is buffered, as DATA without END_STREAM
            async::task<expected<void, io::error>> _h2_flush() noexcept {
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

            // After the handler, with trailers: the head (with the length
            // of a body known whole), the body without END_STREAM, then the
            // trailers' HEADERS with it
            async::task<expected<void, io::error>> _h2_finish_trailers() noexcept {
                size_t taken = 0;
                if (auto r = _h2_now(false, head_sent ? declared : optional<uint64_t>(uint64_t(body.size())), taken); !r) {
                    co_return r;
                }
                if (taken != SIZE_MAX && !body.empty()) {
                    BodyBuffer rest = std::move(body);
                    body.clear();
                    if (auto r = co_await _h2_send(std::move(rest), false, taken); !r) {
                        co_return r;
                    }
                }
                TrailerFields block(trailers);
                auto r = h2->owner->send_headers(h2->id, block, true);
                if (!r) {
                    failed = r.error();
                }
                co_return r;
            }

            // A recorder's flush: the head kept as it is now (the first
            // time), what is buffered moved into the record
            void record_flush() {
                take_file();
                if (!record->head_sent) {
                    record->head_sent = true;
                    record->status = status;
                    record->fields = fields;
                    head_sent = true;
                }
                if (!head_request && !bodiless(status)) {
                    body.each([&](const slice<const byte>& part) {
                        record->body.insert(record->body.end(), part.data(), part.data() + part.size());
                    });
                }
                flushed += body.size();
                body.release();
                ++record->flushes;
            }

            // An informational response (1xx but 101) sent now, before the
            // head: on HTTP/1.1 its own head, on HTTP/2 a HEADERS without
            // END_STREAM; nothing to a client of HTTP/1.0 (RFC 9110 §15.2)
            async::task<expected<void, io::error>> informational(int code, http::headers f) noexcept {
                if (hijacked || ended || head_sent) {
                    co_return io::detail::fail(io::error(io::errc::closed, "write", "informational response after the head"));
                }
                if (auto e = invalid_field(f)) {
                    co_return io::detail::fail(io::error(std::make_error_code(std::errc::invalid_argument), "response", *e));
                }
                if (record) {
                    record->informational.push_back(pair<int, http::headers>(code, f));
                    co_return expected<void, io::error>();
                }
                if (h2) {
                    InformationalFields block(code, f);
                    co_return h2->owner->send_headers(h2->id, block, false);
                }
                if (request_minor == 0) {
                    co_return expected<void, io::error>();
                }
                std::string h;
                h.reserve(64);
                h += "HTTP/1.1 ";
                h += std::to_string(code);
                h += ' ';
                h += reason(code);
                h += "\r\n";
                for (auto& x : HeadersAccess::fields(f)) {
                    auto n = x.first.view();
                    if (ResponseFields::connection_field(n)) {
                        continue;
                    }
                    h += n;
                    h += ": ";
                    h += x.second.view();
                    h += "\r\n";
                }
                h += "\r\n";
                auto r = co_await wire->connection().async_write(string(std::string_view(h)));
                if (!r) {
                    failed = r.error();
                    close_after = true;
                    co_return io::detail::fail(r);
                }
                co_return expected<void, io::error>();
            }

            // After the handler: the head with the length of a body known
            // whole, or the rest after a flush, and END_STREAM; at once
            // when the windows take it (no frame), a task only for the rest
            optional<async::task<expected<void, io::error>>> _h2_finish_start(expected<void, io::error>& now) {
                if (failed) {
                    now = io::detail::fail(*failed);
                    return nullopt;
                }
                if (aborted && head_sent) {
                    now = io::detail::fail(io::error(io::errc::closed, "write", "response broken off"));   // the stream reset by the server
                    return nullopt;
                }
                if (!trailers.empty() && !head_request && !bodiless(status)) {
                    return _h2_finish_trailers();
                }
                size_t taken = 0;
                now = _h2_now(true, whole_length(), taken);
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
    //
    // A writer kept past its response (a task the handler started holds a
    // copy) writes nowhere once the server has sent the response: a write
    // or an error() is dropped, a flush is io::errc::closed, and the
    // connection, gone on to the next request, never sees its bytes.
    class response_writer {
    public:
        // 200 unless set; 100 to 199 are not the handler's (invalid_argument),
        // nor anything outside 200 to 999; after the head has gone, ignored
        SGCL_INLINE_HOT response_writer& set_status(int code) {
            if (code < 200 || code > 999) {
                throw invalid_argument("http::response_writer: a status is 200 to 999");
            }
            if (!_impl->head_sent) {
                _impl->status = code;
                _impl->touched = true;
            }
            return *this;
        }

        SGCL_INLINE_HOT int status() const noexcept {
            return _impl->status;
        }

        SGCL_INLINE_HOT response_writer& set_header(const string& name, const string& value) noexcept {
            _impl->fields.set(name, value);
            return *this;
        }

        SGCL_INLINE_HOT response_writer& add_header(const string& name, const string& value) noexcept {
            _impl->fields.add(name, value);
            return *this;
        }

        // A Set-Cookie field added (one per cookie: several cookies are
        // several fields)
        SGCL_INLINE_HOT response_writer& add_cookie(const cookie& c) {
            _impl->fields.add("Set-Cookie", c.to_string());
            return *this;
        }

        // The fields of the response; changed after the head has gone, they
        // go nowhere
        SGCL_INLINE_HOT http::headers& headers() const noexcept {
            return _impl->fields;
        }

        SGCL_INLINE_HOT response_writer& write(const string& text) noexcept {
            if (_impl->ended) {
                return *this;   // the response has gone: a writer kept past it writes nowhere
            }
            _impl->take_file();
            _impl->body.append(text.view());
            _impl->touched = true;
            return *this;
        }

        SGCL_INLINE_HOT response_writer& write(const slice<const byte>& data) noexcept {
            if (_impl->ended) {
                return *this;
            }
            _impl->take_file();
            _impl->body.append(data.data(), data.size());
            _impl->touched = true;
            return *this;
        }

        // The file from its position to its end, as the body's bytes (the
        // position moved to the end). When it is all the body (HTTP/1.1,
        // nothing written before or after it, no flush), it goes after
        // the head by sendfile, the file's pages to the socket with no
        // copy through the process (over TLS read in blocks and sealed
        // where they lie), and its length is the Content-Length; else its
        // bytes are taken as write(bytes) takes them. Only the body:
        // Content-Type and the rest are the handler's
        SGCL_INLINE_HOT response_writer& write(const io::file& f) {
            if (!_impl->ended) {
                _impl->write_file(f);
            }
            return *this;
        }

        // A literal, a character array, a std::string_view: as a string
        // (an exact match, else the conversions to a string and to bytes tie)
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT response_writer& write(const T& text) noexcept {
            return write(slice<const byte>(text));
        }

        // Sends now: the head, if it has not gone, and what is buffered.
        // A handler runs on a worker, so it flushes as a task does, `co_await
        // w.async_flush()`; flush() blocks a thread (a response written from
        // one of the program's threads, never a worker). After the
        // response has ended (or been hijacked): io::errc::closed
        SGCL_INLINE_HOT expected<void, io::error> flush() const {
            return _co_flush(_impl).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_flush() const noexcept {
            return _co_flush(_impl);
        }

        // The status with its reason as text/plain ("404 Not Found" gives
        // "Not Found\n"), what was buffered dropped: Go's http.Error
        SGCL_INLINE_HOT void error(int code) {
            error(code, reason(code));
        }

        void error(int code, const string& message) {
            set_status(code);
            if (_impl->ended) {
                return;
            }
            if (!_impl->head_sent) {
                _impl->file = io::file();
                _impl->has_file = false;
                _impl->body.release();
                _impl->fields.erase("Content-Length");
                _impl->fields.set("Content-Type", "text/plain; charset=utf-8");
                _impl->fields.set("X-Content-Type-Options", "nosniff");
            }
            _impl->body.append(message.view());
            _impl->body.append("\n", 1);
        }

        // A redirect: the status (302 unless given; a 3xx) and Location as given
        SGCL_INLINE_HOT void redirect(const string& location, int code = status::found) {
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
        SGCL_INLINE_HOT expected<pair<net::connection, io::reader>, io::error> hijack() noexcept {
            if (_impl->record) {
                return io::detail::fail(io::error(std::make_error_code(std::errc::operation_not_supported), "hijack", "response recorder"));
            }
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

        // The fields sent after the body, its trailer section (RFC 9110
        // §6.5): set any time before the response ends. On HTTP/1.1 they
        // need a chunked body, so a response with trailers is chunked to a
        // client of HTTP/1.1 (one of HTTP/1.0, or a length the handler set
        // before a flush, gets none); on HTTP/2 they are the stream's last
        // HEADERS. The names known when the head goes are announced in a
        // Trailer field, unless the handler set one. A field that frames or
        // routes a message (Content-Length, Transfer-Encoding, Host,
        // Trailer, the connection's, Content-Type, Set-Cookie...) or is not
        // fit to be written is left out
        SGCL_INLINE_HOT http::headers& trailers() const noexcept {
            return _impl->trailers;
        }

        // An informational response sent now, before the head: 103 Early
        // Hints with its Link fields, or any 1xx but 101 (a switch of
        // protocols is hijack()'s) and 100 (the server's own, when the
        // handler reads a body that expects it); invalid_argument for
        // another code. A client of HTTP/1.0 gets none (RFC 9110 §15.2):
        // success, nothing sent. After the head has gone (or the response
        // ended, or was hijacked): io::errc::closed; fields not fit to be
        // written: std::errc::invalid_argument, nothing sent. A handler is a
        // task and writes `co_await w.async_send_informational(103, f)`
        SGCL_INLINE_HOT expected<void, io::error> send_informational(int code, const http::headers& fields = {}) const {
            _check_informational(code);
            return _impl->informational(code, fields).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_send_informational(int code, const http::headers& fields = {}) const {
            _check_informational(code);
            return _impl->informational(code, fields);
        }

        // Whether the head has gone (a flush was made)
        SGCL_INLINE_HOT bool header_sent() const noexcept {
            return _impl->head_sent;
        }

    private:
        friend struct detail::WriterAccess;

        SGCL_INLINE_HOT explicit response_writer(const tracked_ptr<detail::WriterImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::WriterImpl> _impl;

        SGCL_INLINE_HOT static void _check_informational(int code) {
            if (code < 102 || code > 199) {
                throw invalid_argument("http::response_writer: an informational status is 102 to 199");
            }
        }

        static async::task<expected<void, io::error>> _co_flush(tracked_ptr<detail::WriterImpl> impl) noexcept {
            if (impl->hijacked || impl->ended) {
                co_return io::detail::fail(io::error(io::errc::closed, "flush", "response"));
            }
            co_return co_await impl->flush();
        }
    };

    namespace detail {
        struct WriterAccess {
            SGCL_INLINE_HOT static response_writer make(const tracked_ptr<WriterImpl>& impl) noexcept {
                return response_writer(impl);
            }

            SGCL_INLINE_HOT static const tracked_ptr<WriterImpl>& impl(const response_writer& w) noexcept {
                return w._impl;
            }

            // The response broken off (a reverse proxy's backend failed
            // after the head went): no end is sent, the connection (HTTP/1.1)
            // ends, the stream (HTTP/2) is reset
            SGCL_INLINE_HOT static void abort(const response_writer& w) noexcept {
                w._impl->aborted = true;
                w._impl->close_after = true;
            }

            // async_flush begun without a frame (WriterImpl::flush_start),
            // with its checks: the result in `now`, or the task of the rest
            static optional<async::task<expected<void, io::error>>> flush_start(const response_writer& w, expected<void, io::error>& now) {
                auto& impl = *w._impl;
                if (impl.hijacked || impl.ended) {
                    now = io::detail::fail(io::error(io::errc::closed, "flush", "response"));
                    return nullopt;
                }
                return impl.flush_start(now);
            }

            // The connection taken over as hijack() takes it, the bytes the
            // server had read past the request moved into rest (a
            // WebSocket's first frames), so that the taker reads the
            // connection itself
            static expected<net::connection, io::error> take_over(const response_writer& w, std::string& rest) noexcept {
                auto h = const_cast<response_writer&>(w).hijack();
                if (!h) {
                    return unexpected(h.error());
                }
                auto& wire = *w._impl->wire;
                rest.assign(wire.view());
                wire.consume(rest.size());
                return h->first;
            }
        };
    }
}
