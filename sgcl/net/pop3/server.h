//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/md5.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../error.h"
#include "../imap/backend.h"
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
#include "../../encoding/base64.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

// The server of POP3 (RFC 1939, RFC 2449, RFC 2595, RFC 5034): the
// INBOX of each user of an imap::backend, as POP3 sees a maildrop
namespace sgcl::net::pop3 {
    namespace detail {
        struct Pop3ServerSettings {
            tracked_ptr<imap::detail::Backend> backend;
            function<bool(const string&, const string&)> check_password;
            function<optional<string>(const string&)> apop_secret;
            optional<net::tls::config> tls;
            bool allow_insecure_auth = false;
            duration idle_timeout = 10 * minute;
            size_t max_connections = 0;
            int max_auth_failures = 3;
            string greeting;
            string hostname;
            function<void(const string&)> on_error;

            void report(const string& what) const {
                if (on_error) {
                    on_error(what);
                } else {
                    std::cerr << "pop3: " << what.view() << std::endl;
                }
            }
        };

        struct Pop3ServerConn {
            enum : int { active = 0, idle = 1, closed = 2 };
            net::connection c;
            std::atomic<int> state = {active};
            tracked_ptr<Pop3ServerConn> prev;
            tracked_ptr<Pop3ServerConn> next;

            explicit Pop3ServerConn(net::connection conn) noexcept
            : c(std::move(conn)) {
            }
        };

        struct Pop3ServerImpl {
            std::mutex lock;
            tracked_ptr<Pop3ServerConn> connections;
            vector<net::listener> listeners;
            std::set<std::string> locked;   // the users with a session in TRANSACTION
            async::detail::WaitGroupState running;
            std::atomic<size_t> active = {0};
            std::atomic<bool> shutting_down = {false};
            std::atomic<bool> closed = {false};
            std::atomic<uint64_t> sequence = {0};

            void link(const tracked_ptr<Pop3ServerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                n->next = connections;
                if (connections) {
                    connections->prev = n;
                }
                connections = n;
            }

            void unlink(const tracked_ptr<Pop3ServerConn>& n) noexcept {
                std::lock_guard<std::mutex> g(lock);
                if (n->prev) {
                    n->prev->next = n->next;
                } else if (connections == n) {
                    connections = n->next;
                }
                if (n->next) {
                    n->next->prev = n->prev;
                }
                n->prev = tracked_ptr<Pop3ServerConn>();
                n->next = tracked_ptr<Pop3ServerConn>();
            }

            vector<tracked_ptr<Pop3ServerConn>> snapshot() noexcept {
                std::lock_guard<std::mutex> g(lock);
                vector<tracked_ptr<Pop3ServerConn>> all;
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

            bool lock_user(const std::string& user) {
                std::lock_guard<std::mutex> g(lock);
                return locked.insert(user).second;
            }

            void unlock_user(const std::string& user) {
                std::lock_guard<std::mutex> g(lock);
                locked.erase(user);
            }
        };

        // One session's state machine (RFC 1939 §3–§6): commands in, replies
        // out, the backend asked on the way; the task around it reads,
        // writes, upgrades to TLS and closes
        struct Pop3Session {
            enum class State : uint8_t { authorization, transaction, done };
            enum class Step : uint8_t { more, starttls, close };

            struct Message {
                uint32_t uid = 0;
                uint64_t size = 0;
                bool deleted = false;
            };

            tracked_ptr<Pop3ServerSettings> cfg;
            tracked_ptr<Pop3ServerImpl> server;
            State state = State::authorization;
            bool tls = false;
            bool sasl_plain = false;     // AUTH PLAIN waits for its response line
            bool locked = false;
            int failures = 0;
            std::string pending_user;    // USER's name, for PASS
            std::string user;            // the user logged in
            std::string timestamp;       // APOP's "<...>" of the greeting
            uint32_t uid_validity = 1;
            std::vector<Message> messages;
            std::string out;

            Pop3Session(tracked_ptr<Pop3ServerSettings> c, tracked_ptr<Pop3ServerImpl> s, bool implicit_tls)
            : cfg(std::move(c))
            , server(std::move(s))
            , tls(implicit_tls) {
            }

            // The maildrop let go at the session's end (never left to the
            // collector: the next login must not wait for a sweep)
            void release() {
                if (locked) {
                    server->unlock_user(user);
                    locked = false;
                }
            }

