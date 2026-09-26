//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response_writer.h"
#include "status.h"
#include "detail/router.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
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
        struct Handler {
            function<void(request, response_writer)> plain;
            function<async::task<>(request, response_writer)> awaited;
        };

        // What a serve() runs with: the server's fields as they were when
        // it was called
        struct ServerSettings {
            duration read_header_timeout;
            duration read_timeout;
            duration write_timeout;
            duration idle_timeout;
            size_t max_header_bytes = 0;
            uint64_t max_body_bytes = 0;
            function<void(const string&)> on_error;

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "http: " << what << '\n';
                }
            }
        };

        // One connection of the server, in its list: the state the loop
        // and shutdown() hand it between with a compare-and-swap
        struct ServerConn {
            enum : int { active = 0, idle = 1, closed = 2 };
            net::connection c;
            std::atomic<int> state = {active};
            async::stop_source stop;
            tracked_ptr<ServerConn> prev;
            tracked_ptr<ServerConn> next;

            ServerConn(net::connection c, const async::stop_token& parent)
            : c(std::move(c)), stop(parent) {
            }
        };

        struct ServerImpl {
            std::mutex lock;
            tracked_ptr<ServerConn> connections;      // the list's head
            vector<net::listener> listeners;
            RouteTable routes;
            vector<Handler> handlers;
            Handler not_found;
            async::wait_group running;
            async::stop_source closing;               // close(): every request's stop
            std::atomic<bool> shutting_down = {false};
            std::atomic<bool> closed = {false};

            void link(const tracked_ptr<ServerConn>& n) {
                std::lock_guard<std::mutex> g(lock);
                n->next = connections;
                if (connections) {
                    connections->prev = n;
                }
                connections = n;
            }

            void unlink(const tracked_ptr<ServerConn>& n) {
                std::lock_guard<std::mutex> g(lock);
                if (n->prev) {
                    n->prev->next = n->next;
                } else if (connections == n) {
                    connections = n->next;
                }
                if (n->next) {
                    n->next->prev = n->prev;
                }
                n->prev = tracked_ptr<ServerConn>();
                n->next = tracked_ptr<ServerConn>();
            }

            vector<tracked_ptr<ServerConn>> snapshot() {
                std::lock_guard<std::mutex> g(lock);
                vector<tracked_ptr<ServerConn>> all;
                for (auto n = connections; n; n = n->next) {
                    all.push_back(n);
                }
                return all;
            }

            void close_listeners() {
                vector<net::listener> ls;
                {
                    std::lock_guard<std::mutex> g(lock);
                    ls = listeners;
                }
                for (auto& l : ls) {
                    (void)l.close();
                }
            }
        };

        inline time_point deadline_after(duration d) {
            return d > duration::zero() ? sgcl::clock::now() + d : time_point();
        }

        inline time_point earlier(time_point a, time_point b) {
            if (a == time_point()) {
                return b;
            }
            if (b == time_point()) {
                return a;
            }
            return a < b ? a : b;
        }

        // A response the server makes itself, before a handler: the status,
        // its reason as the body, and the end of the connection
        inline std::string refusal(int code, std::string_view extra = {}) {
            std::string body = std::to_string(code) + " " + reason(code) + "\n";
            std::string h = "HTTP/1.1 " + std::to_string(code) + " " + reason(code) + "\r\n";
            h += "Content-Type: text/plain; charset=utf-8\r\n";
            h += date_line();
            h += extra;
            h += "Connection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
            return h + body;
        }

        inline async::task<> linger(net::connection c);

        // The refusal sent, and the connection lingered on: whatever the
        // client was still sending must not reset the connection under it
        inline async::task<> send_refusal(net::connection c, int code) {
            std::string bytes = refusal(code);
            slice<const byte> data(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
            if (co_await c.async_write(data)) {
                co_await linger(c);
            }
        }

        inline async::task<expected<void, io::error>> send_continue(net::connection c) {
            static constexpr std::string_view line = "HTTP/1.1 100 Continue\r\n\r\n";
            slice<const byte> data(reinterpret_cast<const byte*>(line.data()), line.size());
            auto r = co_await c.async_write(data);
            if (!r) {
                co_return io::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

        enum class Next : uint8_t { again, end, hijacked };

        // A connection ended with a request's body unread: the writing half
        // closed first and the rest read and dropped for a while (half a
        // second, 1 MB at most), so that the client reads the response
        // before the close, which with its bytes still unread would be a
        // reset that loses it (Go's closeWrite and wait)
        inline async::task<> linger(net::connection c) {
            (void)c.close_write();
            c.set_read_deadline(sgcl::clock::now() + std::chrono::milliseconds(500));
            tracked_ptr block = make_tracked<io::detail::IoBlock>();
            size_t dropped = 0;
            while (dropped < (size_t(1) << 20)) {
                slice<byte> room(block, block->data(), block->size());
                auto n = co_await c.async_read(room);
                if (!n || *n == 0) {
                    break;
                }
                dropped += *n;
            }
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
                co_await send_refusal(c, refused);
                co_return Next::end;
            }
            req->method = string(req->head.as_slice(line.method_at, line.method_size));
            req->target = string(req->head.as_slice(line.target_at, line.target_size));
            req->minor = line.minor;
            std::string_view method = req->method.view();
            std::string_view target = req->target.view();
            std::string host_text(host ? *host : std::string_view());
            if (target.front() == '/') {
                if (auto u = net::url::parse(string("http://" + (host_text.empty() ? std::string("localhost") : host_text) + std::string(target)))) {
                    req->url = std::move(*u);
                }
            } else if (target == "*" || method == "CONNECT") {
                if (auto u = net::url::parse(string("http://" + (method == "CONNECT" ? std::string(target) : (host_text.empty() ? std::string("localhost") : host_text)) + "/"))) {
                    req->url = std::move(*u);
                }
            } else {
                if (auto u = net::url::parse(req->target)) {
                    req->url = std::move(*u);
                }
            }
            if (!req->url) {
                co_await send_refusal(c, 400);
                co_return Next::end;
            }
            bool expect_continue = false;
            if (auto expect = HeadersAccess::find(req->fields, "expect")) {
                if (iequal(trim_ows(*expect), "100-continue") && line.minor == 1) {
                    expect_continue = true;
                } else {
                    co_await send_refusal(c, 417);
                    co_return Next::end;
                }
            }
            if (framing.kind == Framing::length) {
                req->content_length.emplace(framing.length);
                if (cfg->max_body_bytes && framing.length > cfg->max_body_bytes) {
                    co_await send_refusal(c, 413);
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
            req->body = make_tracked<Body>(wire, framing, cfg->max_body_bytes, false);
            if (expect_continue && framing.kind != Framing::none) {
                net::connection conn = c;
                req->body->set_before_first_read([conn]() { return send_continue(conn); });
            }
            req->remote = c.remote_endpoint();
            req->stop = node->stop.token();

            auto r = RequestAccess::make(req);
            auto writer = WriterAccess::make(w);
            auto found = s->routes.find(method, host_text, req->url->path().view());
            try {
                switch (found.kind) {
                    case RouteTable::Found::route: {
                        for (auto& v : found.values) {
                            req->path_values.push_back(pair<string, string>(string(std::string_view(v.first)), string(std::string_view(v.second))));
                        }
                        auto& h = s->handlers[found.index];
                        if (h.plain) {
                            h.plain(r, writer);
                        } else {
                            co_await h.awaited(r, writer);
                        }
                        break;
                    }
                    case RouteTable::Found::redirect: {
                        std::string to = found.location;
                        if (req->url->has_query()) {
                            to += '?';
                            to += req->url->query().view();
                        }
                        writer.redirect(string(std::string_view(to)), status::temporary_redirect);
                        break;
                    }
                    case RouteTable::Found::method_not_allowed:
                        writer.set_header("Allow", string(std::string_view(found.allow)));
                        writer.error(status::method_not_allowed);
                        break;
                    case RouteTable::Found::not_found:
                        if (s->not_found.plain) {
                            s->not_found.plain(r, writer);
                        } else if (s->not_found.awaited) {
                            co_await s->not_found.awaited(r, writer);
                        } else {
                            writer.error(status::not_found);
                        }
                        break;
                }
            } catch (const std::exception& e) {
                cfg->report(string("a handler of ") + req->method + " " + req->target + " threw: " + e.what());
                w->close_after = true;
                if (!w->head_sent && !w->hijacked) {
                    w->fields = http::headers();
                    writer.error(status::internal_server_error);
                }
            } catch (...) {
                cfg->report(string("a handler of ") + req->method + " " + req->target + " threw");
                w->close_after = true;
                if (!w->head_sent && !w->hijacked) {
                    w->fields = http::headers();
                    writer.error(status::internal_server_error);
                }
            }
            if (w->hijacked) {
                co_return Next::hijacked;
            }
            // a body read past the limit, or broken, with nothing written
            // for it: the server answers 413 or 400
            auto& body = req->body;
            if (body->failed()) {
                w->close_after = true;
                if (!w->head_sent && !w->touched) {
                    writer.error(body->error_status() == 413 ? status::content_too_large : status::bad_request);
                }
            }
            // what the handler left of the body is read after the response,
            // up to 256 KB (Go's bound); a declared rest past that closes the
            // connection, and the response says so
            constexpr uint64_t DrainBound = 256 * 1024;
            if (!body->done() && !body->failed() && framing.kind == Framing::length && framing.length - body->read_total() > DrainBound) {
                w->close_after = true;
            }
            if (s->shutting_down.load()) {
                w->close_after = true;
            }
            auto sent = co_await w->finish();
            if (!sent) {
                node->stop.request_stop();
                co_return Next::end;
            }
            bool drained = body->done() && !body->failed();
            if (!body->done() && !body->failed() && !w->close_after) {
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
            for (;;) {
                if (wire->buffered() == 0) {
                    node->state.store(ServerConn::idle);
                    if (s->shutting_down.load()) {
                        break;
                    }
                    c.set_read_deadline(deadline_after(cfg->idle_timeout));
                    auto r = co_await wire->fill();
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
            cfg->on_error = on_error;
            return cfg;
        }

        tracked_ptr<detail::ServerImpl> _impl;

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
