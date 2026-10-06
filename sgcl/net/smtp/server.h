//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "sender_checks.h"
#include "envelope.h"
#include "detail/machine.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../../async/coroutine.h"
#include "../../async/wait_group.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/email.h"
#include "../../io/stream.h"

#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>
#include <string>
#include <type_traits>
#include <unistd.h>

// The server of SMTP: a handler per message, as http::server has one per
// request; no relaying and no queue — what is done with a message is the
// handler's
namespace sgcl::net::smtp {
    class message;
    class server;

    namespace detail {
        struct MessageState {
            smtp::envelope env;
            vector<byte> data;
            size_t at = 0;
            optional<smtp::sender_verdict> auth;
        };

        struct MessageAccess;

        struct ServerHandler {
            function<smtp::reply(smtp::message)> plain;
            function<async::task<smtp::reply>(smtp::message)> awaited;
        };

        struct SmtpServerConn {
            enum : int { active = 0, idle = 1, closed = 2 };
            net::connection c;
            std::atomic<int> state = {active};
            tracked_ptr<SmtpServerConn> prev;
            tracked_ptr<SmtpServerConn> next;

            explicit SmtpServerConn(net::connection conn) noexcept
            : c(std::move(conn)) {
            }
        };

        struct SmtpServerImpl {
            std::mutex lock;
            tracked_ptr<SmtpServerConn> connections;
            vector<net::listener> listeners;
            ServerHandler handler;
            async::detail::WaitGroupState running;
            std::atomic<bool> shutting_down = {false};
            std::atomic<bool> closed = {false};

            void link(const tracked_ptr<SmtpServerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                n->next = connections;
                if (connections) {
                    connections->prev = n;
                }
                connections = n;
            }

            void unlink(const tracked_ptr<SmtpServerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                if (n->prev) {
                    n->prev->next = n->next;
                } else if (connections == n) {
                    connections = n->next;
                }
                if (n->next) {
                    n->next->prev = n->prev;
                }
                n->prev = tracked_ptr<SmtpServerConn>();
                n->next = tracked_ptr<SmtpServerConn>();
            }

            vector<tracked_ptr<SmtpServerConn>> snapshot() noexcept {
                std::lock_guard<std::mutex> g(lock);
                vector<tracked_ptr<SmtpServerConn>> all;
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

        struct SmtpServerSettings {
            MachineSettings machine;
            optional<net::tls::config> starttls;
            optional<smtp::sender_checks> checks;
            duration timeout = 5 * minute;
            function<void(const string&)> on_error;

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "smtp: " << what.view() << std::endl;
                }
            }
        };

    }

    // A message a server received, as its handler gets it: the envelope
    // and the bytes of the message (its dots taken off, line breaks CRLF),
    // read whole before the handler runs, at most server::max_message_bytes.
    // A reader of those bytes, and the message parsed in one call. A handle:
    // a copy is the same message and reads on from where it is.
    class message : public io::mixin::reader<message> {
    public:
        message() noexcept = default;   // no message; an operation on it is a contract violation

        const smtp::envelope& envelope() const noexcept {
            return _s->env;
        }

        // The next bytes of the message; 0 at its end
        expected<size_t, io::error> read(const slice<byte>& buffer) const noexcept {
            size_t n = std::min(buffer.size(), _s->data.size() - _s->at);
            if (n) {
                sgcl::detail::copy_bytes(buffer.data(), _s->data.data() + _s->at, n);
            }
            _s->at += n;
            return n;
        }

        async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept {
            co_return read(buffer);
        }

        // The size in bytes
        size_t size() const noexcept {
            return _s->data.size();
        }

        // The whole message, wherever the reading is
        vector<byte> bytes() const noexcept {
            return _s->data;
        }

        // What the server's checks found (server::sender_checks set):
        // SPF, DKIM and DMARC of the message; nullopt without checks
        const optional<smtp::sender_verdict>& sender_verdict() const noexcept {
            return _s->auth;
        }

        // The message parsed (encoding::email::parse)
        expected<encoding::email, encoding::error> email() const {
            return encoding::email::parse(_s->data);
        }

        expected<encoding::email, encoding::error> email(const encoding::email::limits& l) const {
            return encoding::email::parse(_s->data, l);
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

    private:
        friend struct detail::MessageAccess;

        explicit message(tracked_ptr<detail::MessageState> s) noexcept
        : _s(std::move(s)) {
        }

        tracked_ptr<detail::MessageState> _s;
    };

