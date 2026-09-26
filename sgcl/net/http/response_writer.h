//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "headers.h"
#include "status.h"
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
#include <cstdio>
#include <mutex>
#include <string>

namespace sgcl::net::http {
    namespace detail {
        // "Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n" of now, made once a
        // second (RFC 9110 §6.6.1: a server with a clock sends one); the
        // time is time::now()'s, so a test's manual clock moves it
        inline std::string date_line() {
            static std::mutex lock;
            static int64_t second = INT64_MIN;
            static std::string line;
            auto now = time::now();
            std::lock_guard<std::mutex> g(lock);
            if (now.unix() != second) {
                second = now.unix();
                line = "Date: ";
                line += now.format(time::http).view();
                line += "\r\n";
            }
            return line;
        }

        // The server's side of one exchange: the response as the handler
        // builds it, and how it goes out
        struct WriterImpl {
            tracked_ptr<Wire> wire;
            int status = 200;
            http::headers fields;
            std::string body;                 // written, not yet sent
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

            // The head, with the framing decided: `length` for a whole body
            // known now, nullopt for a flush (chunked, the handler's length,
            // or to the close)
            std::string head(optional<uint64_t> length) {
                std::string h;
                h.reserve(256);
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
                    h += date_line();
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
                return h;
            }

            // The handler's Content-Length, when it gave one that is a number
            optional<uint64_t> handler_length() const {
                optional<uint64_t> cl;
                if (!HeadersAccess::count(fields, "content-length") || !content_length(fields, cl)) {
                    return nullopt;
                }
                return cl;
            }

            async::task<expected<void, io::error>> send(std::string bytes) {
                if (failed) {
                    co_return io::detail::fail(*failed);
                }
                if (bytes.empty()) {
                    co_return expected<void, io::error>();
                }
                slice<const byte> data(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
                auto r = co_await wire->connection().async_write(data);
                if (!r) {
                    failed = r.error();
                    close_after = true;
                    co_return io::detail::fail(r);
                }
                co_return expected<void, io::error>();
            }

            // The body written so far, in the framing of a flushed response
            std::string framed(std::string_view data) {
                std::string out;
                if (head_request || bodiless(status) || data.empty()) {
                    return out;
                }
                if (chunked) {
                    char size[20];
                    int n = std::snprintf(size, sizeof(size), "%zx\r\n", data.size());
                    out.append(size, size_t(n));
                    out += data;
                    out += "\r\n";
                } else if (declared) {
                    uint64_t room = *declared > sent ? *declared - sent : 0;
                    out.append(data.substr(0, size_t(std::min<uint64_t>(room, data.size()))));
                    if (data.size() > room) {
                        close_after = true;   // more than it said: the rest is dropped
                    }
                } else {
                    out += data;
                }
                sent += out.size();
                return out;
            }

            async::task<expected<void, io::error>> flush() {
                std::string out;
                if (!head_sent) {
                    set_optional(declared, handler_length());
                    out = head(nullopt);
                    head_sent = true;
                }
                out += framed(body);
                body.clear();
                co_return co_await send(std::move(out));
            }

            // After the handler: the rest of the response
            async::task<expected<void, io::error>> finish() {
                if (hijacked) {
                    co_return expected<void, io::error>();
                }
                std::string out;
                if (!head_sent) {
                    out = head(uint64_t(body.size()));
                    head_sent = true;
                    if (!head_request && !bodiless(status)) {
                        out += body;
                    }
                } else {
                    out = framed(body);
                    if (chunked && !head_request && !bodiless(status)) {
                        out += "0\r\n\r\n";
                    }
                    if (declared && sent != *declared) {
                        close_after = true;
                    }
                }
                body.clear();
                co_return co_await send(std::move(out));
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
            _impl->body += text.view();
            _impl->touched = true;
            return *this;
        }

        response_writer& write(const slice<const byte>& data) {
            _impl->body.append(reinterpret_cast<const char*>(data.data()), data.size());
            _impl->touched = true;
            return *this;
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
                _impl->body.clear();
                _impl->fields.erase("Content-Length");
                _impl->fields.set("Content-Type", "text/plain; charset=utf-8");
                _impl->fields.set("X-Content-Type-Options", "nosniff");
            }
            _impl->body += message.view();
            _impl->body += '\n';
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
