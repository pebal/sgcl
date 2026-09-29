//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response_writer.h"
#include "status.h"
#include "detail/router.h"
#include "detail/server_state.h"
#include "detail/h2/serve.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../async/stop_token.h"
#include "../../async/wait_group.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <type_traits>

namespace sgcl::net::http {
    class server;

    namespace detail {
        // A TLS config's ALPN list as serve_tls() uses it (Go's
        // adjustNextProtos): h2 added at the end when on and absent, or
        // taken out when off; http/1.1 added at the end when absent
        inline net::tls::config adjusted_alpn(net::tls::config c, bool http2) {
            vector<string> out;
            bool h2 = false;
            bool h1 = false;
            for (auto& p : c.alpn) {
                if (p == "h2") {
                    if (!http2) {
                        continue;
                    }
                    h2 = true;
                } else if (p == "http/1.1") {
                    h1 = true;
                }
                out.push_back(p);
            }
            if (http2 && !h2) {
                out.push_back(string("h2"));
            }
            if (!h1) {
                out.push_back(string("http/1.1"));
            }
            c.alpn = std::move(out);
            return c;
        }

        // One exchange: the head read and checked, the handler run, the
        // response finished and the body drained; whether the connection
        // goes on
        inline async::task<Next> serve_one(tracked_ptr<ServerImpl> s, tracked_ptr<ServerSettings> cfg, tracked_ptr<ServerConn> node, tracked_ptr<Wire> wire) {
            auto& c = node->c;
            auto start = sgcl::clock::now();
            auto read_limit = cfg->read_timeout > duration::zero() ? start + cfg->read_timeout : time_point();
            c.set_read_deadline(earlier(cfg->read_header_timeout > duration::zero() ? start + cfg->read_header_timeout : time_point(), read_limit));
            auto head = co_await wire->read_head(cfg->max_header_bytes);
            if (!head) {
                if (head.error().code() == net::errc::header_too_large) {
                    co_await send_refusal(c, 431);
                }
                co_return Next::end;
            }
            if (!*head) {
                co_return Next::end;
            }
            tracked_ptr req = make_tracked<RequestImpl>();
            req->head = **head;
            RequestLine line;
            BodyFraming framing;
            int refused = check_request_head(req->head, line, req->fields, framing);
            auto host = HeadersAccess::find(req->fields, "host");
            if (refused) {
                // the method is known when the request line was read (a
                // refusal for the framing or Host): a HEAD gets no body
                bool head_request = line.method_size == 4 && req->head.view().substr(line.method_at, 4) == "HEAD";
                co_await send_refusal(c, refused, head_request);
                co_return Next::end;
            }
            req->method = method_name(req->head.view().substr(line.method_at, line.method_size));
            req->target = req->head.as_slice(line.target_at, line.target_size);
            req->minor = line.minor;
            std::string_view method = req->method.view();
            std::string_view target = req->target.view();
            std::string_view host_text = host ? *host : std::string_view();
            if (host) {   // a slice of the head, for the url made later
                req->host = req->head.as_slice(size_t(host->data() - req->head.data()), host->size());
            }
            string route_path;   // the full parse's path, when the fast one refused
            std::string_view path;
            if (auto fast = net::detail::origin_form_path(host_text.empty() ? std::string_view("localhost") : host_text, target)) {
                path = *fast;   // the url itself made only if the handler asks (RequestImpl::url_of)
                req->url_later = true;
            } else if (target.front() == '/') {
                if (auto u = net::url::parse(string("http://" + (host_text.empty() ? std::string("localhost") : std::string(host_text)) + std::string(target)))) {
                    req->url = std::move(*u);
                }
            } else if (target == "*" || method == "CONNECT") {
                if (auto u = net::url::parse(string("http://" + (method == "CONNECT" ? std::string(target) : (host_text.empty() ? std::string("localhost") : std::string(host_text))) + "/"))) {
                    req->url = std::move(*u);
                }
            } else {
                if (auto u = net::url::parse(string(req->target))) {
                    req->url = std::move(*u);
                }
            }
            if (!req->url_later) {
                if (!req->url) {
                    co_await send_refusal(c, 400, method == "HEAD");
                    co_return Next::end;
                }
                route_path = req->url->path();
                path = route_path.view();
            }
            bool expect_continue = false;
            if (auto expect = HeadersAccess::find(req->fields, "expect")) {
                if (iequal(trim_ows(*expect), "100-continue") && line.minor == 1) {
                    expect_continue = true;
                } else {
                    co_await send_refusal(c, 417, method == "HEAD");
                    co_return Next::end;
                }
            }
            if (framing.kind == Framing::length) {
                req->content_length.emplace(framing.length);
                if (cfg->max_body_bytes && framing.length > cfg->max_body_bytes) {
                    co_await send_refusal(c, 413, method == "HEAD");
                    co_return Next::end;
                }
            } else if (framing.kind == Framing::none) {
                if (HeadersAccess::count(req->fields, "content-length")) {
                    req->content_length.emplace(0);
                }
            }
            c.set_read_deadline(read_limit);
            c.set_write_deadline(cfg->write_timeout > duration::zero() ? sgcl::clock::now() + cfg->write_timeout : time_point());

            tracked_ptr w = make_tracked<WriterImpl>();
            w->wire = wire;
            w->head_request = method == "HEAD";
            w->request_minor = line.minor;
            if (line.minor == 1) {
                w->close_after = HeadersAccess::has_token(req->fields, "connection", "close");
            } else {
                w->keep_alive_asked = HeadersAccess::has_token(req->fields, "connection", "keep-alive");
                w->close_after = !w->keep_alive_asked;
            }
            if (s->shutting_down.load()) {
                w->close_after = true;
            }
            if (framing.kind != Framing::none) {   // a request without a body has no Body: body() hands out an empty stream
                req->body = make_tracked<Body>(wire, framing, cfg->max_body_bytes, false);
            }
            if (expect_continue && framing.kind != Framing::none) {
                net::connection conn = c;
                req->body->set_before_first_read([conn]() { return send_continue(conn); });
            }
            req->remote = c.remote_endpoint();
            req->stop = node->stop.token();

            auto r = RequestAccess::make(req);
            auto writer = WriterAccess::make(w);
            try {
                if (auto t = dispatch(*s, req, r, writer, method, host_text, path)) {
                    co_await *t;
                }
            } catch (const std::exception& e) {
                handler_threw(*cfg, *req, *w, writer, e.what());
            } catch (...) {
                handler_threw(*cfg, *req, *w, writer, nullptr);
            }
            if (w->hijacked) {
                co_return Next::hijacked;
            }
            // a body read past the limit, or broken, with nothing written
            // for it: the server answers 413 or 400
            auto& body = req->body;
            if (body && body->failed()) {
                w->close_after = true;
                if (!w->head_sent && !w->touched) {
                    writer.error(body->error_status() == 413 ? status::content_too_large : status::bad_request);
                }
            }
            // what the handler left of the body is read after the response,
            // up to 256 KB (Go's bound); a declared rest past that closes the
            // connection, and the response says so
            constexpr uint64_t DrainBound = 256 * 1024;
            if (body && !body->done() && !body->failed() && framing.kind == Framing::length && framing.length - body->read_total() > DrainBound) {
                w->close_after = true;
            }
            if (s->shutting_down.load()) {
                w->close_after = true;
            }
            // fields that cannot be written (a value of a user's with CR or
            // LF would split the response): the handler's mistake, answered
            // as a throw is, with a 500 of the server's own fields, and the
            // connection ends. The error a flush of the handler's gave back
            // is dropped with them: nothing of that head was sent
            if (!w->head_sent && !w->hijacked) {
                if (auto e = invalid_field(w->fields)) {
                    cfg->report(string("a handler of ") + req->method + " " + string(req->target) + " wrote " + *e);
                    w->close_after = true;
                    w->failed.reset();
                    w->fields = http::headers();
                    writer.error(status::internal_server_error);
                }
            }
            const uint64_t body_bytes = w->body_bytes();   // before the finish gives the blocks back
            expected<void, io::error> sent;
            if (auto rest = w->finish_start(sent)) {   // a frame only when the connection would wait
                sent = co_await *rest;
            }
            log_access(*cfg, *req, *w, body_bytes, path, line.minor == 1 ? std::string_view("HTTP/1.1") : std::string_view("HTTP/1.0"), start);
            if (!sent) {
                node->stop.request_stop();
                co_return Next::end;
            }
            bool drained = !body || (body->done() && !body->failed());
            if (body && !body->done() && !body->failed() && !w->close_after) {
                drained = co_await body->discard(DrainBound);
            }
            if (!drained) {
                co_await linger(c);
                co_return Next::end;
            }
            co_return !w->close_after ? Next::again : Next::end;
        }

