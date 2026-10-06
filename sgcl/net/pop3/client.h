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
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/duration.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../encoding/base64.h"

#include <chrono>
#include <string>
#include <string_view>

// The client of POP3 (RFC 1939) with CAPA and PIPELINING (RFC 2449), STLS
// (RFC 2595), AUTH PLAIN (RFC 5034), APOP, TOP and UIDL
namespace sgcl::net::pop3 {
    class client;

    namespace detail {
        inline constexpr size_t MaxReplyLine = 8192;
        inline constexpr size_t MaxMessage = size_t(1) << 30;

        struct Pop3ClientState {
            tracked_ptr<Pop3Wire> wire;
            string server;         // "host:port", for the errors
            string greeting;
            string timestamp;      // APOP's "<...>" of the greeting
            vector<string> caps;
            bool tls = false;
            bool broken = false;
            duration timeout = 30 * second;
        };

        struct Pop3Reply {
            bool ok = false;
            std::string text;      // after "+OK " or "-ERR "
        };

        inline time_point pop3_deadline(const Pop3ClientState& s) noexcept {
            return s.timeout > duration::zero() ? net::detail::no_deadline_at_max(sgcl::clock::now() + s.timeout) : time_point();
        }

        inline io::error pop3_broken(const Pop3ClientState& s) noexcept {
            return io::error(io::errc::closed, "pop3", s.server);
        }

        // One status line: +OK or -ERR (or "+ " of a SASL challenge, taken as +OK)
        inline async::task<expected<Pop3Reply, io::error>> pop3_reply(tracked_ptr<Pop3ClientState> s, const char* op) noexcept {
            tracked_ptr<Pop3Wire> w = s->wire;
            w->c.set_read_deadline(pop3_deadline(*s));
            std::string line;
            for (;;) {
                bool too_long = false;
                if (w->take_line(line, MaxReplyLine, too_long)) {
                    break;
                }
                if (too_long) {
                    s->broken = true;
                    co_return unexpected(pop3_error(errc::malformed_response, string::concat("pop3 ", op), string("a reply line too long")));
                }
                expected<size_t, io::error> r = size_t(0);
                for (;;) {   // the read without a frame of its own (net's try_read)
                    auto t = w->try_fill();
                    if (t.done) {
                        r = std::move(t.result);
                        break;
                    }
                    if (t.slow) {
                        r = co_await w->fill();
                        break;
                    }
                    if (auto ready = co_await t.ready; !ready) {
                        r = net::detail::fail(ready);
                        break;
                    }
                }
                if (!r) {
                    s->broken = true;
                    co_return unexpected(io::error(r.error().code(), string::concat("pop3 ", op), s->server));
                }
                if (*r == 0) {
                    s->broken = true;
                    co_return unexpected(io::error(io::errc::unexpected_eof, string::concat("pop3 ", op), s->server));
                }
            }
            Pop3Reply rep;
            std::string_view v = line;
            if (v.substr(0, 3) == "+OK") {
                rep.ok = true;
                v.remove_prefix(3);
            } else if (v.substr(0, 4) == "-ERR") {
                v.remove_prefix(4);
            } else if (v.substr(0, 1) == "+") {
                rep.ok = true;
                v.remove_prefix(1);
            } else {
                s->broken = true;
                co_return unexpected(pop3_error(errc::malformed_response, string::concat("pop3 ", op), string(line)));
            }
            if (!v.empty() && v.front() == ' ') {
                v.remove_prefix(1);
            }
            rep.text = std::string(v);
            co_return rep;
        }

