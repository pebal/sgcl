//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "request.h"
#include "response.h"
#include "status.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

namespace sgcl::net::http {
    using dial_function = function<async::task<expected<net::connection, io::error>>(const net::url&, async::stop_token)>;

    namespace detail {
        struct IdleConnection {
            net::connection c;
            tracked_ptr<Wire> wire;
            time_point since;
            uint64_t id = 0;
        };

        struct Pool;

        // What an idle connection's timer holds: the pool, and which
        // connection it is to close if it is still idle when it fires
        struct PoolExpiry {
            tracked_ptr<Pool> pool;
            string key;
            uint64_t id = 0;
        };

        // The idle connections, by origin ("http://example.com:80"), each
        // list last in first out. A connection put in the pool gets a timer
        // of idle_timeout (the module's timer thread) that closes it if it
        // is still there, and take() skips one past its time as well
        struct Pool {
            std::mutex lock;
            map<string, vector<IdleConnection>> idle;
            uint64_t next_id = 0;

            // the timer's: the connection closed if it is still idle
            void expire(const string& key, uint64_t id) {
                optional<net::connection> gone;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto it = idle.find(key);
                    if (it == idle.end()) {
                        return;
                    }
                    auto& list = it->second;
                    for (auto x = list.begin(); x != list.end(); ++x) {
                        if (x->id == id) {
                            gone = x->c;
                            list.erase(x);
                            break;
                        }
                    }
                    if (list.empty()) {
                        idle.erase(it);
                    }
                }
                if (gone) {
                    (void)gone->close();
                }
            }

            optional<IdleConnection> take(const string& key, duration idle_timeout) {
                vector<net::connection> stale;
                optional<IdleConnection> found;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto it = idle.find(key);
                    if (it == idle.end()) {
                        return nullopt;
                    }
                    auto& list = it->second;
                    auto now = sgcl::clock::now();
                    while (!list.empty()) {
                        auto x = list.back();
                        list.pop_back();
                        if (idle_timeout > duration::zero() && now - x.since > std::chrono::nanoseconds(idle_timeout)) {
                            stale.push_back(x.c);
                            continue;
                        }
                        found = x;
                        break;
                    }
                    if (list.empty()) {
                        idle.erase(it);
                    }
                }
                for (auto& c : stale) {
                    (void)c.close();
                }
                return found;
            }

            void put(const tracked_ptr<Pool>& self, const string& key, IdleConnection x, size_t most, duration idle_timeout) {
                optional<net::connection> dropped;
                uint64_t id = 0;
                {
                    std::lock_guard<std::mutex> g(lock);
                    auto& list = idle[key];
                    if (most == 0) {
                        dropped = x.c;
                    } else {
                        if (list.size() >= most) {
                            dropped = list.front().c;
                            list.erase(list.begin());
                        }
                        id = x.id = ++next_id;
                        list.push_back(std::move(x));
                    }
                }
                if (dropped) {
                    (void)dropped->close();
                }
                if (id && idle_timeout > duration::zero()) {
                    tracked_ptr<PoolExpiry> e = make_tracked<PoolExpiry>();
                    e->pool = self;
                    e->key = key;
                    e->id = id;
                    async::detail::add_timer(idle_timeout, tracked_ptr<void>(e), [](void* p) {
                        auto e = static_cast<PoolExpiry*>(p);
                        e->pool->expire(e->key, e->id);
                    });
                }
            }

