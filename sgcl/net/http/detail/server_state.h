//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../request.h"
#include "../response_writer.h"
#include "../status.h"
#include "router.h"
#include "wire.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../socket.h"
#include "../../../async/coroutine.h"
#include "../../../async/stop_token.h"
#include "../../../async/wait_group.h"
#include "../../../core/aliases.h"
#include "../../../core/clock.h"
#include "../../../core/duration.h"
#include "../../../core/function.h"
#include "../../../core/make_tracked.h"
#include "../../../core/string.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"
#include "../../../slog/logger.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <type_traits>

// What the server of net::http keeps (its routes, its connections, the
// settings a serve() runs with) and what both of its protocols share: the
// dispatch of a request to its handler. server.h serves HTTP/1.1 over it,
// h2/serve.h HTTP/2.
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
            bool http2 = true;
            bool h2c = false;
            uint32_t max_concurrent_streams = 250;
            function<void(const string&)> on_error;
            optional<slog::logger> access_log;   // a record per exchange, when set (server::access_log)

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "http: " << what << '\n';
                }
            }
        };

        // The access log's record of one exchange, when the server has one:
        // at info, or at error for a 5xx; every attribute a view of the
        // request and of the response's counts, the remote address written
        // into the line (endpoint::write_text), so that a request adds
        // nothing to the managed heap (DESIGN 283). A request-id field
        // (X-Request-ID) is written when the request has one. `bytes` is
        // the writer's body_bytes() taken before the finish, which gives
        // the body's blocks back
        inline void log_access(const ServerSettings& cfg, const RequestImpl& req, const WriterImpl& w, uint64_t bytes, std::string_view path,
                               std::string_view proto, time_point start) {
            if (!cfg.access_log) {
                return;
            }
            const slog::logger& log = *cfg.access_log;
            const slog::level l = w.status >= 500 ? slog::level::error : slog::level::info;
            if (!log.enabled(l)) {
                return;
            }
            const duration took = sgcl::clock::now() - start;
            const std::string_view method = req.method.view();
            const std::string_view agent = HeadersAccess::find(req.fields, "user-agent").value_or(std::string_view());
            if (auto id = HeadersAccess::find(req.fields, "x-request-id")) {
                log.log(l, "request", "method", method, "path", path, "proto", proto, "status", w.status, "bytes", bytes, "duration", took,
                        "remote", req.remote, "user_agent", agent, "request_id", *id);
            } else {
                log.log(l, "request", "method", method, "path", path, "proto", proto, "status", w.status, "bytes", bytes, "duration", took,
                        "remote", req.remote, "user_agent", agent);
            }
        }

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
            async::detail::WaitGroupState running;
            async::stop_source closing;               // close(): every request's stop
            std::atomic<bool> shutting_down = {false};
            std::atomic<bool> closed = {false};

            void link(const tracked_ptr<ServerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                n->next = connections;
                if (connections) {
                    connections->prev = n;
                }
                connections = n;
            }

            void unlink(const tracked_ptr<ServerConn>& n) noexcept {
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

            vector<tracked_ptr<ServerConn>> snapshot() noexcept {
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

        inline time_point deadline_after(duration d) noexcept {
            return d > duration::zero() ? sgcl::clock::now() + d : time_point();
        }

        inline time_point earlier(time_point a, time_point b) noexcept {
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
        // The answer of a refusal and the connection's end; to a HEAD the
        // head alone, its Content-Length the body a GET would have had
        // (RFC 9110 §9.3.2: no content in an answer to HEAD — a client
        // reads none, and bytes after the head would be taken for the
        // next answer)
        inline std::string refusal(int code, std::string_view extra, bool head_request) noexcept {
            std::string body = std::to_string(code) + " " + reason(code) + "\n";
            std::string h = "HTTP/1.1 " + std::to_string(code) + " " + reason(code) + "\r\n";
            h += "Content-Type: text/plain; charset=utf-8\r\n";
            append_date_line(h);
            h += extra;
            h += "Connection: close\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n";
            return head_request ? h : h + body;
        }

        inline std::string refusal(int code, std::string_view extra = {}) noexcept {
            return refusal(code, extra, false);
        }

        inline async::task<> linger(net::connection c) noexcept;

        // The refusal sent, and the connection lingered on: whatever the
        // client was still sending must not reset the connection under it
        inline async::task<> send_refusal(net::connection c, int code, bool head_request) noexcept {
            std::string bytes = refusal(code, {}, head_request);
            slice<const byte> data(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
            if (co_await c.async_write(data)) {
                co_await linger(c);
            }
        }

        inline async::task<> send_refusal(net::connection c, int code) noexcept {
            co_await send_refusal(c, code, false);
        }

        inline async::task<expected<void, io::error>> send_continue(net::connection c) noexcept {
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
        inline async::task<> linger(net::connection c) noexcept {
            (void)c.close_write();
            c.set_read_deadline(sgcl::clock::now() + std::chrono::milliseconds(500));
            auto block = std::make_unique_for_overwrite<byte[]>(config::io_buffer_size);   // scratch: unmanaged, the frame's while it runs
            size_t dropped = 0;
            while (dropped < (size_t(1) << 20)) {
                slice<byte> room(block.get(), config::io_buffer_size);
                auto n = co_await c.async_read(room);
                if (!n || *n == 0) {
                    break;
                }
                dropped += *n;
            }
        }

        // The methods of nearly every request as strings made once, so
        // that a head's method costs no string of its own; another method
        // is made from the head's bytes. Held by a root never destroyed, as
        // the standard streams' files are: a static destructor may still
        // serve a request
        struct MethodNames {
            string get = "GET";
            string head = "HEAD";
            string post = "POST";
            string put = "PUT";
            string del = "DELETE";
            string patch = "PATCH";
            string options = "OPTIONS";
        };

        inline string method_name(std::string_view m) noexcept {
            static root_ptr<MethodNames>* names = new root_ptr<MethodNames>(make_tracked<MethodNames>());
            auto& n = **names;
            switch (m.size()) {
                case 3:
                    if (m == "GET") return n.get;
                    if (m == "PUT") return n.put;
                    break;
                case 4:
                    if (m == "HEAD") return n.head;
                    if (m == "POST") return n.post;
                    break;
                case 5:
                    if (m == "PATCH") return n.patch;
                    break;
                case 6:
                    if (m == "DELETE") return n.del;
                    break;
                case 7:
                    if (m == "OPTIONS") return n.options;
                    break;
            }
            return string(m);
        }


        // The route of a request found and its handler run, as far as it
        // goes without waiting: a plain handler (or a redirect, a 405, a
        // 404) is done on return, an awaited one's task is given back for
        // the caller to await in its own frame (no frame of this one's). A
        // throw of a plain handler goes to the caller's catch
        inline optional<async::task<>> dispatch(ServerImpl& s, const tracked_ptr<RequestImpl>& req, request& r, response_writer& writer, std::string_view method,
                                                std::string_view host_text, std::string_view path) {
            auto found = s.routes.find(method, host_text, path);
            switch (found.kind) {
                case RouteTable::Found::route: {
                    req->path_values = std::move(found.values);   // the router's vector itself: managed strings, as it made them
                    auto& h = s.handlers[found.index];
                    if (h.plain) {
                        h.plain(r, writer);
                        return nullopt;
                    }
                    return h.awaited(r, writer);
                }
                case RouteTable::Found::redirect: {
                    std::string to = found.location;
                    if (auto u = req->url_of(); u && u->has_query()) {
                        to += '?';
                        to += u->query().view();
                    }
                    writer.redirect(string(std::string_view(to)), status::temporary_redirect);
                    return nullopt;
                }
                case RouteTable::Found::method_not_allowed:
                    writer.set_header("Allow", string(std::string_view(found.allow)));
                    writer.error(status::method_not_allowed);
                    return nullopt;
                case RouteTable::Found::not_found:
                    if (s.not_found.plain) {
                        s.not_found.plain(r, writer);
                        return nullopt;
                    }
                    if (s.not_found.awaited) {
                        return s.not_found.awaited(r, writer);
                    }
                    writer.error(status::not_found);
                    return nullopt;
            }
            return nullopt;
        }

        // A handler threw: on_error hears of it, a 500 when nothing was
        // sent, and the connection (HTTP/1.1) ends after the response
        inline void handler_threw(const ServerSettings& cfg, const RequestImpl& req, WriterImpl& w, response_writer& writer, const char* what) {
            if (what) {
                cfg.report(string("a handler of ") + req.method + " " + string(req.target) + " threw: " + what);
            } else {
                cfg.report(string("a handler of ") + req.method + " " + string(req.target) + " threw");
            }
            w.close_after = true;
            if (!w.head_sent && !w.hijacked) {
                w.fields = http::headers();
                writer.error(status::internal_server_error);
            }
        }
    }
}
