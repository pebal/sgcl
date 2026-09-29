//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "headers.h"
#include "status.h"
#include "detail/wire.h"
#include "../url.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cstdint>

namespace sgcl::encoding {
    class json;
}

namespace sgcl::net::http {
    namespace detail {
        struct ResponseImpl {
            int status = 0;
            int minor = 1;
            bool h2 = false;                 // came over HTTP/2
            string head;
            http::headers fields;
            optional<net::url> url;
            optional<uint64_t> content_length;
            tracked_ptr<Body> body;
        };

        struct ResponseAccess;
    }

    // A response as the client receives it. A handle of one word. The body
    // read to its end gives the connection back to the client's pool by
    // itself, with no close() (Go asks for both the end and Close); close()
    // is for a body left unread.
    //
    // A status of 4xx or 5xx is a response, not an error (as in Go): ok()
    // tells a 2xx.
    class response {
    public:
        int status() const noexcept {
            return _impl->status;
        }

        // The protocol the response came over, as Go's resp.Proto:
        // "HTTP/1.1", "HTTP/1.0" or "HTTP/2.0"
        string proto() const {
            return _impl->h2 ? "HTTP/2.0" : _impl->minor == 0 ? "HTTP/1.0" : "HTTP/1.1";
        }

        // 200 to 299
        bool ok() const noexcept {
            return _impl->status >= 200 && _impl->status < 300;
        }

        // A field of the head, "" when there is none
        string header(const string& name) const {
            return _impl->fields.get(name);
        }

        const http::headers& headers() const noexcept {
            return _impl->fields;
        }

        optional<uint64_t> content_length() const noexcept {
            return _impl->content_length;
        }

        // The URL the response came from: the last of the redirects
        net::url url() const {
            return *_impl->url;
        }

        // The whole body as text; the connection goes back to the pool.
        // text() blocks the thread (never from a worker); in a task
        // `co_await res.async_text()`
        expected<string, io::error> text() const {
            return _co_text(_impl).wait();
        }

        async::task<expected<string, io::error>> async_text() const {
            return _co_text(_impl);
        }

        expected<vector<byte>, io::error> bytes() const {
            return _co_bytes(_impl).wait();
        }

        async::task<expected<vector<byte>, io::error>> async_bytes() const {
            return _co_bytes(_impl);
        }

        // The body as JSON: a value, or a T through describe (defined in
        // download.h, which client.h and http.h bring in)
        expected<encoding::json, io::error> json() const;
        async::task<expected<encoding::json, io::error>> async_json() const;
        template<class T>
        expected<T, io::error> json() const;
        template<class T>
        async::task<expected<T, io::error>> async_json() const;

        // The body streamed into the file at path, through path + ".part"
        // renamed at its end (nothing half-written left): the bytes written
        expected<uint64_t, io::error> save(const string& path) const;
        async::task<expected<uint64_t, io::error>> async_save(string path) const;

        // The body as a stream; its end gives the connection back
        io::reader body() const {
            return io::reader(_impl->body);
        }

        // The trailers of a chunked body, once it has been read to its end
        http::headers trailers() const {
            return _impl->body->trailers();
        }

        // The body given up: the rest dropped when the buffer holds it
        // (the connection back to the pool), the connection closed when it
        // does not. Does not wait.
        void close() const {
            if (!_impl->body->discard_buffered()) {
                _impl->body->abandon();
            }
        }

    private:
        friend struct detail::ResponseAccess;

        explicit response(const tracked_ptr<detail::ResponseImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::ResponseImpl> _impl;

        static async::task<expected<vector<byte>, io::error>> _co_bytes(tracked_ptr<detail::ResponseImpl> impl) {
            co_return co_await impl->body->read_everything();
        }

        static async::task<expected<string, io::error>> _co_text(tracked_ptr<detail::ResponseImpl> impl) {
            co_return co_await impl->body->read_text();   // straight into the string
        }

    };

    namespace detail {
        struct ResponseAccess {
            static response make(const tracked_ptr<ResponseImpl>& impl) {
                return response(impl);
            }

            static const tracked_ptr<ResponseImpl>& impl(const response& r) noexcept {
                return r._impl;
            }
        };
    }
}
