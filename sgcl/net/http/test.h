//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "client.h"
#include "headers.h"
#include "request.h"
#include "response_writer.h"
#include "server.h"
#include "detail/test_certificate.h"
#include "../connection.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/weak_ptr.h"
#include "../../time/datetime.h"

#include <atomic>
#include <mutex>
#include <string>
#include <system_error>
#include <type_traits>

// What a test of HTTP code needs (Go's net/http/httptest): a server on a
// port of the loopback in one line (test_server), with TLS on a
// certificate made at its start and HTTP/2 when asked, a client made for
// it, the requests it received; a response written by a handler without
// the network (response_recorder); a request as a server hands one to a
// handler (test_request).
namespace sgcl::net::http {
    namespace detail {
        template<class H>
        concept TestHandler = std::is_invocable_v<H&, request, response_writer>;

        struct TestServerState {
            http::server server;
            http::client client;
            net::endpoint endpoint;
            string url;
            optional<crypto::x509::certificate> certificate;
            std::mutex lock;
            vector<request> requests;
            async::task<expected<void, io::error>> serving;
            std::atomic<bool> closed = {false};
        };
    }

    // A server of the program's handler (or routes) on a port of the
    // loopback, started in the constructor and closed by close() or the
    // destructor (Go's httptest.Server). Its URL, a client made for it (no
    // proxy, TLS trusting its certificate, HTTP/2 as it serves), what it
    // received. A guard of its scope: moved, never copied.
    class test_server {
    public:
        struct options {
            // https: a test CA and a leaf it signed (localhost, 127.0.0.1,
            // ::1) made now, the client trusting the CA
            bool tls = false;
            // HTTP/2: by ALPN over TLS, by prior knowledge (h2c) without;
            // HTTP/1.1 is served beside it either way
            bool http2 = false;
            // every request kept for requests(), as it reached its route
            bool keep_requests = false;
        };

        // A server of the one handler (every method and path), a function
        // of (request, response_writer) returning void or async::task<>
        template<detail::TestHandler Handler>
        explicit test_server(Handler handler) {
            http::server s;
            s.route("/", std::move(handler));
            _start(s, options());
        }

        template<detail::TestHandler Handler>
        test_server(Handler handler, const options& o) {
            http::server s;
            s.route("/", std::move(handler));
            _start(s, o);
        }

        // The routes of a server the program made; its fields (timeouts,
        // limits, on_error, http2) as they are, but http2 and h2c, which
        // options decide
        explicit test_server(const http::server& s) {
            _start(s, options());
        }

        test_server(const http::server& s, const options& o) {
            _start(s, o);
        }

        test_server(const test_server&) = delete;
        test_server& operator=(const test_server&) = delete;

        // The moved-from server is closed for nothing: url() empty, close()
        // does nothing
        SGCL_INLINE_HOT test_server(test_server&& other) noexcept
        : _s(std::move(other._s)) {
            other._s = nullptr;
        }

        test_server& operator=(test_server&& other) noexcept {
            if (this != &other) {
                _close_now();
                _s = std::move(other._s);
                other._s = nullptr;
            }
            return *this;
        }

        // Closed at once (close() without its wait): the listener and the
        // connections closed, every request's stop() stopped
        ~test_server() {
            _close_now();
        }

        // "http://127.0.0.1:port", "https://127.0.0.1:port"; "" for a
        // moved-from server
        SGCL_INLINE_HOT string url() const noexcept {
            return _s ? _s->url : string();
        }

        // The address it listens on
        SGCL_INLINE_HOT net::endpoint endpoint() const noexcept {
            return _s ? _s->endpoint : net::endpoint();
        }

        // A client for it: no proxy, TLS that trusts the server's CA,
        // HTTP/2 (h2 by ALPN, or h2c) when the server speaks it. The same
        // client each time (a copy shares its pool); its idle connections
        // are closed with the server. Any URL may be asked through it
        SGCL_INLINE_HOT http::client client() const {
            _check();
            return _s->client;
        }

        // The server serving: its routes may be added to while it runs
        SGCL_INLINE_HOT http::server server() const {
            _check();
            return _s->server;
        }

        // The CA the server's certificate leads to (what client() trusts);
        // nullopt without TLS
        SGCL_INLINE_HOT optional<crypto::x509::certificate> certificate() const {
            _check();
            return _s->certificate;
        }

