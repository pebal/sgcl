//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "envelope.h"
#include "detail/wire.h"
#include "../connection.h"
#include "../dns.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/channel.h"
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
#include "../../encoding/email.h"

#include <chrono>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <unistd.h>

// The client of SMTP (RFC 5321) with the extensions a submission and a
// relay use: STARTTLS (RFC 3207), implicit TLS (RFC 8314), AUTH PLAIN,
// LOGIN and XOAUTH2 (RFC 4954, RFC 4616), PIPELINING (RFC 2920), 8BITMIME
// (RFC 6152), SMTPUTF8 (RFC 6531), SIZE (RFC 1870), DSN (RFC 3461),
// CHUNKING (RFC 3030) and the enhanced status codes (RFC 2034, RFC 3463)
namespace sgcl::net::smtp {
    class client;

    namespace detail {
        // RFC 5321 §4.5.3.2: the least a client waits for each reply
        inline constexpr duration GreetingWait = 5 * minute;
        inline constexpr duration MailWait = 5 * minute;
        inline constexpr duration RcptWait = 5 * minute;
        inline constexpr duration DataWait = 2 * minute;
        inline constexpr duration BlockWait = 3 * minute;
        inline constexpr duration EndWait = 10 * minute;
        inline constexpr size_t MaxReplyLine = 4096;   // RFC 5321 §4.5.3.1.5 says 512; servers write more

        struct ClientState {
            tracked_ptr<SmtpWire> wire;
            smtp::options o;
            string server;        // "host:port", for the errors
            string server_name;   // the host, for TLS
            string greeting;
            vector<pair<string, string>> ext;   // EHLO's keywords (upper case) and their parameters
            bool esmtp = false;
            bool tls = false;
            std::atomic<bool> broken = {false};   // written by a stop from another thread too
            std::mutex lock;      // the connection's word against a stop from another thread
            bool stopped = false;
        };

        inline const string* extension(const ClientState& s, std::string_view keyword) noexcept {
            for (auto& e : s.ext) {
                if (smtp_iequal(e.first.view(), keyword)) {
                    return &e.second;
                }
            }
            return nullptr;
        }

        inline time_point wait_until(const ClientState& s, duration rfc) noexcept {
            duration d = s.o.timeout > duration::zero() ? s.o.timeout : rfc;
            return no_deadline_at_max(sgcl::clock::now() + d);
        }

        inline io::error broken_error(const ClientState& s) noexcept {
            return io::error(io::errc::closed, "smtp", s.server);
        }

        // The connection's end where a reply was due
        inline io::error eof_error(const ClientState& s, const string& op) noexcept {
            return io::error(io::errc::unexpected_eof, string("smtp ") + op, s.server);
        }

        // A reply, its lines read in this frame: the bytes there taken
        // without a frame of their own, the wait for more on the
        // transport's readiness (SmtpWire::try_fill)
        inline async::task<expected<smtp::reply, io::error>> read_reply(tracked_ptr<ClientState> s, duration wait, string op) noexcept {
            tracked_ptr<SmtpWire> w = s->wire;
            w->c.set_read_deadline(wait_until(*s, wait));
            ReplyParser p;
            std::string line;
            for (;;) {
                bool too_long = false;
                if (w->take_line(line, MaxReplyLine, too_long)) {
                    if (!p.feed(line)) {
                        s->broken = true;
                        co_return fail(net_error(errc::malformed_smtp_reply, string("smtp ") + op, p.error));
                    }
                    if (p.done) {
                        co_return p.r;
                    }
                    continue;
                }
                if (too_long) {
                    s->broken = true;
                    co_return fail(net_error(errc::malformed_smtp_reply, string("smtp ") + op, string("line too long")));
                }
                expected<size_t, io::error> r = size_t(0);
                for (;;) {
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
                        r = fail(ready);
                        break;
                    }
                }
                if (!r) {
                    s->broken = true;
                    co_return fail(r);
                }
                if (*r == 0) {
                    s->broken = true;
                    co_return fail(eof_error(*s, op));
                }
            }
        }