    namespace detail {
        struct MessageAccess {
            static smtp::message make(const smtp::envelope& env, const std::string& data, optional<smtp::sender_verdict> auth = nullopt) {
                tracked_ptr s = make_tracked<MessageState>();
                s->env = env;
                s->auth = std::move(auth);
                VectorOverwrite::resize(s->data, data.size());
                if (!data.empty()) {
                    copy_bytes(s->data.data(), data.data(), data.size());
                }
                return smtp::message(s);
            }
        };

        inline async::task<smtp::reply> run_handler(tracked_ptr<SmtpServerImpl> s, tracked_ptr<SmtpServerSettings> cfg, smtp::message m) noexcept {
            ServerHandler h;
            {
                std::lock_guard<std::mutex> g(s->lock);
                h = s->handler;
            }
            try {
                if (h.awaited) {
                    co_return co_await h.awaited(m);
                }
                if (h.plain) {
                    co_return h.plain(m);
                }
                co_return smtp::reply();
            } catch (const std::exception& e) {
                cfg->report(string::concat("a handler threw: ", e.what()));
            } catch (...) {
                cfg->report(string("a handler threw"));
            }
            co_return smtp::reply{451, string("4.3.0"), string("Error processing the message")};
        }

        inline async::task<> serve_connection(tracked_ptr<SmtpServerImpl> s, tracked_ptr<SmtpServerSettings> cfg, net::connection c, bool implicit_tls) noexcept {
            tracked_ptr node = make_tracked<SmtpServerConn>(c);
            s->link(node);
            tracked_ptr block = make_tracked<SmtpBlock>();
            ServerMachine m;
            m.cfg = cfg->machine;
            m.cfg.tls_offered = cfg->starttls.has_value() && !implicit_tls;
            m.env.client = c.remote_endpoint();
            m.env.tls = implicit_tls;
            m.greet();
            for (;;) {
                MachineStep step = m.step();
                if (!m.out.empty()) {
                    // what the socket takes at once without a frame (net's
                    // start_write); m.out is not touched until the rest is written
                    c.set_write_deadline(sgcl::clock::now() + cfg->timeout);
                    auto st = net::detail::ConnectionAccess::impl(c).start_write(
                        slice<const byte>(reinterpret_cast<const byte*>(m.out.data()), m.out.size()));
                    expected<size_t, io::error> w = std::move(st.done);
                    if (st.rest) {
                        w = co_await std::move(*st.rest);
                    }
                    m.out.clear();
                    if (!w) {
                        break;
                    }
                }
                if (step == MachineStep::close) {
                    break;
                }
                if (step == MachineStep::message) {
                    optional<smtp::sender_verdict> auth;
                    if (cfg->checks) {
                        auth = co_await smtp::async_check_sender(string(m.data()), m.env, *cfg->checks);
                        if (cfg->checks->reject && auth->dmarc && auth->dmarc->status == dmarc::status::fail && auth->dmarc->disposition == dmarc::policy::reject) {
                            m.data().clear();
                            m.message_done(smtp::reply{550, string("5.7.1"), string::concat("Rejected by the DMARC policy of ", auth->dmarc->domain)});
                            continue;
                        }
                        if (cfg->checks->add_header) {
                            m.data() = with_results(m.data(), auth->results(cfg->checks->authserv_id));
                        }
                    }
                    smtp::message msg = MessageAccess::make(m.env, m.data(), std::move(auth));
                    m.data().clear();
                    smtp::reply r = co_await run_handler(s, cfg, msg);
                    m.message_done(r);
                    continue;
                }
                if (step == MachineStep::starttls) {
                    c.set_deadline(time_point());
                    auto t = co_await net::tls::async_server(c, *cfg->starttls);
                    if (!t) {
                        break;
                    }
                    c = *t;   // the node keeps the raw connection: closing it ends this one
                    m.tls_started();
                    continue;
                }
                // more input
                if (s->shutting_down.load()) {
                    m.shutdown("421 4.3.2 Service shutting down");
                    continue;
                }
                node->state.store(SmtpServerConn::idle);
                if (s->shutting_down.load()) {
                    break;
                }
                c.set_read_deadline(sgcl::clock::now() + cfg->timeout);
                expected<size_t, io::error> r = size_t(0);
                for (;;) {   // the read without a frame of its own (net's try_read), as http's server reads
                    bool slow = false;
                    auto& impl = net::detail::ConnectionAccess::impl(c);
                    auto t = impl.try_read(slice<byte>(block->data(), block->size()), slow);
                    if (slow) {
                        r = co_await c.async_read(slice<byte>(block->data(), block->size()));
                        break;
                    }
                    if (!t) {
                        r = net::detail::fail(t);
                        break;
                    }
                    if (*t) {
                        r = **t;
                        break;
                    }
                    if (auto ready = co_await impl.raw_readable(); !ready) {
                        r = net::detail::fail(ready);
                        break;
                    }
                }
                int expected_state = SmtpServerConn::idle;
                if (!node->state.compare_exchange_strong(expected_state, SmtpServerConn::active)) {
                    break;   // the shutdown took it while it waited
                }
                if (!r) {
                    if (r.error().is_timeout()) {
                        m.shutdown("421 4.4.2 Timeout, closing");
                        continue;
                    }
                    break;
                }
                if (*r == 0) {
                    break;
                }
                m.feed(std::string_view(reinterpret_cast<const char*>(block->data()), *r));
            }
            (void)co_await c.async_close();
            s->unlink(node);
            s->running.done();
            co_return;
        }
    }

