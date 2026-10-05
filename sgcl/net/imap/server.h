//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "backend.h"
#include "error.h"
#include "types.h"
#include "detail/server_state.h"
#include "detail/session.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../async/wait_group.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/weak_ptr.h"

#include <chrono>
#include <mutex>

namespace sgcl::net::imap {
    // An IMAP server (RFC 9051, IMAP4rev2 with IMAP4rev1's clients): the
    // protocol and its extensions over a backend that keeps the mail. A
    // handle of one word: copies share the connections and the open
    // mailboxes. The fields are read when serve() is called.
    class server {
    public:
        SGCL_INLINE_HOT server()
        : _impl(make_tracked<detail::ServerImpl>()) {
        }

        server(const server&) = default;
        server& operator=(const server&) = default;

        // Listens on the address (":143") and serves until shutdown() or
        // close(): then net::errc::server_closed. STARTTLS is offered when
        // tls is set. serve() blocks the thread, the connections served on
        // the scheduler; in a task `co_await s.async_serve(":143")`
        SGCL_INLINE_HOT expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // Listens over TLS from the first byte (port 993, RFC 8314) with
        // the config, and serves
        SGCL_INLINE_HOT expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const {
            return async_serve_tls(address, c).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const noexcept {
            return _co_serve_tls(_impl, _settings(), address, c);
        }

        // The connections of a listener the program made (a TLS listener's
        // are taken as TLS from the start)
        SGCL_INLINE_HOT expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return _co_serve(_impl, _settings(), l);
        }

        // Gracefully: the listeners closed, each session told BYE after its
        // current command (an idle one at once); returns when all have ended
        SGCL_INLINE_HOT void shutdown() const {
            async_shutdown().wait();
        }

        SGCL_INLINE_HOT async::task<> async_shutdown() const noexcept {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed
        void close() const {
            _impl->shutting_down.store(true);
            _impl->closed.store(true);
            _impl->close_listeners();
            for (auto& e : _impl->snapshot()) {
                e->state.store(detail::ConnEntry::closed);
                (void)e->c.close();
            }
        }

        // The number of connections being served
        SGCL_INLINE_HOT size_t connections() const noexcept {
            return _impl->active.load();
        }

        imap::backend backend;                                      // where the mail is; a memory_backend of the server's own by default
        // The credentials of LOGIN and AUTHENTICATE PLAIN and LOGIN: the
        // user and the password; empty: the backend's authenticate (a
        // memory_backend's add_user), else every login refused
        function<bool(const string&, const string&)> check_password;
        // The bearer token of AUTHENTICATE XOAUTH2 and OAUTHBEARER (RFC
        // 7628): the user and the token; empty: neither offered
        function<bool(const string&, const string&)> check_token;
        // STARTTLS offered with this config; while it is set, LOGIN and
        // AUTHENTICATE are refused before TLS (LOGINDISABLED)
        optional<net::tls::config> tls;
        duration idle_timeout = std::chrono::minutes(30);           // an authenticated connection silent this long is closed (RFC 9051 §5.4: at least 30 minutes)
        duration login_timeout = std::chrono::seconds(60);          // the same before authentication
        duration poll_interval = std::chrono::seconds(10);          // an idling session asks the backend for changes from outside this often; zero: never
        size_t max_literal_bytes = size_t(64) << 20;                // the largest literal, a message appended (APPENDLIMIT)
        size_t max_command_bytes = size_t(64) << 10;                // the longest command line, literals apart
        size_t max_connections = 0;                                 // past it a connection is told BYE; zero: none
        int max_auth_failures = 3;                                  // failed logins before the connection is closed; zero: none
        bool compress = true;                                       // COMPRESS=DEFLATE (RFC 4978) offered
        vector<pair<string, string>> id = {{string("name"), string("sgcl")}};   // the server's answer to ID (RFC 2971)
        string greeting = string("IMAP4rev2 server ready");         // the text of the greeting
        function<void(const string&)> on_error;                     // a backend's failure, an accept's; a line on stderr by default

    private:
        tracked_ptr<detail::ServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::ServerSettings>();
            cfg->backend = detail::BackendAccess::get(backend);
            cfg->check_password = check_password;
            cfg->check_token = check_token;
            cfg->tls = tls;
            cfg->idle_timeout = idle_timeout;
            cfg->login_timeout = login_timeout;
            cfg->poll_interval = poll_interval;
            cfg->max_literal = max_literal_bytes ? max_literal_bytes : 1;
            cfg->max_command = max_command_bytes ? max_command_bytes : 1024;
            cfg->max_connections = max_connections;
            cfg->max_auth_failures = max_auth_failures;
            cfg->compress = compress;
            cfg->id = id;
            cfg->greeting = greeting;
            cfg->on_error = on_error;
            return cfg;
        }

        static void _watch(const tracked_ptr<detail::ServerImpl>& impl, const tracked_ptr<detail::ServerSettings>& cfg) {
            {
                std::lock_guard<std::mutex> g(impl->lock);
                if (impl->watching || !cfg->backend->has_watch()) {
                    return;
                }
                impl->watching = true;
            }
            weak_ptr<detail::ServerImpl> weak(impl);
            cfg->backend->watch(detail::Watcher([weak](const string& user, const string& name) {
                if (detail::hub_change) {
                    return;
                }
                if (tracked_ptr<detail::ServerImpl> s = weak.lock()) {
                    s->changed_outside(user, name);
                }
            }));
        }

        tracked_ptr<detail::ServerImpl> _impl;

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, net::listener l) noexcept {
            if (impl->shutting_down.load()) {
                (void)l.close();
                co_return io::detail::fail(net::detail::net_error(net::errc::server_closed, "serve", l.local_endpoint().to_string()));
            }
            {
                std::lock_guard<std::mutex> g(impl->lock);
                impl->listeners.push_back(l);
            }
            _watch(impl, cfg);
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
                const bool tls = net::tls::state_of(*c).has_value();
                async::go(detail::serve_connection(impl, cfg, *c, tls));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_tls(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, string address,
                                                                    net::tls::config c) noexcept {
            auto l = co_await net::tls::async_listen(address, c);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::ServerImpl> impl, tracked_ptr<detail::ServerSettings> cfg, string address) noexcept {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::ServerImpl> impl) noexcept {
            impl->shutting_down.store(true);
            impl->close_listeners();
            // the connections waiting for a command told BYE and closed; the
            // others end after the command they run
            for (auto& e : impl->snapshot()) {
                int expected_state = detail::ConnEntry::idle;
                if (e->state.compare_exchange_strong(expected_state, detail::ConnEntry::closed)) {
                    std::string bye = "* BYE [UNAVAILABLE] Server shutting down\r\n";
                    (void)co_await detail::write_all(e->c, bye);
                    (void)co_await e->c.async_close();
                }
            }
            co_await impl->running;
        }
    };
}