        // The requests received in their order, as they reached their
        // routes (options::keep_requests; none otherwise): their heads, and
        // their bodies as far as the handlers read them
        vector<request> requests() const {
            _check();
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->requests;
        }

        // The connections of the clients closed now, the server going on
        // (Go's CloseClientConnections): a request in progress sees its
        // connection end
        void close_client_connections() const {
            _check();
            auto& impl = *detail::ServerAccess::impl(_s->server);
            for (auto& n : impl.snapshot()) {
                (void)n->c.close();
            }
        }

        // Closed: the listener, every connection and the client's idle
        // ones, every request's stop() stopped, and the handlers waited
        // for (Go's Close). A second close, or one of a moved-from server,
        // does nothing. close() blocks the thread (never from a worker); in
        // a task `co_await ts.async_close()`
        void close() {
            if (_close_now()) {
                (void)_s->serving.wait();
                _s->server.shutdown();
            }
        }

        async::task<> async_close() {
            if (_close_now()) {
                return _co_wait(_s);
            }
            return _co_none();
        }

    private:
        tracked_ptr<detail::TestServerState> _s;

        void _check() const {
            if (!_s) {
                throw invalid_argument("http::test_server: moved from");
            }
        }

        // The server closed now, once: whether this call did it
        bool _close_now() noexcept {
            if (!_s || _s->closed.exchange(true)) {
                return false;
            }
            _s->client.close_idle_connections();
            _s->server.close();
            return true;
        }

        static async::task<> _co_wait(tracked_ptr<detail::TestServerState> s) noexcept {
            (void)co_await s->serving;
            co_await s->server.async_shutdown();
        }

        static async::task<> _co_none() noexcept {
            co_return;
        }

        void _start(const http::server& routes, const options& o) {
            _s = make_tracked<detail::TestServerState>();
            http::server s = routes;
            s.http2 = o.http2;
            s.h2c = o.http2 && !o.tls;
            http::client c;
            c.proxy = http::proxy();
            c.http2 = o.http2;
            c.h2c = o.http2 && !o.tls;
            expected<net::listener, io::error> l = net::tcp::listen("127.0.0.1:0");
            if (o.tls) {
                auto made = detail::make_test_certificates(time::now().unix());
                auto id = net::tls::identity::from_pem(made.leaf_pem + made.ca_pem, made.key_pem.as_slice());
                if (!id) {
                    throw std::system_error(id.error().code(), std::string(id.error().message().view()));
                }
                net::tls::config server_tls;
                server_tls.identities.push_back(*id);
                server_tls.alpn = o.http2 ? vector<string>{string("h2"), string("http/1.1")} : vector<string>{string("http/1.1")};
                l = net::tls::listen("127.0.0.1:0", server_tls);
                auto ca = crypto::x509::certificate::from_pem(made.ca_pem);
                if (ca) {
                    _s->certificate.emplace(*ca);
                }
                c.tls.roots = crypto::x509::certificate_pool::from_pem(made.ca_pem);
            }
            if (!l) {
                throw std::system_error(l.error().code(), std::string(l.error().message().view()));
            }
            _s->endpoint = l->local_endpoint();
            _s->url = string::concat(o.tls ? "https://" : "http://", _s->endpoint.to_string());
            _s->server = s;
            _s->client = c;
            if (o.keep_requests) {
                tracked_ptr<detail::TestServerState> state = _s;
                weak_ptr<detail::TestServerState> weak(state);   // the serving does not keep the test server
                _s->serving = detail::ServerAccess::serve_observed(s, *l, [weak](const request& r) {
                    if (auto st = weak.lock()) {
                        std::lock_guard<std::mutex> g(st->lock);
                        st->requests.push_back(r);
                    }
                });
            } else {
                _s->serving = s.async_serve(*l);
            }
            (void)_s->serving.spawn();
        }
    };

    // A response written by a handler with no network (Go's
    // httptest.ResponseRecorder): writer() is a response_writer whose
    // flushes and end send nothing and keep everything. What the handler
    // wrote is read back: the status, the fields (as the head went at the
    // first flush, or as they stand), the body (flushed and buffered), the
    // trailers, the informational responses, the count of flushes. A
    // handle of one word: a copy is the same recorder.
    class response_recorder {
    public:
        SGCL_INLINE_HOT response_recorder() noexcept
        : _w(make_tracked<detail::WriterImpl>()) {
            _w->record = make_tracked<detail::RecordState>();
        }

