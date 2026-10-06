//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "headers.h"
#include "status.h"
#include "detail/wire.h"
#include "../url.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/functions.h"
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
            io::reader decoded;              // what the body is read through when set: the client's decoding, the cache's copy
            bool reads_decoded = false;
            bool uncompressed = false;
            uint8_t cache = 0;               // response::cache_status
            optional<int64_t> age;           // a response of the cache: its age, seconds
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
        SGCL_INLINE_HOT int status() const noexcept {
            return _impl->status;
        }

        // The protocol the response came over, as Go's resp.Proto:
        // "HTTP/1.1", "HTTP/1.0" or "HTTP/2.0"
        SGCL_INLINE_HOT string proto() const noexcept {
            return _impl->h2 ? "HTTP/2.0" : _impl->minor == 0 ? "HTTP/1.0" : "HTTP/1.1";
        }

        // 200 to 299
        SGCL_INLINE_HOT bool ok() const noexcept {
            return _impl->status >= 200 && _impl->status < 300;
        }

        // A field of the head, "" when there is none
        SGCL_INLINE_HOT string header(const string& name) const noexcept {
            return _impl->fields.get(name);
        }

        SGCL_INLINE_HOT const http::headers& headers() const noexcept {
            return _impl->fields;
        }

        // Content-Length as the response gave it; nullopt for chunked, none,
        // and a body the client decoded (uncompressed())
        SGCL_INLINE_HOT optional<uint64_t> content_length() const noexcept {
            return _impl->content_length;
        }

        // How the client's cache (client::cache) answered: none (no cache, or
        // a request it does not take), miss (stored now, or not storable),
        // hit (fresh, no request sent), revalidated (a 304 refreshed the
        // stored response), stale (served stale: stale-while-revalidate, or
        // stale-if-error when the server failed)
        enum class cache_status : uint8_t { none, miss, hit, revalidated, stale };

        SGCL_INLINE_HOT cache_status from_cache() const noexcept {
            return cache_status(_impl->cache);
        }

        // The age of a response the cache served (RFC 9111 §4.2.3), its Age
        // field; nullopt for one from the network
        SGCL_INLINE_HOT optional<duration> age() const noexcept {
            if (!_impl->age) {
                return nullopt;
            }
            return duration(std::chrono::seconds(*_impl->age));
        }

        // Whether the client decoded the body by itself: it asked for a
        // coding (client::decompress) and the response came in it; its
        // Content-Encoding and Content-Length then removed (Go's
        // Response.Uncompressed)
        SGCL_INLINE_HOT bool uncompressed() const noexcept {
            return _impl->uncompressed;
        }

        // The cookies of the Set-Cookie fields, in their order (Go's
        // Response.Cookies); a field that holds no cookie is passed over
        vector<http::cookie> cookies() const noexcept {
            vector<http::cookie> out;
            for (auto& f : detail::HeadersAccess::fields(_impl->fields)) {
                if (detail::iequal(f.first.view(), "set-cookie")) {
                    if (auto c = detail::parse_cookie(string(f.second.view()))) {
                        out.push_back(std::move(*c));
                    }
                }
            }
            return out;
        }

        // The URL the response came from: the last of the redirects
        SGCL_INLINE_HOT net::url url() const noexcept {
            return *_impl->url;
        }

        // The whole body as text; the connection goes back to the pool.
        // text() blocks the thread (never from a worker); in a task
        // `co_await res.async_text()`
        SGCL_INLINE_HOT expected<string, io::error> text() const {
            return _co_text(_impl).wait();
        }

        SGCL_INLINE_HOT async::task<expected<string, io::error>> async_text() const noexcept {
            return _co_text(_impl);
        }

        SGCL_INLINE_HOT expected<vector<byte>, io::error> bytes() const {
            return _co_bytes(_impl).wait();
        }

        SGCL_INLINE_HOT async::task<expected<vector<byte>, io::error>> async_bytes() const noexcept {
            return _co_bytes(_impl);
        }

        // The body as JSON: a value, or a T through describe (defined in
        // download.h, which client.h and http.h bring in)
        expected<encoding::json, io::error> json() const;
        async::task<expected<encoding::json, io::error>> async_json() const noexcept;
        template<class T>
        expected<T, io::error> json() const;
        template<class T>
        async::task<expected<T, io::error>> async_json() const noexcept;

        // The body streamed into the file at path, through path + ".part"
        // renamed at its end (nothing half-written left): the bytes written
        expected<uint64_t, io::error> save(const string& path) const;
        async::task<expected<uint64_t, io::error>> async_save(string path) const noexcept;

        // The body as a stream (decoded, when uncompressed()); its end
        // gives the connection back
        SGCL_INLINE_HOT io::reader body() const noexcept {
            return _impl->reads_decoded ? _impl->decoded : io::reader(_impl->body);
        }

        // The trailers of a chunked body, once it has been read to its end
        SGCL_INLINE_HOT http::headers trailers() const noexcept {
            return _impl->body->trailers();
        }

        // The body given up: the rest dropped when the buffer holds it
        // (the connection back to the pool), the connection closed when it
        // does not. Does not wait.
        SGCL_INLINE_HOT void close() const {
            if (!_impl->body->discard_buffered()) {
                _impl->body->abandon();
            }
        }

    private:
        friend struct detail::ResponseAccess;

        SGCL_INLINE_HOT explicit response(const tracked_ptr<detail::ResponseImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::ResponseImpl> _impl;

        static async::task<expected<vector<byte>, io::error>> _co_bytes(tracked_ptr<detail::ResponseImpl> impl) noexcept {
            if (impl->reads_decoded) {
                co_return co_await io::async_read_all(impl->decoded);
            }
            co_return co_await impl->body->read_everything();
        }

        static async::task<expected<string, io::error>> _co_text(tracked_ptr<detail::ResponseImpl> impl) noexcept {
            if (impl->reads_decoded) {
                co_return co_await io::async_read_all_text(impl->decoded);
            }
            co_return co_await impl->body->read_text();   // straight into the string
        }

    };

    namespace detail {
        struct ResponseAccess {
            SGCL_INLINE_HOT static response make(const tracked_ptr<ResponseImpl>& impl) noexcept {
                return response(impl);
            }

            SGCL_INLINE_HOT static const tracked_ptr<ResponseImpl>& impl(const response& r) noexcept {
                return r._impl;
            }
        };
    }
}