            bool clear_text_refused() const noexcept {
                return cfg->tls && !tls && !cfg->allow_insecure_auth;
            }

            void ok(std::string_view text) {
                out += "+OK";
                if (!text.empty()) {
                    out += ' ';
                    out.append(text);
                }
                out += "\r\n";
            }

            void err(std::string_view text) {
                out += "-ERR ";
                out.append(text);
                out += "\r\n";
            }

            void greet() {
                std::string g(cfg->greeting.view());
                if (cfg->apop_secret) {
                    uint64_t n = server->sequence.fetch_add(1);
                    timestamp = "<" + std::to_string(::getpid()) + "." + std::to_string(n) + "." +
                                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) +
                                "@" + std::string(cfg->hostname.view()) + ">";
                    g += " " + timestamp;
                }
                ok(g);
            }

            void capabilities() {
                ok("Capability list follows");
                out += "TOP\r\nUIDL\r\nRESP-CODES\r\nAUTH-RESP-CODE\r\nPIPELINING\r\nEXPIRE NEVER\r\nLOGIN-DELAY 0\r\n";
                if (state == State::authorization) {
                    if (!clear_text_refused()) {
                        out += "USER\r\nSASL PLAIN\r\n";
                    }
                    if (cfg->tls && !tls) {
                        out += "STLS\r\n";
                    }
                }
                out += "IMPLEMENTATION sgcl\r\n.\r\n";
            }

            bool password_ok(const std::string& u, const std::string& p) {
                if (cfg->check_password) {
                    return cfg->check_password(string(u), string(p));
                }
                return cfg->backend->has_authenticate() && cfg->backend->authenticate(string(u), string(p));
            }

            // The maildrop taken: locked, the INBOX read
            Step login(const std::string& u) {
                if (!server->lock_user(u)) {
                    err("[IN-USE] Maildrop already locked");
                    return Step::more;
                }
                locked = true;
                user = u;
                auto box = cfg->backend->open(string(u), string("INBOX"));
                if (!box) {
                    cfg->report(string::concat("open INBOX of ", string(u), ": ", box.error().message()));
                    err("[SYS/TEMP] Maildrop unavailable");
                    server->unlock_user(u);
                    locked = false;
                    return Step::more;
                }
                uid_validity = box->uid_validity;
                messages.clear();
                uint64_t total = 0;
                for (auto& m : box->messages) {
                    messages.push_back(Message{m.uid, m.size, false});
                    total += m.size;
                }
                state = State::transaction;
                ok("Maildrop has " + std::to_string(messages.size()) + " messages (" + std::to_string(total) + " octets)");
                return Step::more;
            }

            Step failed_login() {
                if (cfg->max_auth_failures > 0 && ++failures >= cfg->max_auth_failures) {
                    err("[AUTH] Too many failures");
                    return Step::close;
                }
                err("[AUTH] Authentication failed");
                return Step::more;
            }

            // A message number of the session: 1..n, not marked deleted
            Message* number(std::string_view arg) {
                if (arg.empty() || arg.size() > 10) {
                    return nullptr;
                }
                uint64_t n = 0;
                for (char c : arg) {
                    if (c < '0' || c > '9') {
                        return nullptr;
                    }
                    n = n * 10 + uint64_t(c - '0');
                }
                if (n == 0 || n > messages.size() || messages[n - 1].deleted) {
                    return nullptr;
                }
                return &messages[n - 1];
            }

            std::string uid_of(const Message& m) const {
                return std::to_string(uid_validity) + "." + std::to_string(m.uid);
            }

            Step sasl_response(std::string_view line) {
                sasl_plain = false;
                if (line == "*") {
                    err("Authentication cancelled");
                    return Step::more;
                }
                auto decoded = encoding::base64::standard.decode(string(line));
                if (!decoded) {
                    err("Invalid base64");
                    return Step::more;
                }
                std::string_view v(reinterpret_cast<const char*>(decoded->data()), decoded->size());
                size_t a = v.find('\0');
                size_t b = a == std::string_view::npos ? a : v.find('\0', a + 1);
                if (b == std::string_view::npos) {
                    return failed_login();
                }
                std::string authz(v.substr(0, a)), authc(v.substr(a + 1, b - a - 1)), pass(v.substr(b + 1));
                if ((!authz.empty() && authz != authc) || !password_ok(authc, pass)) {
                    return failed_login();
                }
                return login(authc);
            }