        // Text written whole: what the socket takes at once without a
        // frame (net's start_write), a task for the rest only when it
        // would wait
        inline async::task<expected<void, io::error>> write_all(tracked_ptr<ClientState> s, string text, duration wait) noexcept {
            net::connection c = s->wire->c;
            c.set_write_deadline(wait_until(*s, wait + std::chrono::seconds(text.size() / 16384)));
            auto st = net::detail::ConnectionAccess::impl(c).start_write(slice<const byte>(text));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                s->broken = true;
                co_return fail(w);
            }
            co_return expected<void, io::error>();
        }

        inline async::task<expected<smtp::reply, io::error>> command(tracked_ptr<ClientState> s, string line, duration wait, string op) noexcept {
            if (s->broken) {
                co_return fail(broken_error(*s));
            }
            net::connection c = s->wire->c;
            c.set_write_deadline(wait_until(*s, wait));
            string text = string::concat(line, "\r\n");
            auto st = net::detail::ConnectionAccess::impl(c).start_write(slice<const byte>(text));
            expected<size_t, io::error> w = std::move(st.done);
            if (st.rest) {
                w = co_await std::move(*st.rest);
            }
            if (!w) {
                s->broken = true;
                co_return fail(w);
            }
            co_return co_await read_reply(s, wait, std::move(op));
        }

        // The name EHLO gives: the one set, else this host's name when it
        // is a domain, else the address of the connection as a literal
        // (RFC 5321 §4.1.4)
        inline string helo_name(const ClientState& s) {
            if (!s.o.hostname.empty()) {
                return s.o.hostname;
            }
            char name[256] = {};
            if (::gethostname(name, sizeof name - 1) == 0 && std::string_view(name).find('.') != std::string_view::npos) {
                return string(name);
            }
            auto local = s.wire->c.local_endpoint();
            if (local.address().is_v4()) {
                return string::concat("[", local.address().to_string(), "]");
            }
            if (local.address().is_v6()) {
                return string::concat("[IPv6:", local.address().with_zone(string()).to_string(), "]");
            }
            return string("localhost");
        }

        // EHLO, and HELO when the server knows no EHLO; the extensions of
        // its reply
        inline async::task<expected<void, io::error>> hello(tracked_ptr<ClientState> s) noexcept {
            string name = helo_name(*s);
            auto r = co_await command(s, string::concat("EHLO ", name), GreetingWait, "EHLO");
            if (!r) {
                co_return fail(r);
            }
            s->ext.clear();
            if (r->code == 250) {
                s->esmtp = true;
                auto lines = r->text.split('\n');
                bool first = true;
                for (auto& line : lines) {
                    if (first) {
                        first = false;
                        continue;
                    }
                    std::string_view v = line.view();
                    size_t sp = v.find_first_of(" =");
                    std::string key = smtp_uppered(v.substr(0, sp));
                    std::string params = sp == std::string_view::npos ? std::string() : std::string(v.substr(sp + 1));
                    if (key == "AUTH" && extension(*s, "AUTH")) {
                        // "AUTH=LOGIN" of old servers beside "AUTH LOGIN PLAIN": both lists
                        for (auto& e : s->ext) {
                            if (e.first == "AUTH") {
                                e.second = string::concat(e.second, " ", params);
                            }
                        }
                        continue;
                    }
                    s->ext.push_back(pair<string, string>(string(key), string(params)));
                }
                co_return expected<void, io::error>();
            }
            if (r->code >= 500) {
                auto h = co_await command(s, string::concat("HELO ", name), GreetingWait, "HELO");
                if (!h) {
                    co_return fail(h);
                }
                if (h->code == 250) {
                    s->esmtp = false;
                    co_return expected<void, io::error>();
                }
                co_return fail(reply_error(errc::smtp_reply, "HELO", *h));
            }
            co_return fail(reply_error(errc::smtp_reply, "EHLO", *r));
        }

        inline async::task<expected<void, io::error>> starttls(tracked_ptr<ClientState> s) noexcept {
            auto r = co_await command(s, "STARTTLS", GreetingWait, "STARTTLS");
            if (!r) {
                co_return fail(r);
            }
            if (r->code != 220) {
                if (s->o.require_tls) {
                    co_return fail(reply_error(errc::smtp_tls_required, "STARTTLS", *r));
                }
                co_return expected<void, io::error>();   // opportunistic: go on in clear text
            }
            if (s->wire->buffered()) {
                // bytes after the 220, sent in clear text: never part of the session
                s->broken = true;
                co_return fail(net_error(errc::malformed_smtp_reply, "smtp STARTTLS", string("data after the reply")));
            }
            tls::config c = s->o.tls;
            if (c.server_name.empty()) {
                c.server_name = s->server_name;
            }
            s->wire->c.set_deadline(time_point());
            auto t = co_await tls::async_client(s->wire->c, c);
            if (!t) {
                s->broken = true;
                co_return fail(t);
            }
            {
                std::lock_guard<std::mutex> g(s->lock);
                s->wire = make_tracked<SmtpWire>(*t);
                if (s->stopped) {
                    (void)t->close();
                }
            }
            s->tls = true;
            co_return co_await hello(s);
        }

        inline bool mechanism_offered(const ClientState& s, std::string_view m) {
            const string* auth = extension(s, "AUTH");
            if (!auth) {
                return false;
            }
            for (auto& w : auth->split(' ')) {
                if (smtp_iequal(w.view(), m)) {
                    return true;
                }
            }
            return false;
        }

        inline string b64(std::string_view s) {
            return encoding::base64::standard.encode(slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size()));
        }

        inline async::task<expected<void, io::error>> authenticate(tracked_ptr<ClientState> s) noexcept {
            const auto& o = s->o;
            if (o.username.empty() && o.password.empty() && o.oauth_token.empty()) {
                co_return expected<void, io::error>();
            }
            if (!s->tls && !o.allow_insecure_auth) {
                co_return fail(net_error(errc::smtp_tls_required, "smtp AUTH", string("credentials over a connection without TLS")));
            }
            if (!extension(*s, "AUTH")) {
                co_return fail(net_error(errc::smtp_unsupported, "smtp AUTH", string("the server offers no AUTH")));
            }
            std::string mech = smtp_uppered(o.auth.view());
            if (mech.empty()) {
                mech = !o.oauth_token.empty() ? "XOAUTH2" : mechanism_offered(*s, "PLAIN") ? "PLAIN" : mechanism_offered(*s, "LOGIN") ? "LOGIN" : "";
            }
            if (mech.empty() || !mechanism_offered(*s, mech)) {
                co_return fail(net_error(errc::smtp_unsupported, "smtp AUTH", string::concat("no mechanism of both sides: ", *extension(*s, "AUTH"))));
            }
            const std::string op = "AUTH " + mech;
            expected<smtp::reply, io::error> r = smtp::reply();
            if (mech == "PLAIN") {
                std::string token;
                token += '\0';
                token += o.username.view();
                token += '\0';
                token += o.password.view();
                r = co_await command(s, string::concat("AUTH PLAIN ", b64(token)), MailWait, string(op));
            } else if (mech == "LOGIN") {
                r = co_await command(s, "AUTH LOGIN", MailWait, string(op));
                if (r && r->code == 334) {
                    r = co_await command(s, b64(o.username.view()), MailWait, string(op));
                }
                if (r && r->code == 334) {
                    r = co_await command(s, b64(o.password.view()), MailWait, string(op));
                }
            } else if (mech == "XOAUTH2") {
                std::string token = "user=";
                token += o.username.view();
                token += "\x01" "auth=Bearer ";
                token += o.oauth_token.view();
                token += "\x01\x01";
                r = co_await command(s, string::concat("AUTH XOAUTH2 ", b64(token)), MailWait, string(op));
                if (r && r->code == 334) {
                    // the error as JSON in base64; an empty line ends the exchange (RFC 7628 §3.2.3)
                    r = co_await command(s, "", MailWait, string(op));
                }
            } else {
                co_return fail(net_error(errc::smtp_unsupported, "smtp AUTH", string::concat("mechanism not supported here: ", mech)));
            }
            if (!r) {
                co_return fail(r);
            }
            if (r->code != 235) {
                co_return fail(reply_error(errc::smtp_auth_failed, string(op), *r));
            }
            co_return expected<void, io::error>();
        }

        // The URL taken apart: the address to dial, the server's name, the
        // implicit TLS, the credentials in it
        struct SmtpTarget {
            string address;
            string host;
            bool implicit_tls = false;
            string username;
            string password;
        };

        inline expected<SmtpTarget, io::error> smtp_target(const string& text) {
            auto u = net::url::parse(text);
            if (!u) {
                return fail(net_error(errc::invalid_url, "smtp", text));
            }
            SmtpTarget t;
            std::string scheme(u->scheme().view());
            if (scheme == "smtps") {
                t.implicit_tls = true;
            } else if (scheme != "smtp") {
                return fail(net_error(errc::unsupported_scheme, "smtp", text));
            }
            if (u->hostname().empty()) {
                return fail(net_error(errc::invalid_url, "smtp", text));
            }
            t.host = u->hostname();
            uint16_t port = u->port() ? *u->port() : t.implicit_tls ? 465 : 25;
            std::string host(t.host.view());
            if (host.find(':') != std::string::npos) {
                host = "[" + host + "]";
            }
            t.address = string(host + ":" + std::to_string(port));
            t.username = string(net::detail::url_unescape(u->username().view()));
            t.password = string(net::detail::url_unescape(u->password().view()));
            return t;
        }

        inline async::task<expected<tracked_ptr<ClientState>, io::error>> start(tracked_ptr<ClientState> s, net::connection conn) noexcept;

        // A session opened: the connection (TLS from the start for
        // smtps://), the greeting, EHLO, STARTTLS when there is one to
        // take, AUTH when there are credentials
        inline async::task<expected<tracked_ptr<ClientState>, io::error>> open(tracked_ptr<ClientState> s, SmtpTarget t) noexcept {
            const time_point dial_deadline = s->o.connect_timeout > duration::zero() ? no_deadline_at_max(sgcl::clock::now() + s->o.connect_timeout) : time_point();
            auto c = co_await net::detail::dial(t.address, s->o.stop, dial_deadline);
            if (!c) {
                co_return fail(c);
            }
            net::connection conn = *c;
            if (t.implicit_tls) {
                tls::config cfg = s->o.tls;
                if (cfg.server_name.empty()) {
                    cfg.server_name = t.host;
                }
                auto tc = co_await tls::async_client(conn, cfg);
                if (!tc) {
                    co_return fail(tc);
                }
                conn = *tc;
                s->tls = true;
            }
            co_return co_await start(s, conn);
        }

        // The session over a connection made: the greeting, EHLO, STARTTLS
        // when there is one to take, AUTH when there are credentials
        inline async::task<expected<tracked_ptr<ClientState>, io::error>> start(tracked_ptr<ClientState> s, net::connection conn) noexcept {
            {
                std::lock_guard<std::mutex> g(s->lock);
                s->wire = make_tracked<SmtpWire>(conn);
                if (s->stopped) {
                    (void)conn.close();
                }
            }
            auto greet = co_await read_reply(s, GreetingWait, "greeting");
            if (!greet) {
                (void)conn.close();
                co_return fail(greet);
            }
            if (greet->code != 220) {
                (void)conn.close();
                co_return fail(reply_error(errc::smtp_reply, "smtp greeting", *greet));
            }
            s->greeting = greet->text;
            if (auto h = co_await hello(s); !h) {
                (void)s->wire->c.close();
                co_return fail(h);
            }
            if (!s->tls && s->o.starttls && extension(*s, "STARTTLS")) {
                if (auto st = co_await starttls(s); !st) {
                    (void)s->wire->c.close();
                    co_return fail(st);
                }
            }
            if (!s->tls && s->o.require_tls) {
                (void)s->wire->c.close();
                co_return fail(net_error(errc::smtp_tls_required, "smtp STARTTLS", string("the server offers no STARTTLS")));
            }
            if (auto a = co_await authenticate(s); !a) {
                (void)co_await command(s, "QUIT", GreetingWait, "QUIT");
                (void)s->wire->c.close();
                co_return fail(a);
            }
            co_return s;
        }

        inline tracked_ptr<ClientState> new_state(const SmtpTarget& t, const smtp::options& o) {
            tracked_ptr s = make_tracked<ClientState>();
            s->o = o;
            if (s->o.username.empty()) {
                s->o.username = t.username;
            }
            if (s->o.password.empty()) {
                s->o.password = t.password;
            }
            s->server = t.address;
            s->server_name = t.host;
            return s;
        }

        // What a stop leaves to the task it stopped: the work's result
        // sent here, room for it so that a send never waits
        template<class T>
        struct StopResult {
            async::detail::ChannelState<expected<T, io::error>> done{1};
        };

        template<class T>
        async::task<void> run_into(tracked_ptr<StopResult<T>> r, async::task<expected<T, io::error>> work) noexcept {
            auto got = co_await std::move(work);
            r->done.try_send(std::move(got));
        }

        // The work, ended by the stop: the connection closed (the work
        // then fails at once) and ECANCELED returned
        template<class T>
        async::task<expected<T, io::error>> stoppable(async::task<expected<T, io::error>> work, tracked_ptr<ClientState> s) noexcept {
            async::stop_token stop = s->o.stop;
            if (!stop.stop_possible()) {
                co_return co_await std::move(work);
            }
            auto cancel = [&] {
                std::lock_guard<std::mutex> g(s->lock);
                s->stopped = true;
                s->broken = true;
                if (s->wire) {
                    (void)s->wire->c.close();
                }
            };
            if (stop.stop_requested()) {
                cancel();
                co_return fail(io::error(error_code(ECANCELED, std::system_category()), "smtp", s->server));
            }
            tracked_ptr r = make_tracked<StopResult<T>>();
            async::go(run_into<T>(r, std::move(work)));
            bool stopped = false;
            optional<expected<T, io::error>> result;
            co_await sgcl::async::select(r->done.on_receive([&](optional<expected<T, io::error>> x) { result = std::move(x); }),
                                        stop.on_stop([&] { stopped = true; }));
            if (stopped || !result) {
                cancel();
                co_return fail(io::error(error_code(ECANCELED, std::system_category()), "smtp", s->server));
            }
            co_return std::move(*result);
        }

        // The envelope of a message: From (Sender when there is one) for
        // the reverse path, To, Cc and Bcc for the recipients, each once
        inline smtp::envelope envelope_of(const encoding::email& m) {
            smtp::envelope e;
            auto sender = encoding::email::address::parse(m.header("Sender"));
            if (sender && !m.header("Sender").empty()) {
                e.from = sender->addr();
            } else if (auto f = m.from()) {
                e.from = f->addr();
            }
            auto add = [&](const vector<encoding::email::address>& list) {
                for (auto& a : list) {
                    bool seen = false;
                    for (auto& t : e.to) {
                        if (t == a.addr()) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) {
                        e.to.push_back(a.addr());
                    }
                }
            };
            add(m.to());
            add(m.cc());
            add(m.bcc());
            return e;
        }

        inline string per_recipient(const vector<string>& v, size_t i) noexcept {
            if (v.empty()) {
                return string();
            }
            return v.size() == 1 ? v[0] : i < v.size() ? v[i] : string();
        }

        // A transaction: MAIL, RCPT for each recipient, the data by DATA
        // or BDAT; pipelined when the server says PIPELINING
        inline async::task<expected<smtp::receipt, io::error>> transact(tracked_ptr<ClientState> s, smtp::envelope e, string data) noexcept {
            if (s->broken) {
                co_return fail(broken_error(*s));
            }
            if (s->o.dkim) {
                auto signed_data = s->o.dkim->sign(data, s->o.dkim_options);
                if (!signed_data) {
                    co_return fail(signed_data);
                }
                data = std::move(*signed_data);
            }
            if (e.to.empty()) {
                co_return fail(io::error(error_code(EINVAL, std::system_category()), "smtp", string("no recipients")));
            }
            if (!smtp_path_ok(e.from.view())) {
                co_return fail(io::error(error_code(EINVAL, std::system_category()), "smtp MAIL FROM", e.from));
            }
            bool utf8 = !smtp_ascii(e.from.view());
            for (auto& t : e.to) {
                if (t.empty() || !smtp_path_ok(t.view())) {
                    co_return fail(io::error(error_code(EINVAL, std::system_category()), "smtp RCPT TO", t));
                }
                utf8 |= !smtp_ascii(t.view());
            }
            if (utf8 && !extension(*s, "SMTPUTF8")) {
                co_return fail(net_error(errc::smtp_unsupported, "smtp MAIL FROM", string("an address past ASCII and no SMTPUTF8")));
            }
            const bool eight = !smtp_ascii(data.view());
            if (const string* size = extension(*s, "SIZE")) {
                uint64_t limit = 0;
                for (char c : size->view()) {
                    if (c < '0' || c > '9') {
                        break;
                    }
                    limit = limit * 10 + uint64_t(c - '0');
                }
                if (limit && data.size() > limit) {
                    co_return fail(net_error(errc::smtp_unsupported, "smtp MAIL FROM",
                                             string::concat("a message of ", std::to_string(data.size()), " bytes past the server's SIZE ", size->view())));
                }
            }
            const bool dsn = extension(*s, "DSN") != nullptr;
            const bool chunking = extension(*s, "CHUNKING") != nullptr;
            const bool pipelining = extension(*s, "PIPELINING") != nullptr;
            std::string mail = "MAIL FROM:<";
            mail += e.from.view();
            mail += '>';
            if (extension(*s, "SIZE")) {
                mail += " SIZE=" + std::to_string(data.size());
            }
            if (eight && extension(*s, "8BITMIME")) {
                mail += " BODY=8BITMIME";
            }
            if (utf8) {
                mail += " SMTPUTF8";
            }
            if (dsn && !e.ret.empty()) {
                mail += " RET=" + smtp_uppered(e.ret.view());
            }
            if (dsn && !e.envid.empty()) {
                mail += " ENVID=" + xtext_encode(e.envid.view());
            }
            std::string batch = mail + "\r\n";
            for (size_t i = 0; i < e.to.size(); ++i) {
                std::string rcpt = "RCPT TO:<";
                rcpt += e.to[i].view();
                rcpt += '>';
                if (dsn && !per_recipient(e.notify, i).empty()) {
                    rcpt += " NOTIFY=" + smtp_uppered(per_recipient(e.notify, i).view());
                }
                if (dsn && !per_recipient(e.orcpt, i).empty()) {
                    string orcpt = per_recipient(e.orcpt, i);
                    std::string_view o = orcpt.view();
                    size_t semi = o.find(';');
                    if (semi == std::string_view::npos) {
                        rcpt += " ORCPT=rfc822;" + xtext_encode(o);
                    } else {
                        rcpt += " ORCPT=" + std::string(o.substr(0, semi + 1)) + xtext_encode(o.substr(semi + 1));
                    }
                }
                batch += rcpt + "\r\n";
            }
            const bool data_pipelined = pipelining && !chunking;
            if (data_pipelined) {
                batch += "DATA\r\n";
            }
            smtp::receipt out;
            size_t accepted = 0;
            optional<smtp::reply> mail_refused;
            optional<smtp::reply> last_refusal;
            optional<smtp::reply> data_reply;
            if (pipelining) {
                if (auto w = co_await write_all(s, string(batch), MailWait); !w) {
                    co_return fail(w);
                }
                auto mr = co_await read_reply(s, MailWait, "MAIL FROM");
                if (!mr) {
                    co_return fail(mr);
                }
                if (!mr->positive()) {
                    mail_refused = *mr;
                }
                for (size_t i = 0; i < e.to.size(); ++i) {
                    auto rr = co_await read_reply(s, RcptWait, "RCPT TO");
                    if (!rr) {
                        co_return fail(rr);
                    }
                    if (rr->positive()) {
                        ++accepted;
                    } else {
                        out.rejected.push_back(rejection{e.to[i], *rr});
                        last_refusal = *rr;
                    }
                }
                if (data_pipelined) {
                    auto dr = co_await read_reply(s, DataWait, "DATA");
                    if (!dr) {
                        co_return fail(dr);
                    }
                    data_reply = *dr;
                }
            } else {
                auto mr = co_await command(s, string(mail), MailWait, "MAIL FROM");
                if (!mr) {
                    co_return fail(mr);
                }
                if (!mr->positive()) {
                    co_return fail(reply_error(errc::smtp_reply, string(mail), *mr));
                }
                std::string_view rest(batch);
                rest.remove_prefix(mail.size() + 2);
                for (size_t i = 0; i < e.to.size(); ++i) {
                    size_t nl = rest.find("\r\n");
                    std::string_view line = rest.substr(0, nl);
                    rest.remove_prefix(nl + 2);
                    auto rr = co_await command(s, string(line), RcptWait, "RCPT TO");
                    if (!rr) {
                        co_return fail(rr);
                    }
                    if (rr->positive()) {
                        ++accepted;
                    } else {
                        out.rejected.push_back(rejection{e.to[i], *rr});
                        last_refusal = *rr;
                    }
                }
            }
            if (mail_refused || accepted == 0) {
                if (data_reply && data_reply->code == 354) {
                    // RFC 2920 §3.1: DATA taken with no recipient: the empty message ends it
                    if (auto w = co_await write_all(s, string(".\r\n"), BlockWait); !w) {
                        co_return fail(w);
                    }
                    (void)co_await read_reply(s, EndWait, "DATA");
                }
                (void)co_await command(s, "RSET", MailWait, "RSET");
                if (mail_refused) {
                    co_return fail(reply_error(errc::smtp_reply, string(mail), *mail_refused));
                }
                co_return fail(reply_error(errc::smtp_reply, string::concat("RCPT TO:<", out.rejected.back().recipient, ">"), *last_refusal));
            }
            std::string wire_data;
            if (chunking) {
                std::string body;
                smtp_stuffed(data.view(), body, false);
                wire_data = "BDAT " + std::to_string(body.size()) + " LAST\r\n";
                wire_data += body;
                if (auto w = co_await write_all(s, string(wire_data), BlockWait); !w) {
                    co_return fail(w);
                }
                auto fr = co_await read_reply(s, EndWait, "BDAT");
                if (!fr) {
                    co_return fail(fr);
                }
                if (!fr->positive()) {
                    co_return fail(reply_error(errc::smtp_reply, "BDAT", *fr));
                }
                out.reply = *fr;
                co_return out;
            }
            if (!data_reply) {
                auto dr = co_await command(s, "DATA", DataWait, "DATA");
                if (!dr) {
                    co_return fail(dr);
                }
                data_reply = *dr;
            }
            if (data_reply->code != 354) {
                (void)co_await command(s, "RSET", MailWait, "RSET");
                co_return fail(reply_error(errc::smtp_reply, "DATA", *data_reply));
            }
            smtp_stuffed(data.view(), wire_data, true);
            if (auto w = co_await write_all(s, string(wire_data), BlockWait); !w) {
                co_return fail(w);
            }
            auto fr = co_await read_reply(s, EndWait, "DATA");
            if (!fr) {
                co_return fail(fr);
            }
            if (!fr->positive()) {
                co_return fail(reply_error(errc::smtp_reply, "DATA", *fr));
            }
            out.reply = *fr;
            co_return out;
        }

        // The message written for this server: 8bit where it says
        // 8BITMIME, UTF-8 in the head where SMTPUTF8 is needed and offered,
        // no Bcc
        inline string message_text(const ClientState& s, const encoding::email& m, const smtp::envelope& e) {
            encoding::email::write_options w;
            w.write_bcc = false;
            w.allow_8bit = extension(s, "8BITMIME") != nullptr;
            bool utf8 = !smtp_ascii(e.from.view());
            for (auto& t : e.to) {
                utf8 |= !smtp_ascii(t.view());
            }
            w.allow_utf8 = utf8 && extension(s, "SMTPUTF8") != nullptr;
            return m.to_string(w);
        }

        inline async::task<expected<smtp::receipt, io::error>> send_mail(tracked_ptr<ClientState> s, smtp::envelope e, encoding::email m) noexcept {
            string text = message_text(*s, m, e);
            co_return co_await transact(s, std::move(e), std::move(text));
        }

        inline async::task<expected<smtp::reply, io::error>> simple(tracked_ptr<ClientState> s, string line, string op) noexcept {
            auto r = co_await command(s, std::move(line), MailWait, op);
            if (!r) {
                co_return fail(r);
            }
            if (!r->positive()) {
                co_return fail(reply_error(errc::smtp_reply, op, *r));
            }
            co_return *r;
        }

        inline async::task<expected<void, io::error>> quit(tracked_ptr<ClientState> s) noexcept {
            if (!s->wire) {
                co_return expected<void, io::error>();
            }
            if (!s->broken) {
                (void)co_await command(s, "QUIT", MailWait, "QUIT");
            }
            s->broken = true;
            co_return co_await s->wire->c.async_close();
        }

        inline async::task<expected<smtp::receipt, io::error>> one_shot(string url, encoding::email m, smtp::options o) noexcept {
            auto t = smtp_target(url);
            if (!t) {
                co_return fail(t);
            }
            auto s = new_state(*t, o);
            auto opened = co_await stoppable(open(s, *t), s);
            if (!opened) {
                co_return fail(opened);
            }
            auto e = envelope_of(m);
            auto r = co_await stoppable(send_mail(s, std::move(e), std::move(m)), s);
            (void)co_await quit(s);
            co_return r;
        }

        struct ClientAccess;
    }

    // A session with an SMTP server, for several messages over one
    // connection: Python's smtplib.SMTP, Go's smtp.Client. A handle of one
    // word, as a connection is: a copy is the same session. One operation
    // at a time: a session is a conversation in turn.
    //
    //     auto c = net::smtp::client::connect("smtp://user:pw@mail.example.com:587");
    //     c->send(m1);
    //     c->send(m2);
    //     c->quit();
    class client {
    public:
        client() noexcept = default;   // no session; an operation on it is a contract violation

        // A session opened with the server of the URL ("smtp://host:587",
        // "smtps://user:password@host"): the connection, the greeting, EHLO,
        // STARTTLS, AUTH, as the options say
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& url, const options& o = {}) {
            return async_connect(url, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string url, options o = {}) noexcept {
            return _co_connect(std::move(url), std::move(o));
        }

        // The message to the recipients of its To, Cc and Bcc, from its
        // From (Sender when it has one), its Bcc left out of what is sent;
        // or to the envelope given; or bytes of a message the program
        // made itself. The recipients the server refused while it took
        // others are in the receipt; none taken is the error of the last
        // refusal
        // `send(...)` on this thread, `co_await async_send(...)` in a task
        expected<receipt, io::error> send(const encoding::email& m) const {
            return async_send(m).wait();
        }

        async::task<expected<receipt, io::error>> async_send(const encoding::email& m) const noexcept {
            return detail::stoppable(detail::send_mail(_s, detail::envelope_of(m), m), _s);
        }

        expected<receipt, io::error> send(const encoding::email& m, const envelope& e) const {
            return async_send(m, e).wait();
        }

        async::task<expected<receipt, io::error>> async_send(const encoding::email& m, const envelope& e) const noexcept {
            return detail::stoppable(detail::send_mail(_s, e, m), _s);
        }

        expected<receipt, io::error> send(const envelope& e, const string& data) const {
            return async_send(e, data).wait();
        }

        async::task<expected<receipt, io::error>> async_send(const envelope& e, const string& data) const noexcept {
            return detail::stoppable(detail::transact(_s, e, data), _s);
        }

        // VRFY: what the server says of an address (most say 252, "cannot
        // verify"); a refusal is the error
        expected<reply, io::error> verify(const string& address) const {
            return async_verify(address).wait();
        }

        async::task<expected<reply, io::error>> async_verify(const string& address) const noexcept {
            return detail::stoppable(detail::simple(_s, string::concat("VRFY ", address), "VRFY"), _s);
        }

        // RSET: the transaction the server holds dropped
        expected<void, io::error> reset() const {
            return async_reset().wait();
        }

        async::task<expected<void, io::error>> async_reset() const noexcept {
            return _co_void(detail::stoppable(detail::simple(_s, "RSET", "RSET"), _s));
        }

        // NOOP: the session kept alive, the server asked whether it is there
        expected<void, io::error> noop() const {
            return async_noop().wait();
        }

        async::task<expected<void, io::error>> async_noop() const noexcept {
            return _co_void(detail::stoppable(detail::simple(_s, "NOOP", "NOOP"), _s));
        }

        // QUIT, then the connection closed
        expected<void, io::error> quit() const {
            return async_quit().wait();
        }

        async::task<expected<void, io::error>> async_quit() const noexcept {
            return detail::quit(_s);
        }

        // The connection closed without QUIT
        expected<void, io::error> close() const noexcept {
            _s->broken = true;
            return _s->wire ? _s->wire->c.close() : expected<void, io::error>();
        }

        // The text of the server's greeting ("mail.example.com ESMTP ready")
        string greeting() const noexcept {
            return _s->greeting;
        }

        // Whether EHLO named the extension ("PIPELINING", "SIZE"), and its
        // parameters ("35882577" of SIZE, "PLAIN LOGIN" of AUTH; "" for
        // none); a server that took only HELO names none
        bool has_extension(const string& keyword) const noexcept {
            return detail::extension(*_s, keyword.view()) != nullptr;
        }

        string extension(const string& keyword) const noexcept {
            const string* e = detail::extension(*_s, keyword.view());
            return e ? *e : string();
        }

        // The largest message SIZE allows, 0 when the server names none
        uint64_t max_size() const noexcept {
            const string* e = detail::extension(*_s, "SIZE");
            uint64_t n = 0;
            if (e) {
                for (char c : e->view()) {
                    if (c < '0' || c > '9' || n > UINT64_MAX / 10) {
                        break;
                    }
                    n = n * 10 + uint64_t(c - '0');
                }
            }
            return n;
        }

        // Whether the session runs over TLS (smtps://, or after STARTTLS)
        bool is_tls() const noexcept {
            return _s->tls;
        }

        explicit operator bool() const noexcept {
            return bool(_s);
        }

        friend bool operator==(const client& a, const client& b) noexcept {
            return a._s == b._s;
        }

    private:
        friend struct detail::ClientAccess;

        explicit client(tracked_ptr<detail::ClientState> s) noexcept
        : _s(std::move(s)) {
        }

        static async::task<expected<client, io::error>> _co_connect(string url, options o) noexcept {
            auto t = detail::smtp_target(url);
            if (!t) {
                co_return net::detail::fail(t);
            }
            auto s = detail::new_state(*t, o);
            auto opened = co_await detail::stoppable(detail::open(s, *t), s);
            if (!opened) {
                co_return net::detail::fail(opened);
            }
            co_return client(*opened);
        }

        static async::task<expected<void, io::error>> _co_void(async::task<expected<reply, io::error>> t) noexcept {
            auto r = co_await std::move(t);
            if (!r) {
                co_return net::detail::fail(r);
            }
            co_return expected<void, io::error>();
        }

        tracked_ptr<detail::ClientState> _s;
    };

    // The message sent in one line: a session opened with the server of
    // the URL, the message sent to the recipients of its To, Cc and Bcc,
    // QUIT
    // `send(...)` on this thread, `co_await async_send(...)` in a task
    inline expected<receipt, io::error> send(const string& url, const encoding::email& m, const options& o = {}) {
        return detail::one_shot(url, m, o).wait();
    }

    inline async::task<expected<receipt, io::error>> async_send(string url, encoding::email m, options o = {}) noexcept {
        return detail::one_shot(std::move(url), std::move(m), std::move(o));
    }

    namespace detail {
        // One domain's recipients to its exchangers: the MX records in
        // order of preference (the domain itself when it has none, RFC
        // 5321 §5.1), the next on a failure to connect or a temporary
        // refusal of the session; STARTTLS when offered
        inline async::task<expected<smtp::receipt, io::error>> deliver_domain(string domain, smtp::envelope e, string data, smtp::options o) noexcept {
            vector<string> hosts;
            // a mail domain is a full name (RFC 5321 §2.3.5): asked as one, no search list
            auto mx = co_await net::dns::async_lookup_mx(domain.view().ends_with('.') ? domain : string::concat(domain, "."), o.dns);
            if (mx) {
                for (auto& m : *mx) {
                    std::string h(m.host.view());
                    if (!h.empty() && h.back() == '.') {
                        h.pop_back();
                    }
                    if (h.empty()) {
                        continue;   // a null MX (RFC 7505): the domain takes no mail
                    }
                    hosts.push_back(string(h));
                }
                if (hosts.empty()) {
                    co_return fail(net_error(errc::smtp_reply, "smtp MX", string::concat("556 5.1.10 ", domain, " takes no mail (null MX)")));
                }
            } else if (mx.error().code() == errc::no_data || mx.error().code() == errc::host_not_found) {
                if (mx.error().code() == errc::host_not_found) {
                    co_return fail(mx);
                }
                hosts.push_back(domain);
            } else {
                co_return fail(mx);
            }
            const uint16_t port = o.port ? o.port : 25;
            io::error last;
            for (auto& h : hosts) {
                std::string host(h.view());
                if (host.find(':') != std::string::npos) {
                    host = "[" + host + "]";
                }
                SmtpTarget t;
                t.address = string(host + ":" + std::to_string(port));
                t.host = h;
                auto s = new_state(t, o);
                auto opened = co_await stoppable(open(s, t), s);
                if (!opened) {
                    last = opened.error();
                    auto r = reply_of(last);
                    if (r && r->code >= 500) {
                        co_return fail(opened);   // a permanent refusal: no other exchanger asked
                    }
                    continue;
                }
                auto r = co_await stoppable(transact(s, e, data), s);
                (void)co_await quit(s);
                if (!r) {
                    auto rep = reply_of(r.error());
                    if (!rep || rep->code < 500) {
                        last = r.error();
                        continue;
                    }
                }
                co_return r;
            }
            co_return fail(last);
        }

        inline async::task<expected<smtp::receipt, io::error>> deliver_all(encoding::email m, smtp::options o) noexcept {
            auto e = envelope_of(m);
            if (e.to.empty()) {
                co_return fail(io::error(error_code(EINVAL, std::system_category()), "smtp", string("no recipients")));
            }
            encoding::email::write_options w;
            w.write_bcc = false;
            string data = m.to_string(w);
            // the recipients by domain, in their order
            vector<string> domains;
            vector<vector<string>> groups;
            for (auto& r : e.to) {
                auto at = r.view().rfind('@');
                string d = at == std::string_view::npos ? string() : string(r.view().substr(at + 1));
                std::string lower = smtp_uppered(d.view());
                size_t k = 0;
                for (; k < domains.size(); ++k) {
                    if (smtp_iequal(domains[k].view(), lower)) {
                        break;
                    }
                }
                if (k == domains.size()) {
                    domains.push_back(d);
                    groups.push_back(vector<string>());
                }
                groups[k].push_back(r);
            }
            smtp::receipt out;
            bool any = false;
            optional<io::error> last;
            for (size_t k = 0; k < domains.size(); ++k) {
                smtp::envelope de = e;
                de.to = groups[k];
                auto r = co_await deliver_domain(domains[k], de, data, o);
                if (r) {
                    any = true;
                    out.reply = r->reply;
                    for (auto& x : r->rejected) {
                        out.rejected.push_back(x);
                    }
                    continue;
                }
                last = r.error();
                smtp::reply rep = reply_of(r.error()).value_or(smtp::reply{0, string(), r.error().message()});
                for (auto& t : groups[k]) {
                    out.rejected.push_back(rejection{t, rep});
                }
            }
            if (!any) {
                co_return fail(*last);
            }
            co_return out;
        }
    }

    // The message to its recipients' own servers, without a server of
    // one's own between: each domain's recipients to the exchangers of its
    // MX records on port 25, the next exchanger on a failure, STARTTLS when
    // offered. What a domain refused or could not take is a rejection
    // with its reply (code 0 and the error's text where there was none);
    // nothing taken anywhere is the error
    // `deliver(...)` on this thread, `co_await async_deliver(...)` in a task
    inline expected<receipt, io::error> deliver(const encoding::email& m, const options& o = {}) {
        return detail::deliver_all(m, o).wait();
    }

    inline async::task<expected<receipt, io::error>> async_deliver(encoding::email m, options o = {}) noexcept {
        return detail::deliver_all(std::move(m), std::move(o));
    }
}