        // The body of a multi-line response
        inline async::task<expected<std::string, io::error>> pop3_body(tracked_ptr<Pop3ClientState> s, const char* op) noexcept {
            tracked_ptr<Pop3Wire> w = s->wire;
            w->c.set_read_deadline(pop3_deadline(*s));
            std::string out;
            for (;;) {
                bool too_long = false;
                if (w->take_multiline(out, too_long, MaxMessage)) {
                    co_return out;
                }
                if (too_long) {
                    s->broken = true;
                    co_return unexpected(pop3_error(errc::malformed_response, string::concat("pop3 ", op), string("a response too long")));
                }
                expected<size_t, io::error> r = size_t(0);
                for (;;) {   // the read without a frame of its own (net's try_read)
                    auto t = w->try_fill();
                    if (t.done) {
                        r = std::move(t.result);
                        break;
                    }
                    if (t.slow) {
                        r = co_await w->fill();
                        break;
                    }
                    if (auto ready = co_await t.ready; !ready) {
                        r = net::detail::fail(ready);
                        break;
                    }
                }
                if (!r) {
                    s->broken = true;
                    co_return unexpected(io::error(r.error().code(), string::concat("pop3 ", op), s->server));
                }
                if (*r == 0) {
                    s->broken = true;
                    co_return unexpected(io::error(io::errc::unexpected_eof, string::concat("pop3 ", op), s->server));
                }
            }
        }

        inline async::task<expected<void, io::error>> pop3_send(tracked_ptr<Pop3ClientState> s, std::string text) noexcept {
            s->wire->c.set_write_deadline(pop3_deadline(*s));
            // what the socket takes at once without a frame (net's start_write)
            auto st = net::detail::ConnectionAccess::impl(s->wire->c).start_write(slice<const byte>(reinterpret_cast<const byte*>(text.data()), text.size()));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                s->broken = true;
                co_return unexpected(w.error());
            }
            co_return expected<void, io::error>();
        }

        inline io::error pop3_refused(errc fallback, const char* op, const std::string& text) noexcept {
            return pop3_error(code_of_reply(text, fallback), string::concat("pop3 ", op), string(text));
        }