        inline async::task<> serve_connection(tracked_ptr<ServerImpl> s, tracked_ptr<ServerSettings> cfg, net::connection c) {
            tracked_ptr node = make_tracked<ServerConn>(c, s->closing.token());
            s->link(node);
            tracked_ptr wire = make_tracked<Wire>(c);
            bool hijacked = false;
            // HTTP/2: chosen by ALPN on TLS, or the client's preface first on
            // a plain connection when h2c is on (by prior knowledge, RFC
            // 9113 §3.3); anything else is HTTP/1.1
            bool h2 = false;
            if (cfg->http2) {
                if (auto st = net::tls::state_of(c); st && st->alpn == "h2") {
                    h2 = true;
                }
            }
            bool first = true;
            for (;;) {
                if (h2) {
                    co_await h2::serve(s, cfg, node, wire);
                    break;
                }
                if (wire->buffered() == 0) {
                    wire->trim_out();   // a large response's room not kept while the connection waits
                    node->state.store(ServerConn::idle);
                    if (s->shutting_down.load()) {
                        break;
                    }
                    c.set_read_deadline(deadline_after(cfg->idle_timeout));
                    expected<size_t, io::error> r = size_t(0);
                    for (;;) {   // wire->fill() in this frame (Wire::try_fill)
                        auto step = wire->try_fill();
                        if (step.done) {
                            r = std::move(step.result);
                            break;
                        }
                        if (step.slow) {
                            r = co_await wire->fill();
                            break;
                        }
                        if (auto ready = co_await step.ready; !ready) {
                            r = fail(ready);
                            break;
                        }
                    }
                    if (!r || *r == 0) {
                        break;
                    }
                    int expected = ServerConn::idle;
                    if (!node->state.compare_exchange_strong(expected, ServerConn::active)) {
                        break;   // shutdown took it while idle
                    }
                } else {
                    node->state.store(ServerConn::active);
                }
                if (first && cfg->h2c && !net::tls::state_of(c)) {
                    // the preface's first bytes decide (a request of HTTP/1.1
                    // never begins "PRI * HTTP/2.0"); the machine reads the rest
                    const std::string_view preface(h2::Preface, h2::PrefaceSize);
                    for (;;) {
                        auto v = wire->view();
                        const size_t k = std::min(v.size(), preface.size());
                        if (v.substr(0, k) != preface.substr(0, k)) {
                            break;
                        }
                        if (k == preface.size()) {
                            h2 = true;
                            break;
                        }
                        auto more = co_await wire->fill();   // a preface cut short: the rest decides
                        if (!more || *more == 0) {
                            break;
                        }
                    }
                    if (h2) {
                        continue;
                    }
                }
                first = false;
                auto next = co_await serve_one(s, cfg, node, wire);
                if (next == Next::hijacked) {
                    hijacked = true;
                    break;
                }
                if (next == Next::end) {
                    break;
                }
            }
            // a hijacked connection is the handler's now; any other ends here
            if (!hijacked) {
                (void)co_await c.async_close();
            }
            s->unlink(node);
            s->running.done();
            co_return;
        }
    }

