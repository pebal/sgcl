//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "headers.h"
#include "detail/wire.h"
#include "../ip.h"
#include "../url.h"
#include "../../async/stop_token.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cassert>
#include <cstdint>

namespace sgcl::net::http {
    namespace detail {
        // What a request is made of, on either side
        struct RequestImpl {
            string method;
            string url_text;                 // a client's, as given
            optional<net::url> url;          // parsed: at construction (client), from the head (server)
            http::headers fields;
            int minor = 1;
            bool h2 = false;                 // came over HTTP/2 (a server's) or goes over it (a client's, once sent)

            // a client's body
            enum class BodyKind : uint8_t { none, text, bytes, stream };
            BodyKind body_kind = BodyKind::none;
            string text;
            vector<byte> bytes;
            io::reader stream;
            optional<uint64_t> stream_length;

            // a server's
            string head;                     // the block the fields are slices of
            slice<const char> target;        // the request-target as it came: a slice of the head
            slice<const char> host;          // the Host field's value, a slice of the head (empty: none)
            bool url_later = false;          // url left to the first url() (the head routed by net::detail::origin_form_path)
            sgcl::atomic<tracked_ptr<net::url>> url_made;   // that url, once made: by whichever first asks, published with a compare-exchange

            // The url of a server's request, made on the first use when the
            // head was routed without it: "http://" + Host + the target, as
            // at the head; one made by two readers at once is one of theirs
            const net::url* url_of() {
                if (url) {
                    return &*url;
                }
                if (!url_later) {
                    return nullptr;
                }
                if (auto made = url_made.load()) {
                    return made.get();
                }
                auto parsed = net::url::parse(string::concat("http://", host.empty() ? std::string_view("localhost") : host.view(), target));
                if (!parsed) {
                    return nullptr;   // not reached: the fast path answers only what the parse takes (url.h)
                }
                tracked_ptr<net::url> fresh = make_tracked<net::url>(std::move(*parsed));
                tracked_ptr<net::url> none;
                if (!url_made.compare_exchange_strong(none, fresh)) {
                    return none.get();   // another reader's, made first
                }
                return fresh.get();
            }
            tracked_ptr<Body> body;
            vector<pair<string, string>> path_values;
            net::endpoint remote;
            async::stop_token stop;
            optional<uint64_t> content_length;
        };

        struct RequestAccess;
    }

    // A request: the same type for the client, which builds one and sends
    // it, and for the server, which hands the one it read to a handler, as
    // in Go. A handle of one word: a copy is the same request.
    //
    // The body of a received request is read with text(), bytes() or as a
    // stream, body(); each read is bounded by the server's max_body_bytes
    // (net::errc::body_too_large past it). A handler that does not read it
    // has what is left read for it after it returns, up to 256 KB (Go's
    // bound), and past that the connection is closed.
    class request {
    public:
        // A request to send: the method as given ("GET", "POST"), the URL
        // parsed now and its error reported by the send (net::errc::invalid_url)
        request(const string& method, const string& url)
        : _impl(make_tracked<detail::RequestImpl>()) {
            _impl->method = method;
            _impl->url_text = url;
            if (auto u = net::url::parse(url)) {
                _impl->url = std::move(*u);
            }
        }

        string method() const {
            return _impl->method;
        }

        // The protocol the request came over, as Go's r.Proto: "HTTP/1.1",
        // "HTTP/1.0" or "HTTP/2.0"
        string proto() const {
            return _impl->h2 ? "HTTP/2.0" : _impl->minor == 0 ? "HTTP/1.0" : "HTTP/1.1";
        }

        // The URL: the client's as parsed, or, on the server, the one the
        // request was for ("http://" + Host + the target, or the target in
        // absolute form). A client request whose URL did not parse has
        // none: invalid_argument (the send reports it as invalid_url first)
        net::url url() const {
            auto u = _impl->url_of();
            if (!u) {
                throw invalid_argument("http::request: the URL does not parse");
            }
            return *u;
        }