            void close_all() {
                vector<net::connection> all;
                {
                    std::lock_guard<std::mutex> g(lock);
                    for (auto& kv : idle) {
                        for (auto& x : kv.second) {
                            all.push_back(x.c);
                        }
                    }
                    idle.clear();
                }
                for (auto& c : all) {
                    (void)c.close();
                }
            }
        };

        struct ClientSettings {
            duration timeout;
            duration connect_timeout;
            duration response_header_timeout;
            duration idle_timeout;
            size_t max_idle_per_host = 16;
            int max_redirects = 10;
            size_t max_response_header_bytes = 1 << 20;
            dial_function dial;
        };

        // What one attempt sends
        struct Outgoing {
            string method;
            optional<net::url> target;         // always set: optional only for want of a default url
            http::headers fields;
            RequestImpl::BodyKind body_kind = RequestImpl::BodyKind::none;
            string text;
            vector<byte> bytes;
            io::reader stream;
            optional<uint64_t> stream_length;

            bool replayable() const noexcept {
                return body_kind != RequestImpl::BodyKind::stream;
            }

            bool idempotent() const noexcept {
                auto m = method.view();
                return m == "GET" || m == "HEAD" || m == "OPTIONS" || m == "TRACE" || m == "PUT" || m == "DELETE";
            }
        };

        inline string origin_key(const net::url& u) {
            return string(std::string(u.scheme().view()) + "://" + std::string(net::detail::UrlAccess::host_as_written(u).view()) + ":" + std::to_string(u.effective_port()));
        }

        inline io::error client_error(net::errc e, const Outgoing& o) {
            return net::detail::net_error(e, o.method, o.target->to_string());
        }

        inline io::error client_error(const io::error& e, const Outgoing& o) {
            return io::error(e.code(), o.method, o.target->to_string());
        }

        // The head of a request, and the body when it is in memory
        inline std::string request_bytes(const Outgoing& o, bool& chunked) {
            std::string s;
            s.reserve(256 + o.text.size() + o.bytes.size());
            s += o.method.view();
            s += ' ';
            s += o.target->request_target().view();
            s += " HTTP/1.1\r\n";
            if (!HeadersAccess::count(o.fields, "host")) {
                s += "Host: ";
                s += o.target->host().view();   // with the port when one is written
                s += "\r\n";
            }
            for (auto& f : HeadersAccess::fields(o.fields)) {
                auto n = f.first.view();
                if (iequal(n, "content-length") || iequal(n, "transfer-encoding")) {
                    continue;
                }
                s += n;
                s += ": ";
                s += f.second.view();
                s += "\r\n";
            }
            chunked = false;
            auto m = o.method.view();
            bool expects_body = m == "POST" || m == "PUT" || m == "PATCH";
            switch (o.body_kind) {
                case RequestImpl::BodyKind::none:
                    if (expects_body) {
                        s += "Content-Length: 0\r\n";
                    }
                    break;
                case RequestImpl::BodyKind::text:
                    s += "Content-Length: " + std::to_string(o.text.size()) + "\r\n";
                    break;
                case RequestImpl::BodyKind::bytes:
                    s += "Content-Length: " + std::to_string(o.bytes.size()) + "\r\n";
                    break;
                case RequestImpl::BodyKind::stream:
                    if (o.stream_length) {
                        s += "Content-Length: " + std::to_string(*o.stream_length) + "\r\n";
                    } else {
                        s += "Transfer-Encoding: chunked\r\n";
                        chunked = true;
                    }
                    break;
            }
            s += "\r\n";
            if (o.body_kind == RequestImpl::BodyKind::text) {
                s += o.text.view();
            } else if (o.body_kind == RequestImpl::BodyKind::bytes) {
                s.append(reinterpret_cast<const char*>(o.bytes.data()), o.bytes.size());
            }
            return s;
        }

        inline async::task<expected<void, io::error>> write_all(net::connection c, std::string bytes) {
            slice<const byte> data(reinterpret_cast<const byte*>(bytes.data()), bytes.size());
            auto r = co_await c.async_write(data);
            if (!r) {
                co_return io::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

        // A stream body: its length's worth, or chunked to its end
        inline async::task<expected<void, io::error>> write_stream(net::connection c, io::reader stream, optional<uint64_t> length, bool chunked) {
            tracked_ptr block = make_tracked<io::detail::CopyBlock>();
            uint64_t sent = 0;
            for (;;) {
                size_t room = block->size();
                if (length) {
                    if (sent == *length) {
                        break;
                    }
                    room = size_t(std::min<uint64_t>(room, *length - sent));
                }
                slice<byte> buf(block, block->data(), room);
                auto n = co_await stream.async_read(buf);
                if (!n) {
                    co_return io::detail::fail(n);
                }
                if (*n == 0) {
                    if (length) {
                        co_return io::detail::fail(io::error(io::errc::unexpected_eof, "write", "body"));
                    }
                    break;
                }
                if (chunked) {
                    char size[24];
                    int k = std::snprintf(size, sizeof(size), "%zx\r\n", *n);
                    auto r = co_await write_all(c, std::string(size, size_t(k)));
                    if (!r) {
                        co_return io::detail::fail(r);
                    }
                }
                auto w = co_await c.async_write(buf.first(*n));
                if (!w) {
                    co_return io::detail::fail(w);
                }
                if (chunked) {
                    auto r = co_await write_all(c, "\r\n");
                    if (!r) {
                        co_return io::detail::fail(r);
                    }
                }
                sent += *n;
            }
            if (chunked) {
                co_return co_await write_all(c, "0\r\n\r\n");
            }
            co_return expected<void, io::error>();
        }

        struct Attempt {
            optional<response> result;
            optional<io::error> error;
            bool retry = false;           // failed before a byte of the response on a pooled connection
        };

        inline async::task<expected<net::connection, io::error>> default_dial(const net::url& u, duration timeout) {
            std::string address(net::detail::UrlAccess::host_as_written(u).view());
            address += ':';
            address += std::to_string(u.effective_port());
            if (timeout > duration::zero()) {
                co_return co_await net::tcp::async_connect(string(std::string_view(address)), timeout);
            }
            co_return co_await net::tcp::async_connect(string(std::string_view(address)));
        }

        inline async::task<Attempt> round_trip(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, const Outgoing& o, time_point deadline, bool may_reuse) {
            Attempt a;
            auto key = origin_key(*o.target);
            optional<IdleConnection> idle = may_reuse ? pool->take(key, cfg->idle_timeout) : optional<IdleConnection>();
            bool reused = (bool)idle;
            net::connection c;
            tracked_ptr<Wire> wire;
            if (idle) {
                c = idle->c;
                wire = idle->wire;
            } else {
                duration timeout = cfg->connect_timeout;
                if (deadline != time_point()) {
                    auto left = duration(std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - sgcl::clock::now()));
                    if (left <= duration::zero()) {
                        a.error = client_error(io::error(error_code(ETIMEDOUT, std::system_category()), "dial", ""), o);
                        co_return a;
                    }
                    if (timeout <= duration::zero() || left < timeout) {
                        timeout = left;
                    }
                }
                expected<net::connection, io::error> dialed = io::detail::fail(io::error(io::errc::closed, "dial", ""));
                if (cfg->dial) {
                    async::stop_source stop;
                    if (timeout > duration::zero()) {
                        stop.stop_after(timeout);
                    }
                    dialed = co_await cfg->dial(*o.target, stop.token());
                } else {
                    dialed = co_await default_dial(*o.target, timeout);
                }
                if (!dialed) {
                    a.error = client_error(dialed.error(), o);
                    co_return a;
                }
                c = *dialed;
                wire = make_tracked<Wire>(c);
            }
            c.set_write_deadline(deadline);
            bool chunked = false;
            auto bytes = request_bytes(o, chunked);
            auto written = co_await write_all(c, std::move(bytes));
            if (written && o.body_kind == RequestImpl::BodyKind::stream) {
                written = co_await write_stream(c, o.stream, o.stream_length, chunked);
            }
            if (!written) {
                (void)co_await c.async_close();
                a.retry = reused;
                a.error = client_error(written.error(), o);
                co_return a;
            }
            auto header_deadline = cfg->response_header_timeout > duration::zero() ? sgcl::clock::now() + cfg->response_header_timeout : time_point();
            c.set_read_deadline(header_deadline == time_point() ? deadline : (deadline == time_point() || header_deadline < deadline ? header_deadline : deadline));
            tracked_ptr impl = make_tracked<ResponseImpl>();
            StatusLine line;
            bool first = true;
            for (;;) {
                auto head = co_await wire->read_head(cfg->max_response_header_bytes);
                if (!head || !*head) {
                    (void)co_await c.async_close();
                    a.retry = reused && first && (!head ? head.error().code() == std::errc::connection_reset || head.error().code() == std::errc::broken_pipe : true);
                    if (!head) {
                        a.error = client_error(head.error().code() == net::errc::header_too_large ? net::detail::net_error(net::errc::header_too_large, "read", "") : head.error(), o);
                    } else {
                        a.error = client_error(io::error(io::errc::unexpected_eof, "read", ""), o);
                    }
                    co_return a;
                }
                first = false;
                impl->head = **head;
                impl->fields = http::headers();
                if (parse_response_head(impl->head, line, impl->fields)) {
                    (void)co_await c.async_close();
                    a.error = client_error(net::errc::malformed_response, o);
                    co_return a;
                }
                if (line.status >= 100 && line.status < 200 && line.status != 101) {
                    continue;   // 100 Continue, 103 Early Hints: the final response follows
                }
                break;
            }
            BodyFraming framing;
            if (!response_framing(impl->fields, line.status, o.method.view() == "HEAD", framing)) {
                (void)co_await c.async_close();
                a.error = client_error(net::errc::malformed_response, o);
                co_return a;
            }
            c.set_read_deadline(deadline);
            impl->status = line.status;
            impl->minor = line.minor;
            impl->url = *o.target;
            if (framing.kind == Framing::length) {
                impl->content_length.emplace(framing.length);
            } else if (framing.kind == Framing::none && HeadersAccess::count(impl->fields, "content-length")) {
                optional<uint64_t> cl;
                content_length(impl->fields, cl);
                set_optional(impl->content_length, cl);
            }
            bool keep = line.minor == 1 ? !HeadersAccess::has_token(impl->fields, "connection", "close")
                                        : HeadersAccess::has_token(impl->fields, "connection", "keep-alive");
            keep = keep && framing.kind != Framing::until_close && line.status != 101;
            impl->body = make_tracked<Body>(wire, framing, 0, true);
            size_t most = cfg->max_idle_per_host;
            duration idle_timeout = cfg->idle_timeout;
            impl->body->set_on_end([pool, key, c, wire, keep, most, idle_timeout](bool clean) {
                if (clean && keep && wire->buffered() == 0) {
                    c.set_deadline(time_point());
                    pool->put(pool, key, IdleConnection{c, wire, sgcl::clock::now()}, most, idle_timeout);
                } else {
                    (void)c.close();
                }
            });
            a.result = ResponseAccess::make(impl);
            co_return a;
        }

        // Whether the headers of the credentials go with a redirect: to the
        // same host or to one under it, as Go decides
        inline bool same_or_sub_host(std::string_view from, std::string_view to) noexcept {
            if (from == to) {
                return true;
            }
            return to.size() > from.size() && to.substr(to.size() - from.size()) == from && to[to.size() - from.size() - 1] == '.';
        }

        inline async::task<expected<response, io::error>> send_request(tracked_ptr<ClientSettings> cfg, tracked_ptr<Pool> pool, tracked_ptr<RequestImpl> req) {
            Outgoing o;
            o.method = req->method;
            if (!req->url) {
                co_return io::detail::fail(net::detail::net_error(net::errc::invalid_url, req->method, req->url_text));
            }
            o.target = *req->url;
            o.fields = req->fields;
            o.body_kind = req->body_kind;
            o.text = req->text;
            o.bytes = req->bytes;
            o.stream = req->stream;
            o.stream_length = req->stream_length;
            auto deadline = cfg->timeout > duration::zero() ? sgcl::clock::now() + cfg->timeout : time_point();
            for (int redirects = 0;; ++redirects) {
                if (o.target->scheme() != "http") {
                    co_return io::detail::fail(client_error(net::errc::unsupported_scheme, o));
                }
                auto a = co_await round_trip(cfg, pool, o, deadline, true);
                if (a.error && a.retry && (o.idempotent() || o.replayable()) && o.body_kind != RequestImpl::BodyKind::stream) {
                    a = co_await round_trip(cfg, pool, o, deadline, false);
                }
                if (a.error) {
                    co_return io::detail::fail(*a.error);
                }
                auto res = *a.result;
                int st = res.status();
                if (st != 301 && st != 302 && st != 303 && st != 307 && st != 308) {
                    co_return res;
                }
                auto location = res.header("Location");
                if (location.empty()) {
                    co_return res;
                }
                auto next = o.target->resolve(location);
                if (!next) {
                    co_return res;
                }
                if (st == 307 || st == 308) {
                    if (!o.replayable()) {
                        co_return res;   // a stream cannot be sent again
                    }
                } else if (o.method.view() != "GET" && o.method.view() != "HEAD") {
                    o.method = "GET";
                    o.body_kind = RequestImpl::BodyKind::none;
                    o.text = string();
                    o.bytes = vector<byte>();
                    o.fields.erase("Content-Type");
                }
                if (redirects + 1 > cfg->max_redirects) {
                    res.close();
                    co_return io::detail::fail(client_error(net::errc::too_many_redirects, o));
                }
                if (!same_or_sub_host(o.target->hostname().view(), next->hostname().view())) {
                    o.fields.erase("Authorization");
                    o.fields.erase("Cookie");
                    o.fields.erase("Proxy-Authorization");
                    o.fields.erase("WWW-Authenticate");
                }
                res.close();
                o.target = next->without_fragment();
            }
        }
    }

    // An HTTP/1.1 client with a pool of connections (Go's Client and
    // Transport in one). A handle of one word: copies share the pool. The
    // settings are public fields, read by each request when it starts.
    //
    // The pool is keyed by the origin (scheme, host, port); idle
    // connections are taken last in first out and dropped past
    // idle_timeout. A connection goes back when the body of its response
    // has been read to its end, and is closed when anything went wrong,
    // the server said Connection: close, or the body was not read. A
    // connection from the pool the server had closed (the request failed
    // before a byte of the response) is tried once more on a new one, for
    // an idempotent method or a body that can be sent again (RFC 9110
    // §9.2.2), as Go does.
    //
    // Redirects are followed up to max_redirects (too_many_redirects past
    // it): 301, 302 and 303 as a GET without a body (HEAD stays HEAD),
    // 307 and 308 with the method and the body, unless the body is a
    // stream (then the 307 is the response). Authorization, Cookie and
    // Proxy-Authorization do not follow to a host that is neither the same
    // nor under it. https:// is net::errc::unsupported_scheme until TLS
    // (the next stage of the module).
    class client {
    public:
        client()
        : _pool(make_tracked<detail::Pool>()) {
        }

        // The request sent, redirects followed: the response, whose body is
        // read next; the error of the connection, or of net: invalid_url,
        // unsupported_scheme, malformed_response, header_too_large,
        // too_many_redirects. A 4xx or 5xx is a response. send() blocks the
        // thread (the exchange runs on the scheduler and the thread waits:
        // never from a worker); in a task `co_await client.async_send(req)`.
        expected<response, io::error> send(const request& req) const {
            return async_send(req).wait();
        }

        async::task<expected<response, io::error>> async_send(const request& req) const {
            return detail::send_request(_settings(), _pool, detail::RequestAccess::impl(req));
        }

        expected<response, io::error> get(const string& url) const {
            return send(request("GET", url));
        }

        async::task<expected<response, io::error>> async_get(const string& url) const {
            return async_send(request("GET", url));
        }

        expected<response, io::error> head(const string& url) const {
            return send(request("HEAD", url));
        }

        async::task<expected<response, io::error>> async_head(const string& url) const {
            return async_send(request("HEAD", url));
        }

        expected<response, io::error> post(const string& url, const string& content_type, const string& body) const {
            return send(_post(url, content_type, body));
        }

        async::task<expected<response, io::error>> async_post(const string& url, const string& content_type, const string& body) const {
            return async_send(_post(url, content_type, body));
        }

        // The idle connections of the pool closed now
        void close_idle_connections() const {
            _pool->close_all();
        }

        duration timeout = duration::zero();                     // the whole exchange, the body's reading included; zero: none
        duration connect_timeout = std::chrono::seconds(30);
        duration response_header_timeout = duration::zero();     // from the request sent to the head of the response
        duration idle_timeout = std::chrono::seconds(90);        // a connection in the pool
        size_t max_idle_per_host = 16;                           // Go's default is 2
        int max_redirects = 10;
        size_t max_response_header_bytes = 1 << 20;
        // How a connection is made; tcp::connect by default. A unix socket
        // (Docker's API), a test's connection in memory, TLS in stage 2
        dial_function dial;

    private:
        tracked_ptr<detail::ClientSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::ClientSettings>();
            cfg->timeout = timeout;
            cfg->connect_timeout = connect_timeout;
            cfg->response_header_timeout = response_header_timeout;
            cfg->idle_timeout = idle_timeout;
            cfg->max_idle_per_host = max_idle_per_host;
            cfg->max_redirects = max_redirects;
            cfg->max_response_header_bytes = max_response_header_bytes ? max_response_header_bytes : 1;
            cfg->dial = dial;
            return cfg;
        }

        static request _post(const string& url, const string& content_type, const string& body) {
            request r("POST", url);
            r.set_header("Content-Type", content_type);
            r.set_body(body);
            return r;
        }

        tracked_ptr<detail::Pool> _pool;
    };
}
