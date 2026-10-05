//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "form.h"
#include "headers.h"
#include "multipart.h"
#include "detail/wire.h"
#include "../ip.h"
#include "../tls.h"
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
            enum class BodyKind : uint8_t { none, text, bytes, stream, form };
            BodyKind body_kind = BodyKind::none;
            string text;
            vector<byte> bytes;
            io::reader stream;
            optional<uint64_t> stream_length;
            tracked_ptr<FormState> form;
            function<async::task<>(int, http::headers)> on_informational;   // the 1xx before the response, awaited (a reverse proxy forwards them)
            bool no_redirects = false;       // the first response whatever its status (a reverse proxy's, as Go's Transport)

            // a server's
            string head;                     // the block the fields are slices of
            slice<const char> target;        // the request-target as it came: a slice of the head
            slice<const char> host;          // the Host field's value, a slice of the head (empty: none)
            bool url_later = false;          // url left to the first url() (the head routed by net::detail::origin_form_path)
            sgcl::atomic<tracked_ptr<net::url>> url_made;   // that url, once made: by whichever first asks, published with a compare-exchange

            // The url of a server's request, made on the first use when the
            // head was routed without it: "http://" + Host + the target, as
            // at the head; one made by two readers at once is one of theirs
            const net::url* url_of() noexcept {
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
            net::connection conn;            // a server's: the connection it came over (its TLS state)
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
        // parsed now and its error reported by the send (net::errc::invalid_url,
        // a text past 512 MiB among it: url.h's UrlMaxSize)
        SGCL_INLINE_HOT request(const string& method, const string& url) noexcept
        : _impl(make_tracked<detail::RequestImpl>()) {
            _impl->method = method;
            _impl->url_text = url;
            if (auto u = net::url::parse(url)) {
                _impl->url = std::move(*u);
            }
        }

        SGCL_INLINE_HOT string method() const noexcept {
            return _impl->method;
        }

        // The protocol the request came over, as Go's r.Proto: "HTTP/1.1",
        // "HTTP/1.0" or "HTTP/2.0"
        SGCL_INLINE_HOT string proto() const noexcept {
            return _impl->h2 ? "HTTP/2.0" : _impl->minor == 0 ? "HTTP/1.0" : "HTTP/1.1";
        }

        // The URL: the client's as parsed, or, on the server, the one the
        // request was for ("http://" + Host + the target, or the target in
        // absolute form). A client request whose URL did not parse has
        // none: invalid_argument (the send reports it as invalid_url first)
        SGCL_INLINE_HOT net::url url() const {
            auto u = _impl->url_of();
            if (!u) {
                throw invalid_argument("http::request: the URL does not parse");
            }
            return *u;
        }

        // A field of the head, "" when there is none
        SGCL_INLINE_HOT string header(const string& name) const noexcept {
            return _impl->fields.get(name);
        }

        SGCL_INLINE_HOT http::headers& headers() const noexcept {
            return _impl->fields;
        }

        SGCL_INLINE_HOT request& set_header(const string& name, const string& value) noexcept {
            _impl->fields.set(name, value);
            return *this;
        }

        SGCL_INLINE_HOT request& add_header(const string& name, const string& value) noexcept {
            _impl->fields.add(name, value);
            return *this;
        }

        // The body to send, with its Content-Length
        SGCL_INLINE_HOT request& set_body(const string& text) noexcept {
            _impl->body_kind = detail::RequestImpl::BodyKind::text;
            _impl->text = text;
            return *this;
        }

        SGCL_INLINE_HOT request& set_body(vector<byte> bytes) noexcept {   // by value: a vector's copy copies the bytes, its move does not
            _impl->body_kind = detail::RequestImpl::BodyKind::bytes;
            _impl->bytes = std::move(bytes);
            return *this;
        }

        // A stream, read when the request is sent: with its length given,
        // Content-Length; without, chunked. A stream cannot be sent twice,
        // so a request with one is neither retried nor redirected with 307
        SGCL_INLINE_HOT request& set_body(const io::reader& stream, optional<uint64_t> length = nullopt) noexcept {
            _impl->body_kind = detail::RequestImpl::BodyKind::stream;
            _impl->stream = stream;
            detail::set_optional(_impl->stream_length, length);
            return *this;
        }

        // A form, multipart/form-data (form.h): Content-Type set to the
        // form's (its boundary), the body's length taken when it is sent
        // (the files' sizes then), each file read as the body goes out. A
        // form is sent again as it is, so a request with one is retried and
        // redirected with 307 as one of text is; a file that cannot be read
        // is the send's error
        SGCL_INLINE_HOT request& set_body(const http::form& f) noexcept {
            _impl->body_kind = detail::RequestImpl::BodyKind::form;
            _impl->form = detail::FormAccess::state(f);
            _impl->fields.set("Content-Type", f.content_type());
            return *this;
        }

        // The server's side. {name} of the route's pattern, unescaped
        string path_value(const string& name) const noexcept {
            for (auto& p : _impl->path_values) {
                if (p.first == name) {
                    return p.second;
                }
            }
            return string();
        }

        // The first value of the name in the query, "" when there is none:
        // found in the query's text where the URL holds it, only that value
        // decoded (the pairs before it are not made into strings):
        // query_params::first without its limit, which a URL's query is
        // within
        SGCL_INLINE_HOT string query(const string& name) const noexcept {
            auto u = _impl->url_of();
            if (!u || !u->has_query()) {
                return string();
            }
            return net::detail::UrlAccess::query_first(*u, name);
        }

        // The value of the first cookie of the name in the Cookie fields
        SGCL_INLINE_HOT string cookie(const string& name) const noexcept {
            auto c = detail::request_cookie(_impl->fields, name.view());
            return c ? *c : string();
        }

        // Content-Length as the request gave it; nullopt for chunked or none
        SGCL_INLINE_HOT optional<uint64_t> content_length() const noexcept {
            return _impl->content_length;
        }

        SGCL_INLINE_HOT net::endpoint remote_endpoint() const noexcept {
            return _impl->remote;
        }

        // What the TLS handshake of the connection a server's request came
        // over settled (Go's r.TLS): the client's certificates (mTLS),
        // whether the session was resumed, the protocol; nullopt for a
        // request over plain TCP and for a client's request
        SGCL_INLINE_HOT optional<net::tls::state> tls() const noexcept {
            return net::tls::state_of(_impl->conn);
        }

        // A server's request: stopped when the server closes (close(), or
        // the end of a shutdown for this connection), a write of the
        // response fails or (HTTP/2) the connection ends. A client's: the
        // token set_stop gave, an empty one by default
        SGCL_INLINE_HOT async::stop_token stop() const noexcept {
            return _impl->stop;
        }

        // A client's request cancelled by the token (Go's
        // NewRequestWithContext): a stop before the send, or while it
        // waits for the connection, the head or the body, ends it with
        // ECANCELED (the connection closed, an HTTP/2 stream reset), the
        // reads of the body after it too. A watch of the token goes with
        // the exchange to the end of its body
        SGCL_INLINE_HOT request& set_stop(const async::stop_token& token) noexcept {
            _impl->stop = token;
            return *this;
        }

        // The URL to send to, parsed now; its error reported by the send
        // (net::errc::invalid_url), as the constructor's
        SGCL_INLINE_HOT request& set_url(const string& url) noexcept {
            _impl->url_text = url;
            _impl->url.reset();
            if (auto u = net::url::parse(url)) {
                _impl->url.emplace(std::move(*u));
            }
            _impl->url_later = false;
            return *this;
        }

        // The whole body as text, bounded by the server's max_body_bytes.
        // text() blocks the thread (the reading runs on the scheduler and
        // the thread waits for it: never from a worker); a handler that is
        // a task writes `co_await req.async_text()`
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

        // The body as a stream (reads bounded by max_body_bytes); an empty
        // stream for a request without a body
        SGCL_INLINE_HOT io::reader body() const noexcept {
            if (!_impl->body) {
                return io::reader(detail::no_body);
            }
            return io::reader(_impl->body);
        }

        // The body's parts, read as they come (multipart.h): a reader over
        // body() with the Content-Type's boundary, within the limits given
        // (1000 parts, 16 KB a part's head by default) and the server's
        // max_body_bytes; net::errc::not_multipart for a Content-Type that
        // is not multipart/..., malformed_multipart for one without a
        // boundary
        SGCL_INLINE_HOT expected<multipart_reader, io::error> multipart() const noexcept {
            return multipart(multipart_reader::limits());
        }

        expected<multipart_reader, io::error> multipart(const multipart_reader::limits& l) const noexcept {
            auto boundary = detail::multipart_boundary(_impl->fields.get("Content-Type").view());
            if (!boundary) {
                return unexpected(net::detail::net_error(boundary.error(), "multipart", _impl->fields.get("Content-Type")));
            }
            return multipart_reader(body(), *boundary, l);
        }

        // The fields of a form the body holds, in their order: of
        // application/x-www-form-urlencoded, or the parts of
        // multipart/form-data that are not files (a file's part is read
        // past: multipart() reads files); none for another Content-Type or
        // no body. The values together at most 10 MB, as Go's
        // ParseMultipartForm keeps them (net::errc::body_too_large past it),
        // within the server's max_body_bytes. The URL's query is query().
        // form() blocks the thread (the reading runs on the scheduler: never
        // from a worker); a handler that is a task writes
        // `co_await req.async_form()`
        SGCL_INLINE_HOT expected<net::query_params, io::error> form() const {
            return _co_form(_impl).wait();
        }

        SGCL_INLINE_HOT async::task<expected<net::query_params, io::error>> async_form() const noexcept {
            return _co_form(_impl);
        }

        // The trailers of a chunked body, once it has been read to its end
        SGCL_INLINE_HOT http::headers trailers() const noexcept {
            return _impl->body ? _impl->body->trailers() : http::headers();
        }

    private:
        friend struct detail::RequestAccess;

        SGCL_INLINE_HOT explicit request(const tracked_ptr<detail::RequestImpl>& impl) noexcept
        : _impl(impl) {
        }

        tracked_ptr<detail::RequestImpl> _impl;

        static async::task<expected<vector<byte>, io::error>> _co_bytes(tracked_ptr<detail::RequestImpl> impl) noexcept {
            if (!impl->body) {
                co_return vector<byte>();
            }
            co_return co_await impl->body->read_everything();
        }

        static async::task<expected<net::query_params, io::error>> _co_form(tracked_ptr<detail::RequestImpl> impl) noexcept {
            auto type = detail::media_type(impl->fields.get("Content-Type").view());
            if (type == "application/x-www-form-urlencoded") {
                expected<string, io::error> text = string();
                if (impl->body) {
                    text = co_await impl->body->read_text();
                }
                if (!text) {
                    co_return unexpected(text.error());
                }
                if (text->size() > detail::FormValuesMax) {
                    co_return unexpected(net::detail::net_error(net::errc::body_too_large, "form"));
                }
                co_return net::query_params::parse(*text);
            }
            net::query_params out;
            if (type != "multipart/form-data") {
                co_return out;
            }
            request self(impl);
            auto m = self.multipart();
            if (!m) {
                co_return unexpected(m.error());
            }
            tracked_ptr block = make_tracked<detail::MultipartBlock>();   // managed: the body's reads may run on the pool
            size_t total = 0;
            for (;;) {
                auto p = co_await m->async_next();
                if (!p) {
                    co_return unexpected(p.error());
                }
                if (!*p) {
                    co_return out;
                }
                if (!(*p)->filename.empty() || (*p)->name.empty()) {
                    continue;   // a file, or a part of no field: read past by the next
                }
                std::string value;
                for (;;) {
                    auto n = co_await m->async_read(slice<byte>(block, block->data(), block->size()));
                    if (!n) {
                        co_return unexpected(n.error());
                    }
                    if (*n == 0) {
                        break;
                    }
                    total += *n;
                    if (total > detail::FormValuesMax) {
                        co_return unexpected(net::detail::net_error(net::errc::body_too_large, "form"));
                    }
                    value.append(reinterpret_cast<const char*>(block->data()), *n);
                }
                if (auto added = out.add((*p)->name, string(std::string_view(value))); !added) {
                    co_return unexpected(added.error());
                }
            }
        }

        static async::task<expected<string, io::error>> _co_text(tracked_ptr<detail::RequestImpl> impl) noexcept {
            if (!impl->body) {
                co_return string();
            }
            co_return co_await impl->body->read_text();   // straight into the string
        }

    };

    namespace detail {
        struct RequestAccess {
            SGCL_INLINE_HOT static request make(const tracked_ptr<RequestImpl>& impl) noexcept {
                return request(impl);
            }

            SGCL_INLINE_HOT static const tracked_ptr<RequestImpl>& impl(const request& r) noexcept {
                return r._impl;
            }
        };
    }
}