            Step command(std::string_view line) {
                if (sasl_plain) {
                    return sasl_response(line);
                }
                size_t sp = line.find(' ');
                std::string_view verb = line.substr(0, sp);
                std::string_view args = sp == std::string_view::npos ? std::string_view() : line.substr(sp + 1);
                std::string_view arg1 = args.substr(0, args.find(' '));
                std::string_view arg2 = args.find(' ') == std::string_view::npos ? std::string_view() : args.substr(args.find(' ') + 1);
                auto is = [&](std::string_view v) { return pop3_iequal(verb, v); };
                if (is("CAPA")) {
                    capabilities();
                    return Step::more;
                }
                if (is("QUIT")) {
                    if (state == State::transaction) {
                        vector<uint32_t> gone;
                        for (auto& m : messages) {
                            if (m.deleted) {
                                gone.push_back(m.uid);
                            }
                        }
                        if (!gone.empty()) {
                            auto r = cfg->backend->expunge(string(user), string("INBOX"), gone);
                            if (!r) {
                                cfg->report(string::concat("expunge INBOX of ", string(user), ": ", r.error().message()));
                                err("[SYS/TEMP] Some deleted messages not removed");
                                state = State::done;
                                return Step::close;
                            }
                        }
                    }
                    state = State::done;
                    release();   // before the reply: a client that logs in again at once finds the maildrop free
                    ok("Bye");
                    return Step::close;
                }
                if (state == State::authorization) {
                    if (is("STLS")) {
                        if (!cfg->tls || tls) {
                            err(tls ? "Already in TLS" : "STLS not available");
                            return Step::more;
                        }
                        ok("Begin TLS negotiation");
                        return Step::starttls;
                    }
                    if ((is("USER") || is("PASS") || is("APOP") || is("AUTH")) && clear_text_refused()) {
                        err("[AUTH] Credentials only over TLS: use STLS first");
                        return Step::more;
                    }
                    if (is("USER")) {
                        if (args.empty()) {
                            err("Missing user name");
                            return Step::more;
                        }
                        pending_user = std::string(args);
                        ok("Send PASS");
                        return Step::more;
                    }
                    if (is("PASS")) {
                        if (pending_user.empty()) {
                            err("USER first");
                            return Step::more;
                        }
                        std::string u = pending_user;
                        pending_user.clear();
                        if (!password_ok(u, std::string(args))) {
                            return failed_login();
                        }
                        return login(u);
                    }
                    if (is("APOP")) {
                        if (!cfg->apop_secret || arg2.size() != 32) {
                            if (!cfg->apop_secret) {
                                err("APOP not available");
                                return Step::more;
                            }
                            return failed_login();
                        }
                        auto secret = cfg->apop_secret(string(arg1));
                        if (!secret) {
                            return failed_login();
                        }
                        std::string want = md5_hex(timestamp + std::string(secret->view()));
                        unsigned diff = 0;
                        for (size_t i = 0; i < 32; ++i) {
                            char c = arg2[i];
                            c = char(c + (unsigned(c - 'A') < 6u) * 32);
                            diff |= unsigned(c != want[i]);
                        }
                        if (diff) {
                            return failed_login();
                        }
                        return login(std::string(arg1));
                    }
                    if (is("AUTH")) {
                        if (args.empty()) {
                            ok("Methods follow");
                            out += "PLAIN\r\n.\r\n";
                            return Step::more;
                        }
                        if (!pop3_iequal(arg1, "PLAIN")) {
                            err("Unsupported mechanism");
                            return Step::more;
                        }
                        if (!arg2.empty()) {
                            return sasl_response(arg2 == "=" ? std::string_view() : arg2);
                        }
                        sasl_plain = true;
                        out += "+ \r\n";
                        return Step::more;
                    }
                    err("Not in this state");
                    return Step::more;
                }
                // TRANSACTION
                if (is("STAT")) {
                    size_t n = 0;
                    uint64_t size = 0;
                    for (auto& m : messages) {
                        if (!m.deleted) {
                            ++n;
                            size += m.size;
                        }
                    }
                    ok(std::to_string(n) + " " + std::to_string(size));
                    return Step::more;
                }
                if (is("LIST") || is("UIDL")) {
                    bool uidl = is("UIDL");
                    if (!args.empty()) {
                        Message* m = number(arg1);
                        if (!m) {
                            err("No such message");
                            return Step::more;
                        }
                        ok(std::string(arg1) + " " + (uidl ? uid_of(*m) : std::to_string(m->size)));
                        return Step::more;
                    }
                    ok(uidl ? "Unique ids follow" : "Scan listing follows");
                    for (size_t i = 0; i < messages.size(); ++i) {
                        if (!messages[i].deleted) {
                            out += std::to_string(i + 1);
                            out += ' ';
                            out += uidl ? uid_of(messages[i]) : std::to_string(messages[i].size);
                            out += "\r\n";
                        }
                    }
                    out += ".\r\n";
                    return Step::more;
                }
                if (is("RETR") || is("TOP")) {
                    bool top = is("TOP");
                    Message* m = number(arg1);
                    uint64_t lines = 0;
                    if (top) {
                        bool good = !arg2.empty() && arg2.size() <= 10;
                        for (char c : good ? arg2 : std::string_view()) {
                            good &= c >= '0' && c <= '9';
                            lines = lines * 10 + uint64_t(c - '0');
                        }
                        if (!good) {
                            err("Syntax: TOP msg n");
                            return Step::more;
                        }
                    }
                    if (!m) {
                        err("No such message");
                        return Step::more;
                    }
                    auto text = cfg->backend->read(string(user), string("INBOX"), m->uid);
                    if (!text) {
                        err("[SYS/TEMP] The message cannot be read");
                        return Step::more;
                    }
                    std::string_view t = text->view();
                    if (top) {
                        // the head, the empty line, then n lines of the body
                        size_t head = t.find("\r\n\r\n");
                        size_t cut = head == std::string_view::npos ? t.size() : head + 4;
                        if (head == std::string_view::npos) {
                            size_t lf = t.find("\n\n");
                            cut = lf == std::string_view::npos ? t.size() : lf + 2;
                        }
                        size_t p = cut;
                        for (uint64_t k = 0; k < lines && p < t.size(); ++k) {
                            size_t nl = t.find('\n', p);
                            p = nl == std::string_view::npos ? t.size() : nl + 1;
                        }
                        t = t.substr(0, p);
                        ok("Top of message follows");
                    } else {
                        ok(std::to_string(m->size) + " octets");
                    }
                    pop3_put_multiline(out, t);
                    return Step::more;
                }
                if (is("DELE")) {
                    Message* m = number(arg1);
                    if (!m) {
                        err("No such message");
                        return Step::more;
                    }
                    m->deleted = true;
                    ok("Marked to be deleted");
                    return Step::more;
                }
                if (is("RSET")) {
                    for (auto& m : messages) {
                        m.deleted = false;
                    }
                    ok("Maildrop has " + std::to_string(messages.size()) + " messages");
                    return Step::more;
                }
                if (is("NOOP")) {
                    ok("");
                    return Step::more;
                }
                err(is("STLS") || is("USER") || is("PASS") || is("APOP") || is("AUTH") ? "Not in this state" : "Unknown command");
                return Step::more;
            }
        };