        // The writer to hand to a handler: `handler(req, rec.writer())`, a
        // task's `.wait()` on a thread
        SGCL_INLINE_HOT response_writer writer() const noexcept {
            return detail::WriterAccess::make(_w);
        }

        // The request through the server's routes as the server routes it
        // (its path values, a redirect, 404, 405), the handler run to its
        // end; what a handler throws comes out of serve(). serve() blocks
        // the thread (never from a worker); in a task `co_await
        // rec.async_serve(s, req)`
        void serve(const http::server& s, const request& r) const {
            async_serve(s, r).wait();
        }

        SGCL_INLINE_HOT async::task<> async_serve(const http::server& s, const request& r) const noexcept {
            return _co_serve(detail::ServerAccess::impl(s), r, writer());
        }

        // The status written, 200 unless set
        SGCL_INLINE_HOT int status() const noexcept {
            return _w->record->head_sent ? _w->record->status : _w->status;
        }

        // The fields as the head went at the first flush, or as the writer
        // holds them when nothing was flushed
        SGCL_INLINE_HOT http::headers headers() const noexcept {
            return _w->record->head_sent ? _w->record->fields : _w->fields;
        }

        // The header named, "" when there is none
        SGCL_INLINE_HOT string header(const string& name) const noexcept {
            return headers().get(name);
        }

        // Everything written to the body: what the flushes took and what
        // is still buffered
        string body() const {
            auto& rec = *_w->record;
            _w->take_file();
            std::string out(reinterpret_cast<const char*>(rec.body.data()), rec.body.size());
            if (!_w->head_request && !_w->bodiless(status())) {
                _w->body.copy_to(out);
            }
            return string(std::string_view(out));
        }

        // The trailers set
        SGCL_INLINE_HOT http::headers trailers() const noexcept {
            return _w->trailers;
        }

        // The informational responses sent (send_informational), in order
        SGCL_INLINE_HOT vector<pair<int, http::headers>> informational() const noexcept {
            return _w->record->informational;
        }

        // How many flushes the handler made
        SGCL_INLINE_HOT int flushes() const noexcept {
            return _w->record->flushes;
        }

    private:
        tracked_ptr<detail::WriterImpl> _w;

        static async::task<> _co_serve(tracked_ptr<detail::ServerImpl> s, request r, response_writer w) {
            auto& req = detail::RequestAccess::impl(r);
            auto u = req->url_of();
            string path = u ? u->path() : string("/");
            string host = req->fields.get("Host");
            if (host.empty() && u) {
                host = u->host();
            }
            if (auto t = detail::dispatch(*s, req, r, w, req->method.view(), host.view(), path.view())) {
                co_await *t;
            }
        }
    };

    // A request as a server hands one to its handler (Go's
    // httptest.NewRequest): the method, the target (a path with its query,
    // "/users?id=7", the host then example.com; or a URL, whose host it is),
    // and the body, read through text(), bytes() and body() as a received
    // one is, with its Content-Length. HTTP/1.1, from 192.0.2.1:1234, a Host
    // field, a stop() never stopped. invalid_argument for a target that
    // does not parse
    inline request test_request(const string& method, const string& target, const string& body = string()) {
        tracked_ptr req = make_tracked<detail::RequestImpl>();
        req->method = method;
        auto t = target.view();
        const bool absolute = t.starts_with("http://") || t.starts_with("https://");
        auto u = absolute ? net::url::parse(target) : net::url::parse(string::concat("http://example.com", t.starts_with("/") ? "" : "/", t));
        if (!u || t.empty()) {
            throw invalid_argument("http::test_request: the target does not parse");
        }
        req->url = *u;
        req->url_text = target;
        req->fields.add("Host", u->host());
        req->remote = net::endpoint::parse("192.0.2.1:1234").value();
        req->minor = 1;
        req->stop = async::stop_source().token();
        if (!body.empty()) {
            auto ends = net::connection::in_memory();
            tracked_ptr wire = make_tracked<detail::Wire>(ends.first);
            wire->preload(body.view());
            req->body = make_tracked<detail::Body>(wire, detail::BodyFraming{detail::Framing::length, body.size()}, 0, false);
            req->content_length.emplace(body.size());
            req->fields.add("Content-Length", string(std::to_string(body.size())));
        }
        return detail::RequestAccess::make(req);
    }
}