    // An SMTP server with a handler per message (in the manner of
    // http::server). A handle of one word: copies share the handler and
    // the connections. The fields are read when serve() is called.
    //
    //     net::smtp::server srv;
    //     srv.handle([](net::smtp::message m) { save(m.envelope(), m.email()); });
    //     srv.serve(":2525");
    //
    // The handler takes the message and returns void (accepted, 250),
    // smtp::reply (a code of 400 or more refuses it, a code of 0 accepts),
    // or async::task<> or async::task<smtp::reply> for one that waits. The
    // session offers PIPELINING, SIZE, 8BITMIME, SMTPUTF8, CHUNKING,
    // BINARYMIME, DSN and ENHANCEDSTATUSCODES; STARTTLS with starttls
    // set; AUTH PLAIN and LOGIN with auth set, over TLS only unless
    // allow_insecure_auth says otherwise. on_sender and on_recipient
    // decide MAIL and RCPT (a reply of 0 takes the address).
    class server {
    public:
        server() noexcept
        : _impl(make_tracked<detail::SmtpServerImpl>()) {
        }

        server(const server&) = default;
        server& operator=(const server&) = default;

        template<class Handler>
        server& handle(Handler h) {
            detail::ServerHandler out;
            using R = std::invoke_result_t<Handler&, message>;
            if constexpr (std::is_void_v<R>) {
                out.plain = function<reply(message)>([h = std::move(h)](message m) mutable {
                    h(m);
                    return reply();
                });
            } else if constexpr (std::is_same_v<R, reply>) {
                out.plain = function<reply(message)>(std::move(h));
            } else if constexpr (std::is_same_v<R, async::task<>>) {
                out.awaited = function<async::task<reply>(message)>([h = std::move(h)](message m) mutable { return _awaited_void(h(m)); });
            } else {
                static_assert(std::is_same_v<R, async::task<reply>>, "a handler returns void, smtp::reply, async::task<> or async::task<smtp::reply>");
                out.awaited = function<async::task<reply>(message)>(std::move(h));
            }
            std::lock_guard<std::mutex> g(_impl->lock);
            _impl->handler = std::move(out);
            return *this;
        }

        // Listens on the address (":2525") and serves until shutdown() or
        // close(): then net::errc::server_closed
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // Listens over TLS from the first byte (port 465, RFC 8314)
        expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const {
            return async_serve_tls(address, c).wait();
        }

        async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const noexcept {
            return _co_serve_tls(_impl, _settings(), address, c);
        }

        // The connections of a listener the program made (a TLS one serves
        // as implicit TLS)
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return _co_serve(_impl, _settings(), l, false);
        }

        // Gracefully: the listeners closed, the sessions waiting for a
        // command closed, the others after the command they are in (421);
        // returns when all of them have
        void shutdown() const {
            async_shutdown().wait();
        }