        // A field of the head, "" when there is none
        string header(const string& name) const {
            return _impl->fields.get(name);
        }

        http::headers& headers() const noexcept {
            return _impl->fields;
        }

        request& set_header(const string& name, const string& value) {
            _impl->fields.set(name, value);
            return *this;
        }

        request& add_header(const string& name, const string& value) {
            _impl->fields.add(name, value);
            return *this;
        }

        // The body to send, with its Content-Length
        request& set_body(const string& text) {
            _impl->body_kind = detail::RequestImpl::BodyKind::text;
            _impl->text = text;
            return *this;
        }

        request& set_body(vector<byte> bytes) {   // by value: a vector's copy copies the bytes, its move does not
            _impl->body_kind = detail::RequestImpl::BodyKind::bytes;
            _impl->bytes = std::move(bytes);
            return *this;
        }

        // A stream, read when the request is sent: with its length given,
        // Content-Length; without, chunked. A stream cannot be sent twice,
        // so a request with one is neither retried nor redirected with 307
        request& set_body(const io::reader& stream, optional<uint64_t> length = nullopt) {
            _impl->body_kind = detail::RequestImpl::BodyKind::stream;
            _impl->stream = stream;
            detail::set_optional(_impl->stream_length, length);
            return *this;
        }

        // The server's side. {name} of the route's pattern, unescaped
        string path_value(const string& name) const {
            for (auto& p : _impl->path_values) {
                if (p.first == name) {
                    return p.second;
                }
            }
            return string();
        }

        // The first value of the name in the query, "" when there is none:
        // found in the query's text, only that value decoded (the pairs
        // before it are not made into strings)
        string query(const string& name) const {
            auto u = _impl->url_of();
            if (!u || !u->has_query()) {
                return string();
            }
            return net::query_params::first(u->query(), name);
        }

        // The value of the first cookie of the name in the Cookie fields
        string cookie(const string& name) const {
            auto c = detail::request_cookie(_impl->fields, name.view());
            return c ? *c : string();
        }

        // Content-Length as the request gave it; nullopt for chunked or none
        optional<uint64_t> content_length() const noexcept {
            return _impl->content_length;
        }

        net::endpoint remote_endpoint() const noexcept {
            return _impl->remote;
        }

        // Stopped when the server closes (close(), or the end of a
        // shutdown for this connection) or a write of the response fails
        async::stop_token stop() const noexcept {
            return _impl->stop;
        }

        // The whole body as text, bounded by the server's max_body_bytes.
        // text() blocks the thread (the reading runs on the scheduler and
        // the thread waits for it: never from a worker); a handler that is
        // a task writes `co_await req.async_text()`
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

        // The body as a stream (reads bounded by max_body_bytes); an empty
        // stream for a request without a body
        io::reader body() const {
            if (!_impl->body) {
                return io::reader(detail::no_body);
            }
            return io::reader(_impl->body);
        }

        // The trailers of a chunked body, once it has been read to its end
        http::headers trailers() const {
            return _impl->body ? _impl->body->trailers() : http::headers();
        }

    private:
        friend struct detail::RequestAccess;

        explicit request(const tracked_ptr<detail::RequestImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::RequestImpl> _impl;

        static async::task<expected<vector<byte>, io::error>> _co_bytes(tracked_ptr<detail::RequestImpl> impl) {
            if (!impl->body) {
                co_return vector<byte>();
            }
            co_return co_await impl->body->read_everything();
        }

        static async::task<expected<string, io::error>> _co_text(tracked_ptr<detail::RequestImpl> impl) {
            if (!impl->body) {
                co_return string();
            }
            co_return co_await impl->body->read_text();   // straight into the string
        }

    };

    namespace detail {
        struct RequestAccess {
            static request make(const tracked_ptr<RequestImpl>& impl) {
                return request(impl);
            }

            static const tracked_ptr<RequestImpl>& impl(const request& r) noexcept {
                return r._impl;
            }
        };
    }
}