    // An HTTP/1.1 server with routes (Go's http.Server and ServeMux in
    // one). A handle of one word: copies share the routes and the
    // connections. The fields are read when serve() is called.
    //
    // A handler is a function of (request, response_writer): returning
    // void, for a handler that never waits (write() only buffers), or
    // async::task<>, for one that reads the body or calls other services.
    // Routes are patterns of Go 1.22 (detail/router.h): "GET /users/{id}",
    // "POST /upload", "/static/", "example.com/", "/{$}". A pattern that
    // conflicts with one already there is invalid_argument.
    //
    // A connection runs as one task: the head read under
    // read_header_timeout (from its first byte; idle_timeout before it),
    // checked by the rules of detail/parser.h, the handler, the response
    // finished, what the handler left of the body read (up to 256 KB,
    // else the connection closed), and the next request. Requests that
    // come pipelined are served in turn. A handler that throws gets a 500
    // (when nothing was sent yet), on_error hears of it, and the connection
    // ends; the server goes on.
    class server {
    public:
        server()
        : _impl(make_tracked<detail::ServerImpl>()) {
        }

        template<class Handler>
        server& route(const string& pattern, Handler handler) {
            auto h = _handler(std::move(handler));
            std::lock_guard<std::mutex> g(_impl->lock);
            size_t i = _impl->routes.add(pattern.view());
            if (_impl->handlers.size() <= i) {
                _impl->handlers.resize(i + 1);
            }
            _impl->handlers[i] = std::move(h);
            return *this;
        }