        inline async::task<> pop3_serve_connection(tracked_ptr<Pop3ServerImpl> s, tracked_ptr<Pop3ServerSettings> cfg, net::connection c, bool implicit_tls) noexcept {
            tracked_ptr node = make_tracked<Pop3ServerConn>(c);
            s->link(node);
            const size_t active = s->active.fetch_add(1) + 1;
            tracked_ptr session = make_tracked<Pop3Session>(cfg, s, implicit_tls);
            tracked_ptr wire = make_tracked<Pop3Wire>(c);
            if (cfg->max_connections && active > cfg->max_connections) {
                session->err("[SYS/TEMP] Too many connections");
                (void)co_await pop3_write(c, session->out);
            } else {
                session->greet();
                Pop3Session::Step step = Pop3Session::Step::more;
                std::string line;
                for (;;) {
                    // every whole line answered, the replies written together
                    for (;;) {
                        bool too_long = false;
                        if (!wire->take_line(line, 4096, too_long)) {
                            if (too_long) {
                                session->err("Line too long");
                                step = Pop3Session::Step::close;
                            }
                            break;
                        }
                        step = session->command(line);
                        if (step != Pop3Session::Step::more || session->out.size() >= 65536) {
                            break;   // a long batch's replies written as they grow, the client reading meanwhile
                        }
                    }
                    if (!session->out.empty()) {
                        // what the socket takes at once without a frame (net's start_write);
                        // out is not touched until the rest is written
                        c.set_write_deadline(sgcl::clock::now() + cfg->idle_timeout);
                        auto st = net::detail::ConnectionAccess::impl(c).start_write(
                            slice<const byte>(reinterpret_cast<const byte*>(session->out.data()), session->out.size()));
                        expected<size_t, io::error> w = std::move(st.done);
                        if (st.rest) {
                            w = co_await std::move(*st.rest);
                        }
                        session->out.clear();
                        if (!w) {
                            break;
                        }
                    }
                    if (step == Pop3Session::Step::close) {
                        break;
                    }
                    if (step == Pop3Session::Step::starttls) {
                        wire->discard();   // what came pipelined in clear text is never part of the session
                        c.set_deadline(time_point());
                        auto t = co_await net::tls::async_server(c, *cfg->tls);
                        if (!t) {
                            break;
                        }
                        c = *t;   // the node keeps the raw connection: closing it ends this one
                        wire = make_tracked<Pop3Wire>(c);
                        session->tls = true;
                        step = Pop3Session::Step::more;
                        continue;
                    }
                    if (wire->buffered() && std::string_view(wire->buf).substr(wire->at).find('\n') != std::string_view::npos) {
                        continue;   // whole commands wait already: answered before more is read
                    }
                    if (s->shutting_down.load()) {
                        session->err("[SYS/TEMP] Server shutting down");
                        (void)co_await pop3_write(c, session->out);
                        break;
                    }
                    node->state.store(Pop3ServerConn::idle);
                    if (s->shutting_down.load()) {
                        break;
                    }
                    c.set_read_deadline(sgcl::clock::now() + cfg->idle_timeout);
                    expected<size_t, io::error> r = size_t(0);
                    for (;;) {   // the read without a frame of its own (net's try_read), as smtp's server reads
                        auto t = wire->try_fill();
                        if (t.done) {
                            r = std::move(t.result);
                            break;
                        }
                        if (t.slow) {
                            r = co_await wire->fill();
                            break;
                        }
                        if (auto ready = co_await t.ready; !ready) {
                            r = net::detail::fail(ready);
                            break;
                        }
                    }
                    int expected_state = Pop3ServerConn::idle;
                    if (!node->state.compare_exchange_strong(expected_state, Pop3ServerConn::active)) {
                        break;   // the shutdown took it while it waited
                    }
                    if (!r || *r == 0) {
                        break;   // an end without QUIT: nothing expunged (RFC 1939 §6)
                    }
                }
            }
            (void)co_await c.async_close();
            session->release();
            s->active.fetch_sub(1);
            s->unlink(node);
            s->running.done();
            co_return;
        }
    }