        // A command and its status line; a -ERR is the error
        inline async::task<expected<std::string, io::error>> pop3_command(tracked_ptr<Pop3ClientState> s, std::string line, const char* op, errc refusal = errc::err) noexcept {
            if (s->broken) {
                co_return unexpected(pop3_broken(*s));
            }
            line += "\r\n";
            s->wire->c.set_write_deadline(pop3_deadline(*s));
            auto st = net::detail::ConnectionAccess::impl(s->wire->c).start_write(slice<const byte>(reinterpret_cast<const byte*>(line.data()), line.size()));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                s->broken = true;
                co_return unexpected(w.error());
            }
            auto r = co_await pop3_reply(s, op);
            if (!r) {
                co_return unexpected(r.error());
            }
            if (!r->ok) {
                co_return unexpected(pop3_refused(refusal, op, r->text));
            }
            co_return std::move(r->text);
        }

        // A command whose +OK a multi-line body follows
        inline async::task<expected<std::string, io::error>> pop3_multi(tracked_ptr<Pop3ClientState> s, std::string line, const char* op, errc refusal = errc::err) noexcept {
            auto r = co_await pop3_command(s, std::move(line), op, refusal);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return co_await pop3_body(s, op);
        }

        inline bool pop3_has(const Pop3ClientState& s, std::string_view cap) noexcept {
            for (auto& c : s.caps) {
                std::string_view v = c.view();
                std::string_view word = v.substr(0, v.find(' '));
                if (pop3_iequal(word, cap) || pop3_iequal(v, cap)) {
                    return true;
                }
            }
            return false;
        }

        inline bool pop3_sasl_plain(const Pop3ClientState& s) noexcept {
            for (auto& c : s.caps) {
                std::string_view v = c.view();
                if (v.size() > 5 && pop3_iequal(v.substr(0, 5), "SASL ")) {
                    for (size_t at = 5; at < v.size();) {
                        size_t sp = v.find(' ', at);
                        if (pop3_iequal(v.substr(at, sp == std::string_view::npos ? std::string_view::npos : sp - at), "PLAIN")) {
                            return true;
                        }
                        at = sp == std::string_view::npos ? v.size() : sp + 1;
                    }
                }
            }
            return false;
        }

        inline async::task<expected<void, io::error>> pop3_capa(tracked_ptr<Pop3ClientState> s) noexcept {
            s->caps.clear();
            if (auto w = co_await pop3_send(s, "CAPA\r\n"); !w) {
                co_return unexpected(w.error());
            }
            auto r = co_await pop3_reply(s, "CAPA");
            if (!r) {
                co_return unexpected(r.error());
            }
            if (!r->ok) {
                co_return expected<void, io::error>();   // a server of RFC 1939 alone
            }
            auto body = co_await pop3_body(s, "CAPA");
            if (!body) {
                co_return unexpected(body.error());
            }
            std::string_view v = *body;
            for (size_t at = 0; at < v.size();) {
                size_t eol = v.find("\r\n", at);
                if (eol > at) {
                    s->caps.push_back(string(v.substr(at, eol - at)));
                }
                at = eol + 2;
            }
            co_return expected<void, io::error>();
        }

        // The words of a line: "1 4096", "1 uid"
        inline bool pop3_two(std::string_view v, uint64_t& n, std::string_view& rest) noexcept {
            size_t sp = v.find(' ');
            if (sp == 0 || sp == std::string_view::npos || sp > 10) {
                return false;
            }
            n = 0;
            for (char c : v.substr(0, sp)) {
                if (c < '0' || c > '9') {
                    return false;
                }
                n = n * 10 + uint64_t(c - '0');
            }
            rest = v.substr(sp + 1);
            size_t end = rest.find(' ');
            rest = rest.substr(0, end);
            return !rest.empty();
        }

        inline bool pop3_number(std::string_view v, uint64_t& n) noexcept {
            if (v.empty() || v.size() > 19) {
                return false;
            }
            n = 0;
            for (char c : v) {
                if (c < '0' || c > '9') {
                    return false;
                }
                n = n * 10 + uint64_t(c - '0');
            }
            return true;
        }

        struct Pop3Login {
            string user;
            string password;
            mechanism how = mechanism::automatic;
        };

        inline async::task<expected<void, io::error>> pop3_login(tracked_ptr<Pop3ClientState> s, Pop3Login l) noexcept {
            mechanism how = l.how;
            if (how == mechanism::automatic) {
                how = pop3_sasl_plain(*s) ? mechanism::plain : mechanism::user;
            }
            if (how == mechanism::apop) {
                if (s->timestamp.empty()) {
                    co_return unexpected(pop3_error(errc::not_supported, "pop3 APOP", string("no timestamp in the greeting")));
                }
                std::string digest = md5_hex(std::string(s->timestamp.view()) + std::string(l.password.view()));
                auto r = co_await pop3_command(s, "APOP " + std::string(l.user.view()) + " " + digest, "APOP", errc::authentication_failed);
                if (!r) {
                    co_return unexpected(r.error());
                }
                co_return expected<void, io::error>();
            }
            if (how == mechanism::plain) {
                std::string token;
                token += '\0';
                token += l.user.view();
                token += '\0';
                token += l.password.view();
                string b64 = encoding::base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(token.data()), token.size()));
                auto r = co_await pop3_command(s, "AUTH PLAIN " + std::string(b64.view()), "AUTH", errc::authentication_failed);
                if (!r) {
                    co_return unexpected(r.error());
                }
                co_return expected<void, io::error>();
            }
            auto u = co_await pop3_command(s, "USER " + std::string(l.user.view()), "USER", errc::authentication_failed);
            if (!u) {
                co_return unexpected(u.error());
            }
            auto p = co_await pop3_command(s, "PASS " + std::string(l.password.view()), "PASS", errc::authentication_failed);
            if (!p) {
                co_return unexpected(p.error());
            }
            co_return expected<void, io::error>();
        }

        struct ClientAccess;
    }

    // A session with a POP3 server: the greeting, STLS, the login, then
    // the maildrop's messages listed, read and marked for deletion, QUIT
    // to remove them. A handle of one word, as a connection is: a copy is
    // the same session. One command at a time; a batch of retrieve is
    // pipelined when the server says PIPELINING.
    //
    //     auto c = net::pop3::client::connect("pop3s://alice:secret@mail.example.com");
    //     for (auto& m : *c->list()) {
    //         string text = *c->retrieve(m.number);
    //         c->remove(m.number);
    //     }
    //     c->quit();
    class client {
    public:
        // How a client talks to its server. The credentials come from here
        // or from the URL's user and password
        struct options {
            string user;                                     // the login; from the URL's user name when it has one
            string password;                                 // from the URL's password
            pop3::mechanism mechanism = pop3::mechanism::automatic;
            pop3::security security = pop3::security::automatic;
            net::tls::config tls;                            // pop3s:// and STLS; the server's name the address's host when none is set
            duration timeout = std::chrono::seconds(30);     // the connect and the login together; then each reply's wait; zero: none
            async::stop_token stop;                          // the connect cancelled
        };

        client() noexcept = default;   // no session; an operation on it is a contract violation

        // A session with the server of the address: "host[:port]" (110;
        // 995 with security::tls) or a pop3:// or pop3s:// URL with the
        // user and the password; logged in when there are credentials
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& address) {
            return async_connect(address, options()).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string address) noexcept {
            return _co_connect(std::move(address), net::connection(), options());
        }

        static expected<client, io::error> connect(const string& address, const options& o) {
            return async_connect(address, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string address, options o) noexcept {
            return _co_connect(std::move(address), net::connection(), std::move(o));
        }

        // The same over a connection there is (a test's pipe in memory, a
        // tunnel); TLS as the options say
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            return _co_connect(string(), std::move(transport), std::move(o));
        }

        // The greeting's text ("POP3 server ready <1896.697170952@host>")
        string greeting() const noexcept {
            return _s->greeting;
        }

        // CAPA's lines ("TOP", "UIDL", "SASL PLAIN", "PIPELINING"); empty
        // for a server of RFC 1939 alone
        vector<string> capabilities() const {
            return _s->caps;
        }

        // Whether CAPA named the capability (its first word)
        bool has(const string& capability) const noexcept {
            return detail::pop3_has(*_s, capability.view());
        }

        // Whether the session runs over TLS (pop3s://, or after STLS)
        bool is_tls() const noexcept {
            return _s->tls;
        }

        // STAT: the messages not marked deleted, and their size
        // `status()` on this thread, `co_await async_status()` in a task
        expected<mailbox_status, io::error> status() const {
            return async_status().wait();
        }

        async::task<expected<mailbox_status, io::error>> async_status() const noexcept {
            return _co_status(_s);
        }

        // LIST joined with UIDL (when the server has it): every message
        // not marked deleted, or the one of the number
        // `list(...)` on this thread, `co_await async_list(...)` in a task
        expected<vector<message_info>, io::error> list() const {
            return async_list().wait();
        }

        async::task<expected<vector<message_info>, io::error>> async_list() const noexcept {
            return _co_list(_s);
        }

        expected<message_info, io::error> list(uint32_t number) const {
            return async_list(number).wait();
        }

        async::task<expected<message_info, io::error>> async_list(uint32_t number) const noexcept {
            return _co_list_one(_s, number);
        }

        // RETR: the message of the number, its dots undone, CRLF line
        // breaks; several, pipelined when the server says PIPELINING
        // `retrieve(...)` on this thread, `co_await async_retrieve(...)` in a task
        expected<string, io::error> retrieve(uint32_t number) const {
            return async_retrieve(number).wait();
        }

        async::task<expected<string, io::error>> async_retrieve(uint32_t number) const noexcept {
            return _co_retrieve(_s, number);
        }

        expected<vector<string>, io::error> retrieve(const vector<uint32_t>& numbers) const {
            return async_retrieve(numbers).wait();
        }

        async::task<expected<vector<string>, io::error>> async_retrieve(vector<uint32_t> numbers) const noexcept {
            return _co_retrieve_all(_s, std::move(numbers));
        }

        // TOP: the head of the message and its first lines
        // `top(...)` on this thread, `co_await async_top(...)` in a task
        expected<string, io::error> top(uint32_t number, size_t lines = 0) const {
            return async_top(number, lines).wait();
        }

        async::task<expected<string, io::error>> async_top(uint32_t number, size_t lines = 0) const noexcept {
            return _co_top(_s, number, lines);
        }

        // DELE: the message marked, removed at quit()
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(uint32_t number) const {
            return async_remove(number).wait();
        }

        async::task<expected<void, io::error>> async_remove(uint32_t number) const noexcept {
            return _co_simple(_s, "DELE " + std::to_string(number), "DELE", errc::no_such_message);
        }

        // RSET: the marks taken off
        expected<void, io::error> reset() const {
            return async_reset().wait();
        }

        async::task<expected<void, io::error>> async_reset() const noexcept {
            return _co_simple(_s, "RSET", "RSET", errc::err);
        }

        // NOOP: the session kept alive
        expected<void, io::error> noop() const {
            return async_noop().wait();
        }

        async::task<expected<void, io::error>> async_noop() const noexcept {
            return _co_simple(_s, "NOOP", "NOOP", errc::err);
        }

        // QUIT: the marked messages removed, then the connection closed
        expected<void, io::error> quit() const {
            return async_quit().wait();
        }

        async::task<expected<void, io::error>> async_quit() const noexcept {
            return _co_quit(_s);
        }

        // The connection closed without QUIT: nothing removed
        expected<void, io::error> close() const noexcept {
            _s->broken = true;
            return _s->wire ? _s->wire->c.close() : expected<void, io::error>();
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::ClientAccess;

        explicit client(tracked_ptr<detail::Pop3ClientState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<expected<client, io::error>> _co_connect(string address, net::connection transport, options o) noexcept {
            std::string host, port;
            bool implicit = o.security == security::tls;
            std::string_view a = address.view();
            if (!transport) {
                if (a.size() >= 7 && (detail::pop3_iequal(a.substr(0, 7), "pop3://") || (a.size() >= 8 && detail::pop3_iequal(a.substr(0, 8), "pop3s://")))) {
                    auto u = net::url::parse(address);
                    if (!u || u->hostname().empty()) {
                        co_return unexpected(net::detail::net_error(net::errc::invalid_url, "pop3", address));
                    }
                    bool pop3s = detail::pop3_iequal(u->scheme().view(), "pop3s");
                    if (pop3s && o.security == security::automatic) {
                        implicit = true;
                    }
                    host = std::string(u->hostname().view());
                    if (u->host_address() && u->host_address()->is_v6()) {
                        host = "[" + host + "]";
                    }
                    port = u->port() ? std::to_string(*u->port()) : (implicit ? "995" : "110");
                    if (o.user.empty() && !u->username().empty()) {
                        o.user = string(net::detail::url_unescape(u->username().view()));
                    }
                    if (o.password.empty() && !u->password().empty()) {
                        o.password = string(net::detail::url_unescape(u->password().view()));
                    }
                } else {
                    size_t colon = a.rfind(':');
                    bool v6 = !a.empty() && a.front() == '[';
                    if (colon != std::string_view::npos && ((v6 && a.find(']') < colon) || (!v6 && a.find(':') == colon))) {
                        host = std::string(a.substr(0, colon));
                        port = std::string(a.substr(colon + 1));
                    } else {
                        host = std::string(a);
                        port = implicit ? "995" : "110";
                    }
                    if (port == "995" && o.security == security::automatic) {
                        implicit = true;
                    }
                }
                if (host.empty()) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_address, "pop3", address));
                }
            }
            tracked_ptr s = make_tracked<detail::Pop3ClientState>();
            s->server = transport ? transport.remote_endpoint().to_string() : string(host + ":" + port);
            s->timeout = o.timeout;
            const time_point deadline = o.timeout > duration::zero() ? sgcl::clock::now() + o.timeout : time_point();
            if (!transport) {
                async::stop_source dial_stop;
                if (o.timeout > duration::zero()) {
                    dial_stop.stop_after(o.timeout);
                }
                if (o.stop.stop_possible()) {
                    async::go(_link_stop(o.stop, dial_stop));
                }
                auto t = co_await net::tcp::async_connect(s->server, dial_stop.token());
                dial_stop.request_stop();
                if (!t) {
                    co_return unexpected(t.error());
                }
                transport = *t;
            }
            std::string bare = host.size() > 2 && host.front() == '[' ? host.substr(1, host.size() - 2) : host;
            net::tls::config tls = o.tls;
            if (tls.server_name.empty()) {
                tls.server_name = string(bare);
            }
            if (deadline != time_point()) {
                tls.handshake_timeout = std::max(duration(deadline - sgcl::clock::now()), duration(std::chrono::milliseconds(1)));
            }
            if (implicit) {
                auto t = co_await net::tls::async_client(transport, tls);
                if (!t) {
                    (void)transport.close();
                    co_return unexpected(t.error());
                }
                transport = *t;
                s->tls = true;
            }
            s->wire = make_tracked<detail::Pop3Wire>(transport);
            auto fail = [&](io::error e) -> expected<client, io::error> {
                (void)s->wire->c.close();
                return unexpected(std::move(e));
            };
            auto greet = co_await detail::pop3_reply(s, "greeting");
            if (!greet) {
                co_return fail(greet.error());
            }
            if (!greet->ok) {
                co_return fail(detail::pop3_refused(errc::err, "greeting", greet->text));
            }
            s->greeting = string(greet->text);
            size_t lt = greet->text.find('<');
            size_t gt = lt == std::string::npos ? lt : greet->text.find('>', lt);
            if (gt != std::string::npos && greet->text.find('@', lt) < gt) {
                s->timestamp = string(std::string_view(greet->text).substr(lt, gt - lt + 1));
            }
            if (auto c = co_await detail::pop3_capa(s); !c) {
                co_return fail(c.error());
            }
            const bool want_stls = !s->tls && (o.security == security::starttls || o.security == security::automatic);
            if (want_stls) {
                if (!detail::pop3_has(*s, "STLS")) {
                    co_return fail(detail::pop3_error(errc::starttls_unavailable, "pop3 STLS", s->server));
                }
                auto st = co_await detail::pop3_command(s, "STLS", "STLS");
                if (!st) {
                    co_return fail(st.error());
                }
                if (s->wire->buffered()) {
                    co_return fail(detail::pop3_error(errc::malformed_response, "pop3 STLS", string("data after the reply")));
                }
                s->wire->c.set_deadline(time_point());
                auto t = co_await net::tls::async_client(s->wire->c, tls);
                if (!t) {
                    co_return fail(t.error());
                }
                s->wire = make_tracked<detail::Pop3Wire>(*t);
                s->tls = true;
                if (auto c = co_await detail::pop3_capa(s); !c) {
                    co_return fail(c.error());
                }
            }
            if (!o.user.empty()) {
                if (o.mechanism == mechanism::plain && !detail::pop3_sasl_plain(*s) && !s->caps.empty()) {
                    co_return fail(detail::pop3_error(errc::not_supported, "pop3 AUTH", string("SASL PLAIN not offered")));
                }
                auto l = co_await detail::pop3_login(s, detail::Pop3Login{o.user, o.password, o.mechanism});
                if (!l) {
                    (void)co_await detail::pop3_send(s, "QUIT\r\n");
                    co_return fail(l.error());
                }
            }
            s->wire->c.set_deadline(time_point());
            co_return client(s);
        }

        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            auto t = to.token();
            co_await async::select(from.on_stop([&] { to.request_stop(); }), t.on_stop([] {}));
        }

        static async::task<expected<mailbox_status, io::error>> _co_status(tracked_ptr<detail::Pop3ClientState> s) noexcept {
            auto r = co_await detail::pop3_command(s, "STAT", "STAT");
            if (!r) {
                co_return unexpected(r.error());
            }
            uint64_t n = 0;
            std::string_view size;
            uint64_t bytes = 0;
            if (!detail::pop3_two(*r, n, size) || !detail::pop3_number(size, bytes)) {
                s->broken = true;
                co_return unexpected(detail::pop3_error(errc::malformed_response, "pop3 STAT", string(*r)));
            }
            co_return mailbox_status{size_t(n), bytes};
        }

        static async::task<expected<vector<message_info>, io::error>> _co_list(tracked_ptr<detail::Pop3ClientState> s) noexcept {
            const bool uidl = s->caps.empty() || detail::pop3_has(*s, "UIDL");
            const bool pipelined = detail::pop3_has(*s, "PIPELINING");
            if (pipelined && uidl) {
                if (auto w = co_await detail::pop3_send(s, "LIST\r\nUIDL\r\n"); !w) {
                    co_return unexpected(w.error());
                }
            } else if (auto w = co_await detail::pop3_send(s, "LIST\r\n"); !w) {
                co_return unexpected(w.error());
            }
            auto r = co_await detail::pop3_reply(s, "LIST");
            if (!r) {
                co_return unexpected(r.error());
            }
            expected<std::string, io::error> listing = std::string();
            if (r->ok) {
                listing = co_await detail::pop3_body(s, "LIST");
                if (!listing) {
                    co_return unexpected(listing.error());
                }
            }
            vector<message_info> out;
            if (r->ok) {
                std::string_view v = *listing;
                for (size_t at = 0; at < v.size();) {
                    size_t eol = v.find("\r\n", at);
                    uint64_t n = 0, size = 0;
                    std::string_view rest;
                    if (!detail::pop3_two(v.substr(at, eol - at), n, rest) || !detail::pop3_number(rest, size) || n == 0 || n > UINT32_MAX) {
                        s->broken = true;
                        co_return unexpected(detail::pop3_error(errc::malformed_response, "pop3 LIST", string(v.substr(at, eol - at))));
                    }
                    out.push_back(message_info{uint32_t(n), size, string()});
                    at = eol + 2;
                }
            }
            std::string err_text = r->text;
            if (uidl) {
                if (!pipelined && r->ok) {
                    if (auto w = co_await detail::pop3_send(s, "UIDL\r\n"); !w) {
                        co_return unexpected(w.error());
                    }
                }
                if (pipelined || r->ok) {
                    auto u = co_await detail::pop3_reply(s, "UIDL");
                    if (!u) {
                        co_return unexpected(u.error());
                    }
                    if (u->ok) {
                        auto body = co_await detail::pop3_body(s, "UIDL");
                        if (!body) {
                            co_return unexpected(body.error());
                        }
                        std::string_view v = *body;
                        size_t hint = 0;
                        for (size_t at = 0; at < v.size();) {
                            size_t eol = v.find("\r\n", at);
                            uint64_t n = 0;
                            std::string_view id;
                            if (detail::pop3_two(v.substr(at, eol - at), n, id)) {
                                // the listings are in the order of the numbers: the entry at the
                                // same place first, a search only for a server that orders them otherwise
                                size_t k = hint < out.size() && out[hint].number == n ? hint : out.size();
                                if (k == out.size()) {
                                    for (size_t j = 0; j < out.size(); ++j) {
                                        if (out[j].number == n) {
                                            k = j;
                                            break;
                                        }
                                    }
                                }
                                if (k < out.size()) {
                                    out[k].uid = string(id);
                                    hint = k + 1;
                                }
                            }
                            at = eol + 2;
                        }
                    }
                }
            }
            if (!r->ok) {
                co_return unexpected(detail::pop3_refused(errc::err, "LIST", err_text));
            }
            co_return out;
        }

        static async::task<expected<message_info, io::error>> _co_list_one(tracked_ptr<detail::Pop3ClientState> s, uint32_t number) noexcept {
            auto r = co_await detail::pop3_command(s, "LIST " + std::to_string(number), "LIST", errc::no_such_message);
            if (!r) {
                co_return unexpected(r.error());
            }
            uint64_t n = 0, size = 0;
            std::string_view rest;
            if (!detail::pop3_two(*r, n, rest) || !detail::pop3_number(rest, size)) {
                s->broken = true;
                co_return unexpected(detail::pop3_error(errc::malformed_response, "pop3 LIST", string(*r)));
            }
            message_info m{number, size, string()};
            if (s->caps.empty() || detail::pop3_has(*s, "UIDL")) {
                auto u = co_await detail::pop3_command(s, "UIDL " + std::to_string(number), "UIDL", errc::no_such_message);
                if (u) {
                    std::string_view id;
                    if (detail::pop3_two(*u, n, id)) {
                        m.uid = string(id);
                    }
                } else if (s->broken) {
                    co_return unexpected(u.error());
                }
            }
            co_return m;
        }

        static async::task<expected<string, io::error>> _co_retrieve(tracked_ptr<detail::Pop3ClientState> s, uint32_t number) noexcept {
            auto r = co_await detail::pop3_multi(s, "RETR " + std::to_string(number), "RETR", errc::no_such_message);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return string(*r);
        }

        static async::task<expected<vector<string>, io::error>> _co_retrieve_all(tracked_ptr<detail::Pop3ClientState> s, vector<uint32_t> numbers) noexcept {
            vector<string> out;
            if (numbers.empty()) {
                co_return out;
            }
            if (s->broken) {
                co_return unexpected(detail::pop3_broken(*s));
            }
            const bool pipelined = detail::pop3_has(*s, "PIPELINING");
            if (pipelined) {
                // every RETR at once, the replies read in their order
                std::string batch;
                for (auto n : numbers) {
                    batch += "RETR " + std::to_string(n) + "\r\n";
                }
                if (auto w = co_await detail::pop3_send(s, std::move(batch)); !w) {
                    co_return unexpected(w.error());
                }
                optional<io::error> first;
                for (size_t i = 0; i < numbers.size(); ++i) {
                    auto r = co_await detail::pop3_reply(s, "RETR");
                    if (!r) {
                        co_return unexpected(r.error());
                    }
                    if (!r->ok) {
                        if (!first) {
                            first = detail::pop3_refused(errc::no_such_message, "RETR", r->text);
                        }
                        continue;
                    }
                    auto body = co_await detail::pop3_body(s, "RETR");
                    if (!body) {
                        co_return unexpected(body.error());
                    }
                    out.push_back(string(*body));
                }
                if (first) {
                    co_return unexpected(*first);
                }
                co_return out;
            }
            for (auto n : numbers) {
                auto r = co_await detail::pop3_multi(s, "RETR " + std::to_string(n), "RETR", errc::no_such_message);
                if (!r) {
                    co_return unexpected(r.error());
                }
                out.push_back(string(*r));
            }
            co_return out;
        }

        static async::task<expected<string, io::error>> _co_top(tracked_ptr<detail::Pop3ClientState> s, uint32_t number, size_t lines) noexcept {
            if (!s->caps.empty() && !detail::pop3_has(*s, "TOP")) {
                co_return unexpected(detail::pop3_error(errc::not_supported, "pop3 TOP", s->server));
            }
            auto r = co_await detail::pop3_multi(s, "TOP " + std::to_string(number) + " " + std::to_string(lines), "TOP", errc::no_such_message);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return string(*r);
        }

        static async::task<expected<void, io::error>> _co_simple(tracked_ptr<detail::Pop3ClientState> s, std::string line, const char* op, errc refusal) noexcept {
            auto r = co_await detail::pop3_command(s, std::move(line), op, refusal);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_quit(tracked_ptr<detail::Pop3ClientState> s) noexcept {
            auto r = co_await detail::pop3_command(s, "QUIT", "QUIT");
            s->broken = true;
            (void)co_await s->wire->c.async_close();
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        tracked_ptr<detail::Pop3ClientState> _s;
    };
}