        // What a request no route matches gets; a 404 text/plain by default
        template<class Handler>
        server& not_found(Handler handler) {
            auto h = _handler(std::move(handler));
            std::lock_guard<std::mutex> g(_impl->lock);
            _impl->not_found = std::move(h);
            return *this;
        }

        // Listens on the address (":8080") and serves until shutdown() or
        // close(): then net::errc::server_closed, as Go's ErrServerClosed.
        // serve() blocks the thread (main's, as Go's ListenAndServe), the
        // connections served on the scheduler; in a task `co_await
        // s.async_serve(":8080")`
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const {
            return _co_serve_address(_impl, _settings(), address);
        }

        // Listens over TLS on the address with the config and serves (Go's
        // ListenAndServeTLS). The config's ALPN list is completed as Go
        // completes NextProtos: "h2" added when http2 is on and the list
        // has none ("h2" taken out when http2 is off), "http/1.1" added
        // when missing, the protocols already there kept in their order.
        // The server's order is the preference (tls: the first of ours the
        // client offers), so {"http/1.1"} given stays HTTP/1.1 for a client
        // offering both. A listener of the program's own (serve(listener))
        // keeps the ALPN it was made with.
        expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const {
            return async_serve_tls(address, c).wait();
        }

        async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const {
            return _co_serve_tls(_impl, _settings(), address, detail::adjusted_alpn(c, http2));
        }

        // The connections of a listener the program made
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const {
            return _co_serve(_impl, _settings(), l);
        }

        // Gracefully: the listeners closed, the idle connections closed,
        // the active ones ending after their current response (which says
        // Connection: close); returns when all of them have. A limit on it
        // is `co_await async::with_timeout(s.async_shutdown(), 10s)`, then close().
        void shutdown() const {
            async_shutdown().wait();
        }