    // A POP3 server (RFC 1939 with RFC 2449's CAPA, RESP-CODES and
    // PIPELINING, STLS of RFC 2595, AUTH PLAIN of RFC 5034, APOP): the
    // INBOX of each user of an imap::backend, the store an imap::server
    // serves too. A handle of one word: copies share the connections. The
    // fields are read when serve() is called.
    //
    //     net::imap::memory_backend mail;
    //     mail.add_user("alice", "secret");
    //     net::pop3::server srv;
    //     srv.backend = mail;
    //     srv.serve(":110");
    //
    // A session sees the INBOX as it was at its login; DELE marks, QUIT
    // removes what was marked; a connection that ends without QUIT removes
    // nothing. One session per user at a time ([IN-USE]).
    class server {
    public:
        server() noexcept
        : _impl(make_tracked<detail::Pop3ServerImpl>()) {
        }

        server(const server&) = default;
        server& operator=(const server&) = default;

        // Listens on the address (":110") and serves until shutdown() or
        // close(): then net::errc::server_closed. STLS is offered when tls
        // is set
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // Listens over TLS from the first byte (port 995, RFC 8314)
        expected<void, io::error> serve_tls(const string& address, const net::tls::config& c) const {
            return async_serve_tls(address, c).wait();
        }

        async::task<expected<void, io::error>> async_serve_tls(const string& address, const net::tls::config& c) const noexcept {
            return _co_serve_tls(_impl, _settings(), address, c);
        }