        async::task<> async_shutdown() const noexcept {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed
        void close() const {
            _impl->shutting_down.store(true);
            _impl->closed.store(true);
            _impl->close_listeners();
            for (auto& n : _impl->snapshot()) {
                (void)n->c.close();
            }
        }

        string hostname;                                         // the greeting's and EHLO's name; empty: this host's name
        optional<net::tls::config> starttls;                     // STARTTLS offered, with this config
        function<bool(const string&, const string&)> auth;       // a user and a password checked; AUTH offered when set
        bool allow_insecure_auth = false;                        // AUTH offered without TLS too
        function<reply(const envelope&)> on_sender;              // MAIL decided: a code of 0 takes it
        function<reply(const envelope&, const string&)> on_recipient;   // RCPT decided: a code of 0 takes it
        optional<smtp::sender_checks> sender_checks;         // SPF, DKIM and DMARC checked after DATA, the results to the handler
        uint64_t max_message_bytes = uint64_t(32) << 20;         // SIZE announced; a message past it 552
        size_t max_recipients = 100;                             // RCPT past it 452
        size_t max_line_bytes = 2048;                            // a command past it 500
        size_t max_errors = 10;                                  // refused commands before 421 and the end
        size_t max_junk_commands = 100;                          // NOOP, RSET, VRFY, HELP, EHLO between messages before 421
        duration timeout = 5 * minute;                           // the wait for each command and each piece of data
        function<void(const string&)> on_error;                  // a handler's exception, an accept's failure; a line on stderr by default

    private:
        static async::task<reply> _awaited_void(async::task<> t) noexcept {
            co_await std::move(t);
            co_return reply();
        }

        tracked_ptr<detail::SmtpServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::SmtpServerSettings>();
            auto& m = cfg->machine;
            if (!hostname.empty()) {
                m.hostname = hostname;
            } else {
                char name[256] = {};
                m.hostname = ::gethostname(name, sizeof name - 1) == 0 && name[0] ? string(name) : string("localhost");
            }
            m.max_message_bytes = max_message_bytes;
            m.max_recipients = max_recipients ? max_recipients : 1;
            m.max_line_bytes = max_line_bytes ? max_line_bytes : 512;
            m.max_errors = max_errors ? max_errors : 1;
            m.max_junk_commands = max_junk_commands ? max_junk_commands : 1;
            m.auth_offered = bool(auth);
            m.allow_insecure_auth = allow_insecure_auth;
            m.auth = auth;
            m.on_sender = on_sender;
            m.on_recipient = on_recipient;
            cfg->starttls = starttls;
            cfg->checks = sender_checks;
            if (cfg->checks && cfg->checks->authserv_id.empty()) {
                cfg->checks->authserv_id = m.hostname;
            }
            cfg->timeout = timeout > duration::zero() ? timeout : 5 * minute;
            cfg->on_error = on_error;
            return cfg;
        }

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::SmtpServerImpl> impl, tracked_ptr<detail::SmtpServerSettings> cfg, net::listener l,
                                                                bool implicit_tls) noexcept {
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
                    if (!async::detail::runtime_exiting()) {   // the end of the program (async/scheduler.h: runtime_exit) is no failure to report
                        cfg->report(string("accept: ") + c.error().message());
                    }
                    co_return io::detail::fail(c);
                }
                impl->running.add();
                async::go(detail::serve_connection(impl, cfg, *c, implicit_tls || net::tls::state_of(*c).has_value()));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::SmtpServerImpl> impl, tracked_ptr<detail::SmtpServerSettings> cfg,
                                                                        string address) noexcept {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l, false);
        }

        static async::task<expected<void, io::error>> _co_serve_tls(tracked_ptr<detail::SmtpServerImpl> impl, tracked_ptr<detail::SmtpServerSettings> cfg, string address,
                                                                    net::tls::config c) noexcept {
            auto l = co_await net::tls::async_listen(address, c);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l, true);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::SmtpServerImpl> impl) noexcept {
            impl->shutting_down.store(true);
            impl->close_listeners();
            for (auto& n : impl->snapshot()) {
                int expected = detail::SmtpServerConn::idle;
                if (n->state.compare_exchange_strong(expected, detail::SmtpServerConn::closed)) {
                    (void)co_await n->c.async_close();
                }
            }
            co_await impl->running;
        }

        tracked_ptr<detail::SmtpServerImpl> _impl;
    };
}