        async::task<> async_shutdown() const {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed, the reads and
        // writes in progress ended (io::errc::closed), every request's
        // stop() stopped
        void close() const {
            _impl->shutting_down.store(true);
            _impl->closed.store(true);
            _impl->close_listeners();
            _impl->closing.request_stop();
            for (auto& n : _impl->snapshot()) {
                (void)n->c.close();
            }
        }

        duration read_header_timeout = std::chrono::seconds(10);   // the whole head, from its first byte (Slowloris); Go's is none
        duration read_timeout = duration::zero();                  // the head and the body; zero: none
        duration write_timeout = duration::zero();                 // the response, from the end of the head
        duration idle_timeout = std::chrono::seconds(120);         // for the next request on a kept connection
        size_t max_header_bytes = 32 * 1024;                       // past it: 431
        uint64_t max_body_bytes = uint64_t(32) << 20;              // past it: 413; zero: none
        function<void(const string&)> on_error;                    // a handler's exception, an accept's failure; a line on stderr by default
        // HTTP/2 (RFC 9113) for a connection whose TLS agreed on "h2" by
        // ALPN: the listener's tls::config names it (alpn {"h2", "http/1.1"});
        // the same handlers, request::proto() "HTTP/2.0"
        bool http2 = true;
        // HTTP/2 on a plain connection by prior knowledge (h2c): the
        // client's preface recognised by its first bytes, HTTP/1.1 served
        // on the same port as before (tests, a network of one's own)
        bool h2c = false;
        uint32_t max_concurrent_streams = 250;                     // HTTP/2: the streams a client may have open, the handlers of a connection (Go: 250)

        // A record of every exchange, when the response is finished, to
        // the logger: `method`, `path`, `proto`, `status`, `bytes` (of the
        // body), `duration` (from the request's first byte), `remote`,
        // `user_agent`, and `request_id` when the request has an
        // X-Request-ID; at info, at error for a 5xx. Nothing managed per
        // request. access_log() alone is the default logger buffered (a
        // batch per worker, DESIGN 283); a logger given is used as it is
        // (options::buffered in it for the batches). Read when serve() is called,
        // as the fields are
        server& access_log(const slog::logger& log) {
            _access_log.emplace(log);
            return *this;
        }

        server& access_log() {
            _access_log.emplace(slog::detail::buffered_copy(slog::default_logger()));
            return *this;
        }

    private:
        template<class H>
        static detail::Handler _handler(H h) {
            detail::Handler out;
            using R = std::invoke_result_t<H&, request, response_writer>;
            if constexpr (std::is_void_v<R>) {
                out.plain = function<void(request, response_writer)>(std::move(h));
            } else {
                static_assert(std::is_same_v<R, async::task<>>, "a handler returns void or async::task<>");
                out.awaited = function<async::task<>(request, response_writer)>(std::move(h));
            }
            return out;
        }

        tracked_ptr<detail::ServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::ServerSettings>();
            cfg->read_header_timeout = read_header_timeout;
            cfg->read_timeout = read_timeout;
            cfg->write_timeout = write_timeout;
            cfg->idle_timeout = idle_timeout;
            cfg->max_header_bytes = max_header_bytes ? max_header_bytes : 1;
            cfg->max_body_bytes = max_body_bytes;
            cfg->http2 = http2;
            cfg->h2c = h2c;
            cfg->max_concurrent_streams = max_concurrent_streams ? max_concurrent_streams : 1;
            cfg->on_error = on_error;
            cfg->access_log = _access_log;
            return cfg;
        }

        tracked_ptr<detail::ServerImpl> _impl;
        optional<slog::logger> _access_log;

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, net::listener l) {
            if (impl->shutting_down.load()) {
                (void)l.close();
                co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", l.local_endpoint().to_string()));
            }
            {
                std::lock_guard<std::mutex> g(impl->lock);
                impl->listeners.push_back(l);
            }
            if (impl->shutting_down.load()) {
                (void)l.close();
            }
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    if (impl->shutting_down.load()) {
                        co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", l.local_endpoint().to_string()));
                    }
                    cfg->report(string("accept: ") + c.error().message());
                    co_return io::detail::fail(c);
                }
                impl->running.add();
                async::go(detail::serve_connection(impl, cfg, *c));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_tls(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, string address,
                                                                    net::tls::config c) {
            auto l = co_await net::tls::async_listen(address, c);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, string address) {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::ServerImpl> impl) {
            impl->shutting_down.store(true);
            impl->close_listeners();
            for (auto& n : impl->snapshot()) {
                int expected = detail::ServerConn::idle;
                if (n->state.compare_exchange_strong(expected, detail::ServerConn::closed)) {
                    (void)co_await n->c.async_close();
                }
            }
            co_await impl->running;
        }
    };
}