        // The connections of a listener the program made (a TLS listener's
        // are taken as TLS from the start)
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return _co_serve(_impl, _settings(), l);
        }

        // Gracefully: the listeners closed, the sessions waiting for a
        // command closed, the others after the command they are in; returns
        // when all have ended
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
                n->state.store(detail::Pop3ServerConn::closed);
                (void)n->c.close();
            }
        }

        // The number of connections being served
        size_t connections() const noexcept {
            return _impl->active.load();
        }

        imap::backend backend;                                         // the users' INBOX; a memory_backend of the server's own by default
        function<bool(const string&, const string&)> check_password;   // USER/PASS and AUTH PLAIN; empty: the backend's authenticate
        function<optional<string>(const string&)> apop_secret;         // a user's shared secret for APOP; APOP offered when set
        optional<net::tls::config> tls;                                // STLS offered; credentials refused before it
        bool allow_insecure_auth = false;                              // credentials before STLS all the same
        duration idle_timeout = 10 * minute;                           // a silent connection closed (RFC 1939 §3: at least 10 minutes)
        size_t max_connections = 0;                                    // past it -ERR [SYS/TEMP] and the end; zero: none
        int max_auth_failures = 3;                                     // failed logins before the connection is closed; zero: none
        string greeting = string("POP3 server ready");                 // the greeting's text
        string hostname;                                               // APOP's timestamp's host; empty: this host's name
        function<void(const string&)> on_error;                        // a backend's failure, an accept's; a line on stderr by default

    private:
        tracked_ptr<detail::Pop3ServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::Pop3ServerSettings>();
            cfg->backend = imap::detail::BackendAccess::get(backend);
            cfg->check_password = check_password;
            cfg->apop_secret = apop_secret;
            cfg->tls = tls;
            cfg->allow_insecure_auth = allow_insecure_auth;
            cfg->idle_timeout = idle_timeout > duration::zero() ? idle_timeout : 10 * minute;
            cfg->max_connections = max_connections;
            cfg->max_auth_failures = max_auth_failures;
            cfg->greeting = greeting;
            if (!hostname.empty()) {
                cfg->hostname = hostname;
            } else {
                char name[256] = {};
                cfg->hostname = ::gethostname(name, sizeof name - 1) == 0 && name[0] ? string(name) : string("localhost");
            }
            cfg->on_error = on_error;
            return cfg;
        }

        static async::task<expected<void, io::error>> _co_serve(tracked_ptr<detail::Pop3ServerImpl> impl, tracked_ptr<detail::Pop3ServerSettings> cfg, net::listener l) noexcept {
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
                    if (!async::detail::runtime_exiting()) {
                        cfg->report(string("accept: ") + c.error().message());
                    }
                    co_return io::detail::fail(c);
                }
                impl->running.add();
                async::go(detail::pop3_serve_connection(impl, cfg, *c, net::tls::state_of(*c).has_value()));
            }
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::Pop3ServerImpl> impl, tracked_ptr<detail::Pop3ServerSettings> cfg, string address) noexcept {
            auto l = co_await net::tcp::async_listen(address);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<expected<void, io::error>> _co_serve_tls(tracked_ptr<detail::Pop3ServerImpl> impl, tracked_ptr<detail::Pop3ServerSettings> cfg, string address,
                                                                    net::tls::config c) noexcept {
            auto l = co_await net::tls::async_listen(address, c);
            if (!l) {
                co_return io::detail::fail(l);
            }
            co_return co_await _co_serve(impl, cfg, *l);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::Pop3ServerImpl> impl) noexcept {
            impl->shutting_down.store(true);
            impl->close_listeners();
            for (auto& n : impl->snapshot()) {
                int expected_state = detail::Pop3ServerConn::idle;
                if (n->state.compare_exchange_strong(expected_state, detail::Pop3ServerConn::closed)) {
                    std::string bye = "-ERR [SYS/TEMP] Server shutting down\r\n";
                    (void)co_await detail::pop3_write(n->c, bye);
                    (void)co_await n->c.async_close();
                }
            }
            co_await impl->running;
        }

        tracked_ptr<detail::Pop3ServerImpl> _impl;
    };
}
