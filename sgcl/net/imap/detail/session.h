//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "deflate_conn.h"
#include "io.h"
#include "mime.h"
#include "response.h"
#include "search.h"
#include "server_state.h"
#include "syntax.h"
#include "words.h"
#include "../error.h"
#include "../types.h"
#include "../../tls.h"
#include "../../../async/coroutine.h"
#include "../../../async/mutex.h"
#include "../../../async/timer.h"
#include "../../../core/clock.h"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// One connection of an imap::server (RFC 9051 §3, §6): the greeting, then
// commands read whole (their literals with them, a continuation sent for a
// synchronizing one), each run against the state it is allowed in and
// answered; the updates of the selected mailbox announced where RFC 9051
// §7 lets them be. One task per connection; IDLE leaves the task reading
// for DONE while the hub's announcements are written by the task that
// made the change's wake.
namespace sgcl::net::imap::detail {
    inline constexpr const char* ServerCapabilities =
        "IMAP4rev1 IMAP4rev2 LITERAL+ SASL-IR ENABLE IDLE NAMESPACE UIDPLUS UNSELECT MOVE CONDSTORE QRESYNC ESEARCH SEARCHRES SPECIAL-USE "
        "CREATE-SPECIAL-USE LIST-EXTENDED LIST-STATUS CHILDREN ID UTF8=ACCEPT SORT SORT=DISPLAY THREAD=ORDEREDSUBJECT THREAD=REFERENCES WITHIN "
        "BINARY STATUS=SIZE MULTIAPPEND";

    class Session {
    public:
        enum class State : uint8_t { not_authenticated, authenticated, selected, logout };

        Session(const tracked_ptr<ServerImpl>& s, const tracked_ptr<ServerSettings>& cfg, const net::connection& c, bool tls) noexcept
        : srv(s)
        , cfg(cfg)
        , conn(c)
        , tls(tls) {
            in.reset(c);
        }

        tracked_ptr<ServerImpl> srv;
        tracked_ptr<ServerSettings> cfg;
        net::connection conn;
        Reader in;
        std::string out;
        std::string cmd;            // the command being run, its literals inline
        std::string tag;
        std::string scratch;
        State state = State::not_authenticated;
        string user;
        bool tls = false;
        bool compressed = false;
        bool utf8 = false;          // UTF8=ACCEPT or IMAP4rev2 enabled
        bool rev2 = false;
        bool condstore = false;
        bool qresync = false;
        tracked_ptr<View> view;
        string mailbox;
        std::vector<uint32_t> saved;   // SEARCHRES: the UIDs SAVE kept
        int auth_failures = 0;
        async::mutex out_lock;
        std::atomic<bool> idling{false};
        bool ended = false;

        // --- output ---------------------------------------------------------

        async::task<bool> flush() noexcept {
            if (out.empty()) {
                co_return true;
            }
            auto r = co_await write_all(conn, out);
            out.clear();
            if (out.capacity() > (size_t(1) << 20)) {
                std::string().swap(out);
            }
            co_return bool(r);
        }

        void tagged(std::string_view status, const char* code, std::string_view text) {
            out += tag;
            out += ' ';
            out += status;
            out += ' ';
            if (code) {
                out += '[';
                out += code;
                out += "] ";
            }
            out += text;
            out += "\r\n";
        }

        void ok(std::string_view text, const char* code = nullptr) {
            tagged("OK", code, text);
        }

        void no(std::string_view text, const char* code = nullptr) {
            tagged("NO", code, text);
        }

        void bad(std::string_view text, const char* code = nullptr) {
            tagged("BAD", code, text);
        }

        // A backend's failure as a tagged NO with its response code
        void no_of(const io::error& e, std::string_view what) {
            const char* code = response_code_of(e);
            std::string text(what);
            if (e.code().category() != category()) {
                cfg->report(string(std::string("backend: ") + std::string(e.message().view())));
            }
            no(text, code);
        }

        std::string capabilities() const {
            std::string c = ServerCapabilities;
            c += " APPENDLIMIT=";
            put_number(c, cfg->max_literal);
            if (cfg->backend->has_quota()) {
                c += " QUOTA QUOTA=RES-STORAGE QUOTA=RES-MESSAGE";
            }
            if (cfg->compress && !compressed && state != State::not_authenticated) {
                c += " COMPRESS=DEFLATE";
            }
            const bool login_disabled = cfg->tls && !tls;
            if (state == State::not_authenticated) {
                if (cfg->tls && !tls) {
                    c += " STARTTLS LOGINDISABLED";
                }
                if (!login_disabled) {
                    c += " AUTH=PLAIN AUTH=LOGIN";
                    if (cfg->check_token) {
                        c += " AUTH=XOAUTH2 AUTH=OAUTHBEARER";
                    }
                }
            }
            return c;
        }

        // --- names ------------------------------------------------------------

        // A mailbox name as the client sent it, in UTF-8: modified UTF-7
        // decoded for a client without UTF-8 (UTF-8 sent anyway taken as
        // it is); false for one that is neither
        bool name_in(std::string_view raw, string& out_name) {
            std::string decoded;
            if (utf8) {
                if (!valid_utf8(raw)) {
                    return false;
                }
                decoded = std::string(raw);
            } else if (!utf7_decode(raw, decoded)) {
                if (is_ascii(raw) || !valid_utf8(raw)) {
                    return false;
                }
                decoded = std::string(raw);
            }
            out_name = string(canonical_mailbox(decoded, '/'));
            return true;
        }

        void put_name(std::string& o, std::string_view name) const {
            if (utf8) {
                put_astring(o, name, true);
            } else {
                put_astring(o, utf7_encode(name), false);
            }
        }

        // --- the connection's loop --------------------------------------------

        static async::task<void> run(tracked_ptr<Session> self) noexcept {
            Session& s = *self;
            s.out = "* OK [CAPABILITY " + s.capabilities() + "] " + std::string(s.cfg->greeting.view()) + "\r\n";
            if (!co_await s.flush()) {
                co_return;
            }
            while (!s.ended) {
                const duration limit = s.state == State::not_authenticated ? s.cfg->login_timeout : s.cfg->idle_timeout;
                s.conn.set_read_deadline(limit > duration::zero() ? sgcl::clock::now() + limit : time_point());
                auto got = co_await s.read_command();
                if (!got || !*got) {
                    break;
                }
                if (s.cmd.empty()) {
                    continue;
                }
                int expected_state = ConnEntry::idle;
                if (!s.entry->state.compare_exchange_strong(expected_state, ConnEntry::active)) {
                    break;   // a shutdown closed it meanwhile
                }
                co_await s.dispatch();
                // commands that came pipelined are answered in one write:
                // the output waits while the next command is buffered whole
                if (s.ended || !s.in.has_line() || s.out.size() > (size_t(64) << 10)) {
                    if (!co_await s.flush()) {
                        break;
                    }
                }
                s.entry->state.store(ConnEntry::idle);
                if (s.srv->shutting_down.load() && !s.ended) {
                    s.out += "* BYE [UNAVAILABLE] Server shutting down\r\n";
                    (void)co_await s.flush();
                    break;
                }
            }
            s.leave();
        }

        // A command whole into cmd: false at the end of the connection or
        // when it must end (a line past the limit, a literal that cannot be
        // skipped)
        async::task<expected<bool, io::error>> read_command() noexcept {
            cmd.clear();
            tag.clear();
            uint64_t literals = 0;
            for (;;) {
                const size_t line_start = cmd.size();
                auto r = co_await in.read_line(cmd, cmd.size() + cfg->max_command);
                if (!r) {
                    co_return unexpected(r.error());
                }
                if (*r == LineEnd::eof) {
                    co_return false;
                }
                if (*r == LineEnd::too_long) {
                    out += "* BYE [TOOBIG] Command line too long\r\n";
                    (void)co_await flush();
                    co_return false;
                }
                if (tag.empty()) {
                    Lexer x(cmd);
                    tag = std::string(x.run(tag_char));
                    if (tag.empty() || !x.eat(' ')) {
                        tag = "*";
                    }
                }
                LiteralHead h = literal_at_end(std::string_view(cmd).substr(line_start));
                if (!h.found) {
                    co_return true;
                }
                literals += h.size;
                if (h.overflow || h.size > cfg->max_literal || literals > cfg->max_literal + cfg->max_command) {
                    if (h.sync && !h.overflow) {
                        // the client waits for our word: none, the command refused
                        bad("Literal too big", "TOOBIG");
                        (void)co_await flush();
                        cmd.clear();
                        tag.clear();
                        co_return true;
                    }
                    out += tag + " BAD [TOOBIG] Literal too big\r\n* BYE [TOOBIG] Literal too big\r\n";
                    (void)co_await flush();
                    co_return false;
                }
                if (h.sync) {
                    out += "+ Ready for literal data\r\n";
                    if (!co_await flush()) {
                        co_return false;
                    }
                }
                cmd += "\r\n";
                auto lit = co_await in.read_exact(cmd, size_t(h.size));
                if (!lit) {
                    co_return unexpected(lit.error());
                }
                if (!*lit) {
                    co_return false;
                }
            }
        }

        void leave() {
            unselect(false);
            ended = true;
        }

        // --- dispatch ----------------------------------------------------------

        async::task<void> dispatch() noexcept {
            Lexer x(cmd);
            std::string_view t = x.run(tag_char);
            if (t.empty() || !x.eat(' ')) {
                tag = "*";
                out += "* BAD Missing tag\r\n";
                co_return;
            }
            std::string_view name = x.atom();
            if (name.empty()) {
                bad("Missing command");
                co_return;
            }
            const std::string u = to_upper(name);
            bool uid = false;
            std::string sub;
            if (u == "UID") {
                if (!x.eat(' ')) {
                    bad("Missing UID command");
                    co_return;
                }
                sub = to_upper(x.atom());
                uid = true;
            }
            const bool arg = x.eat(' ');
            (void)arg;
            try {
                co_await _dispatch(x, uid ? sub : u, uid);
            } catch (const std::exception& e) {
                cfg->report(string(std::string("command ") + u + ": " + e.what()));
                no("Internal error", "SERVERBUG");
            }
        }

        async::task<void> _dispatch(Lexer& x, const std::string& c, bool uid) {
            // any state
            if (!uid) {
                if (c == "CAPABILITY") {
                    out += "* CAPABILITY " + capabilities() + "\r\n";
                    ok("CAPABILITY completed");
                    co_return;
                }
                if (c == "NOOP" || (c == "CHECK" && state == State::selected)) {
                    refresh();
                    updates(true);
                    ok(c == "NOOP" ? "NOOP completed" : "CHECK completed");
                    co_return;
                }
                if (c == "LOGOUT") {
                    out += "* BYE Logging out\r\n";
                    ok("LOGOUT completed");
                    ended = true;
                    state = State::logout;
                    co_return;
                }
                if (c == "ID") {
                    cmd_id(x);
                    co_return;
                }
            }
            if (state == State::not_authenticated) {
                if (uid) {
                    bad("Not authenticated");
                    co_return;
                }
                if (c == "STARTTLS") {
                    co_await cmd_starttls();
                } else if (c == "LOGIN") {
                    cmd_login(x);
                } else if (c == "AUTHENTICATE") {
                    co_await cmd_authenticate(x);
                } else if (c == "ENABLE" || c == "SELECT" || c == "EXAMINE" || c == "LIST" || c == "CREATE") {
                    bad("Not authenticated");
                } else {
                    bad("Unknown command");
                }
                co_return;
            }
            if (!uid) {
                if (c == "ENABLE") {
                    cmd_enable(x);
                } else if (c == "SELECT" || c == "EXAMINE") {
                    cmd_select(x, c == "EXAMINE");
                } else if (c == "CREATE") {
                    cmd_create(x);
                } else if (c == "DELETE") {
                    cmd_delete(x);
                } else if (c == "RENAME") {
                    cmd_rename(x);
                } else if (c == "SUBSCRIBE" || c == "UNSUBSCRIBE") {
                    cmd_subscribe(x, c == "SUBSCRIBE");
                } else if (c == "LIST") {
                    cmd_list(x, false);
                } else if (c == "LSUB") {
                    cmd_list(x, true);
                } else if (c == "NAMESPACE") {
                    out += "* NAMESPACE ((\"\" \"/\")) NIL NIL\r\n";
                    ok("NAMESPACE completed");
                } else if (c == "STATUS") {
                    cmd_status(x);
                } else if (c == "APPEND") {
                    cmd_append(x);
                } else if (c == "IDLE") {
                    if (!x.at_end()) {
                        bad("IDLE takes no arguments");
                    } else {
                        co_await cmd_idle();
                    }
                } else if (c == "GETQUOTA" || c == "GETQUOTAROOT") {
                    cmd_quota(x, c == "GETQUOTAROOT");
                } else if (c == "SETQUOTA") {
                    no("Quota limits are not set over IMAP", "NOPERM");
                } else if (c == "COMPRESS") {
                    co_await cmd_compress(x);
                } else if (c == "STARTTLS" || c == "LOGIN" || c == "AUTHENTICATE") {
                    bad("Already authenticated");
                } else if (state != State::selected) {
                    if (c == "CLOSE" || c == "UNSELECT" || c == "EXPUNGE" || c == "SEARCH" || c == "FETCH" || c == "STORE" || c == "COPY" || c == "MOVE"
                        || c == "SORT" || c == "THREAD" || c == "CHECK") {
                        bad("No mailbox selected");
                    } else {
                        bad("Unknown command");
                    }
                } else if (c == "CLOSE") {
                    cmd_close(true);
                } else if (c == "UNSELECT") {
                    cmd_close(false);
                } else if (c == "EXPUNGE") {
                    cmd_expunge(x, false);
                } else if (c == "SEARCH") {
                    cmd_search(x, false);
                } else if (c == "FETCH") {
                    co_await cmd_fetch(x, false);
                } else if (c == "STORE") {
                    cmd_store(x, false);
                } else if (c == "COPY" || c == "MOVE") {
                    cmd_copy(x, false, c == "MOVE");
                } else if (c == "SORT") {
                    cmd_sort(x, false);
                } else if (c == "THREAD") {
                    cmd_thread(x, false);
                } else {
                    bad("Unknown command");
                }
                co_return;
            }
            if (state != State::selected) {
                bad("No mailbox selected");
                co_return;
            }
            if (c == "FETCH") {
                co_await cmd_fetch(x, true);
            } else if (c == "STORE") {
                cmd_store(x, true);
            } else if (c == "COPY" || c == "MOVE") {
                cmd_copy(x, true, c == "MOVE");
            } else if (c == "SEARCH") {
                cmd_search(x, true);
            } else if (c == "EXPUNGE") {
                cmd_expunge(x, true);
            } else if (c == "SORT") {
                cmd_sort(x, true);
            } else if (c == "THREAD") {
                cmd_thread(x, true);
            } else {
                bad("Unknown UID command");
            }
        }

        // --- not authenticated ------------------------------------------------

        void cmd_id(Lexer& x) {
            if (!x.word("NIL")) {
                if (!x.eat('(')) {
                    bad("Invalid ID arguments");
                    return;
                }
                int n = 0;
                while (!x.eat(')')) {
                    x.eat(' ');
                    std::string_view k, v;
                    bool has;
                    if (!x.string(k, scratch) || !x.sp() || !x.nstring(v, has, scratch) || ++n > 30) {
                        bad("Invalid ID arguments");
                        return;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid ID arguments");
                return;
            }
            if (cfg->id.empty()) {
                out += "* ID NIL\r\n";
            } else {
                out += "* ID (";
                bool first = true;
                for (const auto& [k, v] : cfg->id) {
                    if (!first) {
                        out += ' ';
                    }
                    first = false;
                    put_string(out, k.view(), true);
                    out += ' ';
                    put_string(out, v.view(), true);
                }
                out += ")\r\n";
            }
            ok("ID completed");
        }

        async::task<void> cmd_starttls() {
            if (!cfg->tls || tls) {
                bad(tls ? "TLS already active" : "STARTTLS not available");
                co_return;
            }
            ok("Begin TLS negotiation now");
            if (!co_await flush()) {
                ended = true;
                co_return;
            }
            // what came in the clear after the command is dropped, never
            // read as the first bytes of the protected stream
            in.discard();
            auto t = co_await net::tls::async_server(conn, *cfg->tls);
            if (!t) {
                ended = true;
                co_return;
            }
            conn = *t;
            in.reset(conn);
            tls = true;
            replace_connection();
        }

        void replace_connection() {
            std::lock_guard<std::mutex> g(srv->lock);
            entry->c = conn;
        }

        uint64_t conn_id = 0;
        tracked_ptr<ConnEntry> entry;

        bool login_disabled() const noexcept {
            return cfg->tls && !tls;
        }

        bool check_password(const string& u, const string& p) {
            if (cfg->check_password) {
                return cfg->check_password(u, p);
            }
            if (cfg->backend->has_authenticate()) {
                return cfg->backend->authenticate(u, p);
            }
            return false;
        }

        // the capabilities change with the state: in the tagged OK
        void authenticated(const string& u) {
            user = u;
            state = State::authenticated;
            out += tag + " OK [CAPABILITY " + capabilities() + "] Logged in\r\n";
        }

        void auth_failed(const char* code = "AUTHENTICATIONFAILED") {
            ++auth_failures;
            no("Authentication failed", code);
            if (cfg->max_auth_failures > 0 && auth_failures >= cfg->max_auth_failures) {
                out += "* BYE Too many authentication failures\r\n";
                ended = true;
            }
        }

        void cmd_login(Lexer& x) {
            std::string_view u, p;
            std::string su;
            if (!x.astring(u, scratch)) {
                bad("Invalid LOGIN arguments");
                return;
            }
            su = std::string(u);
            if (!x.sp() || !x.astring(p, scratch) || !x.at_end()) {
                bad("Invalid LOGIN arguments");
                return;
            }
            if (login_disabled()) {
                no("LOGIN is disabled before STARTTLS", "PRIVACYREQUIRED");
                return;
            }
            const string name(su);
            if (su.empty() || !check_password(name, string(p))) {
                auth_failed();
                return;
            }
            authenticated(name);
        }

        // One line of an AUTHENTICATE exchange: the client's base64 answer
        // decoded; nullopt for "*" (cancelled), a line that is not base64,
        // or the end of the connection
        async::task<optional<std::string>> sasl_line(bool& cancelled) noexcept {
            cancelled = false;
            std::string line;
            auto r = co_await in.read_line(line, cfg->max_command);
            if (!r || *r != LineEnd::ok) {
                ended = true;
                co_return nullopt;
            }
            if (line == "*") {
                cancelled = true;
                co_return nullopt;
            }
            std::string decoded;
            for (unsigned char c : line) {
                if (base64_value(c) < 0 && c != '=') {
                    co_return nullopt;
                }
            }
            if (!decode_base64(line, decoded)) {
                co_return nullopt;
            }
            co_return decoded;
        }

        async::task<void> cmd_authenticate(Lexer& x) {
            std::string mech = to_upper(x.atom());
            optional<std::string> initial;
            if (x.eat(' ')) {
                std::string_view ir = x.run([](unsigned char c) { return c > ' ' && c < 127; });
                if (ir == "=") {
                    initial = std::string();
                } else {
                    std::string d;
                    for (unsigned char c : ir) {
                        if (base64_value(c) < 0 && c != '=') {
                            bad("Invalid initial response");
                            co_return;
                        }
                    }
                    if (!decode_base64(ir, d)) {
                        bad("Invalid initial response");
                        co_return;
                    }
                    initial = d;
                }
            }
            if (!x.at_end() || mech.empty()) {
                bad("Invalid AUTHENTICATE arguments");
                co_return;
            }
            const bool token = mech == "XOAUTH2" || mech == "OAUTHBEARER";
            if (mech != "PLAIN" && mech != "LOGIN" && !(token && cfg->check_token)) {
                no("Unsupported mechanism", "CANNOT");
                co_return;
            }
            if (login_disabled()) {
                no("Authentication is disabled before STARTTLS", "PRIVACYREQUIRED");
                co_return;
            }
            auto ask = [&](const char* challenge) -> async::task<optional<std::string>> {
                out += "+ ";
                out += challenge;
                out += "\r\n";
                if (!co_await flush()) {
                    ended = true;
                    co_return nullopt;
                }
                bool cancelled;
                auto a = co_await sasl_line(cancelled);
                if (!a) {
                    if (cancelled) {
                        bad("Authentication cancelled");
                    } else if (!ended) {
                        bad("Invalid base64");
                    }
                }
                co_return a;
            };
            if (mech == "PLAIN") {
                optional<std::string> resp = initial;
                if (!resp) {
                    resp = co_await ask("");
                    if (!resp) {
                        co_return;
                    }
                }
                // authzid NUL authcid NUL passwd
                const std::string& r = *resp;
                const size_t a = r.find('\0');
                const size_t b = a == std::string::npos ? std::string::npos : r.find('\0', a + 1);
                if (b == std::string::npos) {
                    bad("Invalid PLAIN response");
                    co_return;
                }
                std::string authz = r.substr(0, a), authc = r.substr(a + 1, b - a - 1), pass = r.substr(b + 1);
                if (!authz.empty() && authz != authc) {
                    auth_failed("AUTHORIZATIONFAILED");
                    co_return;
                }
                const string name(authc);
                if (authc.empty() || !check_password(name, string(pass))) {
                    auth_failed();
                    co_return;
                }
                authenticated(name);
                co_return;
            }
            if (mech == "LOGIN") {
                optional<std::string> u = initial;
                if (!u) {
                    u = co_await ask("VXNlcm5hbWU6");
                    if (!u) {
                        co_return;
                    }
                }
                auto p = co_await ask("UGFzc3dvcmQ6");
                if (!p) {
                    co_return;
                }
                const string name(*u);
                if (u->empty() || !check_password(name, string(*p))) {
                    auth_failed();
                    co_return;
                }
                authenticated(name);
                co_return;
            }
            // XOAUTH2: "user=" u ^A "auth=Bearer " t ^A ^A; OAUTHBEARER
            // (RFC 7628): gs2 header "n,a=user," then ^A pairs, auth=Bearer
            optional<std::string> resp = initial;
            if (!resp) {
                resp = co_await ask("");
                if (!resp) {
                    co_return;
                }
            }
            std::string u, t;
            std::string_view r = *resp;
            if (mech == "OAUTHBEARER") {
                const size_t first = r.find('\x01');
                if (first == std::string_view::npos) {
                    bad("Invalid OAUTHBEARER response");
                    co_return;
                }
                std::string_view gs2 = r.substr(0, first);
                size_t a = gs2.find("a=");
                if (a != std::string_view::npos) {
                    size_t e = gs2.find(',', a);
                    u = std::string(gs2.substr(a + 2, (e == std::string_view::npos ? gs2.size() : e) - a - 2));
                }
                r = r.substr(first);
            }
            size_t i = 0;
            while (i < r.size()) {
                size_t e = r.find('\x01', i);
                if (e == std::string_view::npos) {
                    e = r.size();
                }
                std::string_view kv = r.substr(i, e - i);
                if (kv.substr(0, 5) == "user=") {
                    u = std::string(kv.substr(5));
                } else if (kv.size() > 12 && iequal(kv.substr(0, 12), "auth=Bearer ")) {
                    t = std::string(kv.substr(12));
                }
                i = e + 1;
            }
            if (u.empty() || t.empty() || !cfg->check_token(string(u), string(t))) {
                // the error as a challenge, the client's empty answer, then NO
                out += "+ eyJzdGF0dXMiOiI0MDEiLCJzY2hlbWVzIjoiYmVhcmVyIn0=\r\n";
                if (!co_await flush()) {
                    ended = true;
                    co_return;
                }
                std::string line;
                auto rl = co_await in.read_line(line, cfg->max_command);
                if (!rl || *rl != LineEnd::ok) {
                    ended = true;
                    co_return;
                }
                auth_failed();
                co_return;
            }
            authenticated(string(u));
        }

        // --- authenticated --------------------------------------------------------

        void cmd_enable(Lexer& x) {
            std::string enabled;
            bool any = false;
            while (!x.at_end()) {
                x.eat(' ');
                std::string_view w = x.atom();
                if (w.empty()) {
                    bad("Invalid ENABLE arguments");
                    return;
                }
                any = true;
                const std::string u = to_upper(w);
                bool newly = false;
                if (u == "CONDSTORE") {
                    newly = !condstore;
                    condstore = true;
                } else if (u == "QRESYNC") {
                    newly = !qresync;
                    qresync = condstore = true;
                } else if (u == "UTF8=ACCEPT") {
                    newly = !utf8;
                    utf8 = true;
                } else if (u == "IMAP4REV2") {
                    newly = !rev2;
                    rev2 = utf8 = true;
                }
                if (newly) {
                    enabled += ' ';
                    enabled += u == "IMAP4REV2" ? "IMAP4rev2" : u;
                }
            }
            if (!any) {
                bad("ENABLE needs a capability");
                return;
            }
            out += "* ENABLED" + enabled + "\r\n";
            ok("ENABLE completed");
        }

        // The hub of a mailbox, made and loaded when no session has it
        expected<tracked_ptr<Hub>, io::error> hub_of(const string& name) {
            std::lock_guard<std::mutex> g(srv->lock);
            const string key = ServerImpl::key(user, name);
            auto it = srv->hubs.find(key);
            if (it != srv->hubs.end() && !it->second->dead) {
                return it->second;
            }
            auto c = cfg->backend->open(user, name);
            if (!c) {
                return unexpected(c.error());
            }
            tracked_ptr h = make_tracked<Hub>();
            h->user = user;
            h->name = name;
            h->backend = cfg->backend;
            h->load(*c);
            h->settle();
            srv->hubs[key] = h;
            return h;
        }

        // The selection ended (a SELECT of another, CLOSE, UNSELECT, the
        // end of the connection): the view taken out of its hub, the hub
        // dropped when no session has it any more
        void unselect(bool announce_closed) {
            if (!view) {
                return;
            }
            tracked_ptr<Hub> h = view->hub;
            {
                std::lock_guard<std::mutex> g(h->m);
                vector<tracked_ptr<View>> rest;
                for (auto& v : h->views) {
                    if (v != view) {
                        rest.push_back(v);
                    }
                }
                h->views = rest;
            }
            {
                std::lock_guard<std::mutex> g(srv->lock);
                std::lock_guard<std::mutex> g2(h->m);
                if (h->views.empty()) {
                    const string key = ServerImpl::key(h->user, h->name);
                    auto it = srv->hubs.find(key);
                    if (it != srv->hubs.end() && it->second == h) {
                        srv->hubs.erase(it);
                    }
                }
            }
            view = tracked_ptr<View>();
            mailbox = string();
            saved.clear();
            if (state == State::selected) {
                state = State::authenticated;
            }
            if (announce_closed) {
                out += "* OK [CLOSED] Previous mailbox closed\r\n";
            }
        }

        void cmd_select(Lexer& x, bool examine) {
            std::string_view raw;
            if (!x.astring(raw, scratch)) {
                bad("Invalid SELECT arguments");
                return;
            }
            string name;
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            // (CONDSTORE) / (QRESYNC (uidvalidity modseq [known-uids [seq-match]]))
            bool qr = false;
            uint32_t qr_validity = 0;
            uint64_t qr_modseq = 0;
            sequence_set qr_known;
            bool has_known = false;
            bool want_condstore = false;
            if (x.eat(' ')) {
                if (!x.eat('(')) {
                    bad("Invalid SELECT parameters");
                    return;
                }
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid SELECT parameters");
                        return;
                    }
                    first = false;
                    if (x.word("CONDSTORE")) {
                        want_condstore = true;
                    } else if (x.word("QRESYNC")) {
                        if (!qresync) {
                            bad("QRESYNC is not enabled");
                            return;
                        }
                        if (!x.sp() || !x.eat('(') || !x.nz_number(qr_validity) || !x.sp() || !x.number(qr_modseq)) {
                            bad("Invalid QRESYNC parameters");
                            return;
                        }
                        if (x.eat(' ')) {
                            if (x.peek() != '(') {
                                if (!SequenceAccess::read(qr_known, x)) {
                                    bad("Invalid QRESYNC parameters");
                                    return;
                                }
                                has_known = true;
                                x.eat(' ');
                            }
                            if (x.peek() == '(' && !x.skip_value()) {
                                bad("Invalid QRESYNC parameters");
                                return;
                            }
                        }
                        if (!x.eat(')')) {
                            bad("Invalid QRESYNC parameters");
                            return;
                        }
                        qr = true;
                    } else {
                        bad("Unknown SELECT parameter");
                        return;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid SELECT arguments");
                return;
            }
            const bool had = bool(view);
            unselect(had);
            if (want_condstore) {
                condstore = true;
            }
            auto h = hub_of(name);
            if (!h) {
                no_of(h.error(), "No such mailbox");
                return;
            }
            tracked_ptr v = make_tracked<View>();
            v->hub = *h;
            v->read_only = examine;
            Hub& hub = **h;
            std::string o;
            {
                std::lock_guard<std::mutex> g(hub.m);
                v->uids.reserve(hub.msgs.size());
                for (const auto& m : hub.msgs) {
                    v->uids.push_back(m.uid);
                }
                if (!examine) {
                    for (const auto& m : hub.msgs) {
                        if (m.uid >= hub.recent_from) {
                            v->recent.push_back(m.uid);
                        }
                    }
                    hub.recent_from = hub.uid_next;
                }
                hub.views.push_back(v);
                put_flags_lines(o, hub);
                o += "* ";
                put_number(o, v->uids.size());
                o += " EXISTS\r\n";
                if (!rev2) {
                    o += "* ";
                    put_number(o, v->recent.size());
                    o += " RECENT\r\n";
                    for (size_t i = 0; i < hub.msgs.size(); ++i) {
                        if (!(hub.msgs[i].flags & FlagSeen)) {
                            o += "* OK [UNSEEN ";
                            put_number(o, i + 1);
                            o += "] First unseen\r\n";
                            break;
                        }
                    }
                }
                o += "* OK [UIDVALIDITY ";
                put_number(o, hub.uid_validity);
                o += "] UIDs valid\r\n* OK [UIDNEXT ";
                put_number(o, hub.uid_next);
                o += "] Predicted next UID\r\n* OK [HIGHESTMODSEQ ";
                put_number(o, hub.highest_modseq);
                o += "] Highest\r\n";
                if (qr && qr_validity == hub.uid_validity) {
                    // the UIDs gone since: those of the known set (or of
                    // every UID below UIDNEXT) no longer there
                    const uint32_t top = hub.uid_next > 1 ? hub.uid_next - 1 : 0;
                    std::vector<uint32_t> gone;
                    if (has_known) {
                        for (auto [a, b] : qr_known.ranges()) {
                            uint32_t lo = a ? a : top, hi = b ? b : top;
                            if (lo > hi) {
                                std::swap(lo, hi);
                            }
                            hi = std::min(hi, top);
                            vanished_between(hub, lo, hi, gone);
                        }
                    } else if (top) {
                        vanished_between(hub, 1, top, gone);
                    }
                    if (!gone.empty()) {
                        o += "* VANISHED (EARLIER) ";
                        o += sequence_set(vector<uint32_t>(gone.begin(), gone.end())).to_string().view();
                        o += "\r\n";
                    }
                    for (size_t i = 0; i < hub.msgs.size(); ++i) {
                        const Msg& m = hub.msgs[i];
                        if (m.modseq > qr_modseq && (!has_known || qr_known.contains(m.uid, top))) {
                            o += "* ";
                            put_number(o, i + 1);
                            o += " FETCH (UID ";
                            put_number(o, m.uid);
                            o += " FLAGS ";
                            put_flags(o, m, v.get());
                            o += " MODSEQ (";
                            put_number(o, m.modseq);
                            o += "))\r\n";
                        }
                    }
                }
            }
            view = v;
            mailbox = name;
            state = State::selected;
            out += o;
            if (examine) {
                ok("EXAMINE completed", "READ-ONLY");
            } else {
                ok("SELECT completed", "READ-WRITE");
            }
        }

        static void vanished_between(const Hub& hub, uint32_t lo, uint32_t hi, std::vector<uint32_t>& gone) {
            if (lo == 0 || hi < lo) {
                return;
            }
            // the UIDs of [lo, hi] without a message: walk the messages there
            auto it = std::lower_bound(hub.msgs.begin(), hub.msgs.end(), lo, [](const Msg& a, uint32_t u) { return a.uid < u; });
            uint64_t next = lo;
            while (next <= hi) {
                const uint64_t until = it != hub.msgs.end() && it->uid <= hi ? it->uid : uint64_t(hi) + 1;
                for (uint64_t u = next; u < until && gone.size() < 2000000; ++u) {
                    gone.push_back(uint32_t(u));
                }
                if (it == hub.msgs.end() || it->uid > hi) {
                    break;
                }
                next = uint64_t(it->uid) + 1;
                ++it;
            }
        }

        void put_flags_lines(std::string& o, const Hub& hub) const {
            std::string kw;
            for (const auto& k : hub.keywords) {
                kw += ' ';
                kw += k;
            }
            o += "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft" + kw + ")\r\n";
            o += "* OK [PERMANENTFLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft" + kw + " \\*)] Flags permitted\r\n";
        }

        void put_flags(std::string& o, const Msg& m, const View* v) const {
            o += '(';
            bool first = true;
            for (uint8_t b : {FlagSeen, FlagAnswered, FlagFlagged, FlagDeleted, FlagDraft}) {
                if (m.flags & b) {
                    if (!first) {
                        o += ' ';
                    }
                    first = false;
                    o += system_flag_name(b);
                }
            }
            for (const auto& k : m.keywords) {
                if (!first) {
                    o += ' ';
                }
                first = false;
                o += k;
            }
            if (!rev2 && v && std::binary_search(v->recent.begin(), v->recent.end(), m.uid)) {
                if (!first) {
                    o += ' ';
                }
                o += "\\Recent";
            }
            o += ')';
        }

        void cmd_create(Lexer& x) {
            std::string_view raw;
            if (!x.astring(raw, scratch)) {
                bad("Invalid CREATE arguments");
                return;
            }
            string name;
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            string use;
            if (x.eat(' ')) {
                // (USE (\Sent)) — CREATE-SPECIAL-USE (RFC 6154 §3)
                if (!x.eat('(') || !x.word("USE") || !x.sp()) {
                    bad("Invalid CREATE parameters");
                    return;
                }
                vector<string> attrs;
                if (!parse_flag_list(x, attrs) || !x.eat(')')) {
                    bad("Invalid CREATE parameters");
                    return;
                }
                if (attrs.size() > 1) {
                    no("One special use per mailbox", "USEATTR");
                    return;
                }
                if (attrs.size() == 1) {
                    static constexpr const char* known[] = {"\\All", "\\Archive", "\\Drafts", "\\Flagged", "\\Junk", "\\Sent", "\\Trash"};
                    bool ok_attr = false;
                    for (const char* k : known) {
                        if (iequal(attrs[0].view(), k)) {
                            use = string(k);
                            ok_attr = true;
                        }
                    }
                    if (!ok_attr) {
                        no("Unknown special use", "USEATTR");
                        return;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid CREATE arguments");
                return;
            }
            std::string n(name.view());
            while (n.size() > 1 && n.back() == '/') {
                n.pop_back();   // a trailing delimiter: the intent to make children
            }
            name = string(canonical_mailbox(n, '/'));
            if (name == "INBOX") {
                no("INBOX exists", "ALREADYEXISTS");
                return;
            }
            if (!valid_mailbox_name(name.view())) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            auto r = [&] {
                    HubChange guard;
                    return cfg->backend->create(user, name, use);
                }();
            if (!r) {
                no_of(r.error(), "CREATE failed");
                return;
            }
            ok("CREATE completed");
        }

        void cmd_delete(Lexer& x) {
            std::string_view raw;
            string name;
            if (!x.astring(raw, scratch) || !x.at_end()) {
                bad("Invalid DELETE arguments");
                return;
            }
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            if (name == "INBOX") {
                no("INBOX cannot be deleted", "CANNOT");
                return;
            }
            if (view && mailbox == name) {
                unselect(false);
            }
            auto r = [&] {
                    HubChange guard;
                    return cfg->backend->remove(user, name);
                }();
            if (!r) {
                no_of(r.error(), "DELETE failed");
                return;
            }
            // the hub of exactly this name only: the children stay
            tracked_ptr<Hub> h = srv->find_hub(user, name);
            if (h) {
                {
                    std::lock_guard<std::mutex> g(srv->lock);
                    srv->hubs.erase(ServerImpl::key(user, name));
                }
                vector<function<void()>> wakes;
                {
                    std::lock_guard<std::mutex> g(h->m);
                    h->killed(wakes);
                }
                for (auto& w : wakes) {
                    w();
                }
            }
            ok("DELETE completed");
        }

        void cmd_rename(Lexer& x) {
            std::string_view a, b;
            std::string sa;
            if (!x.astring(a, scratch)) {
                bad("Invalid RENAME arguments");
                return;
            }
            sa = std::string(a);
            if (!x.sp() || !x.astring(b, scratch) || !x.at_end()) {
                bad("Invalid RENAME arguments");
                return;
            }
            string from, to;
            if (!name_in(sa, from) || !name_in(b, to)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            if (to == "INBOX") {
                no("INBOX exists", "ALREADYEXISTS");
                return;
            }
            if (!valid_mailbox_name(to.view())) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            if (view && (mailbox == from || is_under(mailbox.view(), from.view())) && from != "INBOX") {
                unselect(false);
            }
            auto r = [&] {
                    HubChange guard;
                    return cfg->backend->rename(user, from, to);
                }();
            if (!r) {
                no_of(r.error(), "RENAME failed");
                return;
            }
            if (from == "INBOX") {
                srv->changed_outside(user, from);
            } else {
                srv->drop_hubs(user, from);
            }
            ok("RENAME completed");
        }

        void cmd_subscribe(Lexer& x, bool on) {
            std::string_view raw;
            string name;
            if (!x.astring(raw, scratch) || !x.at_end()) {
                bad("Invalid arguments");
                return;
            }
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            auto r = cfg->backend->subscribe(user, name, on);
            if (!r) {
                no_of(r.error(), on ? "SUBSCRIBE failed" : "UNSUBSCRIBE failed");
                return;
            }
            ok(on ? "SUBSCRIBE completed" : "UNSUBSCRIBE completed");
        }

        // A LIST pattern against a name: "*" anything, "%" anything but
        // the delimiter
        static bool pattern_match(std::string_view p, std::string_view n, int depth = 0) noexcept {
            if (depth > 64) {
                return false;
            }
            size_t i = 0, j = 0;
            while (i < p.size()) {
                const char c = p[i];
                if (c == '*' || c == '%') {
                    ++i;
                    for (size_t k = j; k <= n.size(); ++k) {
                        if (pattern_match(p.substr(i), n.substr(k), depth + 1)) {
                            return true;
                        }
                        if (k < n.size() && c == '%' && n[k] == '/') {
                            return false;
                        }
                    }
                    return false;
                }
                if (j >= n.size() || n[j] != c) {
                    return false;
                }
                ++i;
                ++j;
            }
            return j == n.size();
        }

        void cmd_list(Lexer& x, bool lsub) {
            bool sel_subscribed = lsub, recursive = false, sel_special = false;
            bool ret_subscribed = false, ret_children = rev2, ret_special = false;
            bool extended = false;
            std::vector<std::string> status_items;
            if (!lsub && x.peek() == '(') {
                extended = true;
                x.eat('(');
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid LIST selection options");
                        return;
                    }
                    first = false;
                    std::string_view w = x.atom();
                    if (iequal(w, "SUBSCRIBED")) {
                        sel_subscribed = ret_subscribed = true;
                    } else if (iequal(w, "REMOTE")) {
                    } else if (iequal(w, "RECURSIVEMATCH")) {
                        recursive = true;
                    } else if (iequal(w, "SPECIAL-USE")) {
                        sel_special = ret_special = true;
                    } else {
                        bad("Unknown LIST selection option");
                        return;
                    }
                }
                if (recursive && !sel_subscribed && !sel_special) {
                    bad("RECURSIVEMATCH needs another selection option");
                    return;
                }
                if (!x.sp()) {
                    bad("Invalid LIST arguments");
                    return;
                }
            }
            std::string_view ref_raw;
            if (!x.astring(ref_raw, scratch) && !(x.peek() == '"')) {
                bad("Invalid LIST arguments");
                return;
            }
            std::string ref(ref_raw);
            if (!x.sp()) {
                bad("Invalid LIST arguments");
                return;
            }
            std::vector<std::string> patterns;
            if (!lsub && x.peek() == '(') {
                extended = true;
                x.eat('(');
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid LIST patterns");
                        return;
                    }
                    first = false;
                    std::string_view p;
                    if (!x.list_mailbox(p, scratch)) {
                        bad("Invalid LIST patterns");
                        return;
                    }
                    patterns.emplace_back(p);
                }
            } else {
                std::string_view p;
                if (x.peek() == '"' && x.source().substr(x.position(), 2) == "\"\"") {
                    x.seek(x.position() + 2);
                    p = {};
                    patterns.emplace_back();
                } else if (!x.list_mailbox(p, scratch)) {
                    bad("Invalid LIST arguments");
                    return;
                } else {
                    patterns.emplace_back(p);
                }
            }
            if (!lsub && x.eat(' ')) {
                extended = true;
                if (!x.word("RETURN") || !x.sp() || !x.eat('(')) {
                    bad("Invalid LIST return options");
                    return;
                }
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid LIST return options");
                        return;
                    }
                    first = false;
                    std::string_view w = x.atom();
                    if (iequal(w, "SUBSCRIBED")) {
                        ret_subscribed = true;
                    } else if (iequal(w, "CHILDREN")) {
                        ret_children = true;
                    } else if (iequal(w, "SPECIAL-USE")) {
                        ret_special = true;
                    } else if (iequal(w, "STATUS")) {
                        if (!x.sp() || !x.eat('(')) {
                            bad("Invalid STATUS return option");
                            return;
                        }
                        bool f2 = true;
                        while (!x.eat(')')) {
                            if (!f2 && !x.sp()) {
                                bad("Invalid STATUS return option");
                                return;
                            }
                            f2 = false;
                            std::string_view it = x.atom();
                            if (it.empty()) {
                                bad("Invalid STATUS return option");
                                return;
                            }
                            status_items.push_back(to_upper(it));
                        }
                    } else {
                        bad("Unknown LIST return option");
                        return;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid LIST arguments");
                return;
            }
            (void)extended;
            // names in UTF-8
            auto decode = [&](const std::string& s, std::string& o) {
                if (utf8) {
                    o = s;
                    return true;
                }
                return utf7_decode(s, o) || (!is_ascii(s) && valid_utf8(s) && (o = s, true));
            };
            std::string ref8;
            if (!decode(ref, ref8)) {
                ok(lsub ? "LSUB completed" : "LIST completed");
                return;
            }
            // LIST "" "": the delimiter
            if (!lsub && patterns.size() == 1 && patterns[0].empty()) {
                out += "* LIST (\\Noselect) \"/\" \"\"\r\n";
                ok("LIST completed");
                return;
            }
            auto all = cfg->backend->mailboxes(user);
            if (!all) {
                no_of(all.error(), "LIST failed");
                return;
            }
            // every name, with the parents implied by a child
            struct Entry {
                std::string name;
                std::vector<std::string> attrs;
                bool exists = false;
                bool subscribed = false;
                bool special = false;
                bool children = false;
                bool subscribed_children = false;
            };
            std::vector<Entry> entries;
            auto find = [&](const std::string& n) -> Entry* {
                for (auto& e : entries) {
                    if (e.name == n) {
                        return &e;
                    }
                }
                return nullptr;
            };
            bool has_inbox = false;
            for (const auto& le : *all) {
                Entry e;
                e.name = std::string(le.name.view());
                has_inbox |= e.name == "INBOX";
                e.exists = true;
                for (const auto& a : le.attributes) {
                    if (iequal(a.view(), "\\Subscribed")) {
                        e.subscribed = true;
                    } else if (iequal(a.view(), "\\NonExistent")) {
                        e.exists = false;
                    } else {
                        e.attrs.emplace_back(a.view());
                        if (a.view() != "\\Noselect" && a.view() != "\\Noinferiors" && a.view() != "\\Marked" && a.view() != "\\Unmarked") {
                            e.special = true;
                        }
                    }
                }
                if (Entry* old = find(e.name)) {
                    old->subscribed |= e.subscribed;
                    old->exists |= e.exists;
                } else {
                    entries.push_back(std::move(e));
                }
            }
            if (!has_inbox) {
                Entry e;
                e.name = "INBOX";
                e.exists = true;
                entries.push_back(std::move(e));
            }
            const size_t real = entries.size();
            for (size_t i = 0; i < real; ++i) {
                std::string n = entries[i].name;
                const bool live = entries[i].exists;
                const bool sub = entries[i].subscribed;
                size_t slash;
                while ((slash = n.rfind('/')) != std::string::npos && slash > 0) {
                    n = n.substr(0, slash);
                    Entry* p = find(n);
                    if (!p) {
                        Entry e;
                        e.name = n;
                        entries.push_back(std::move(e));
                        p = &entries.back();
                    }
                    p->children |= live;
                    p->subscribed_children |= sub;
                }
            }
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
                if (a.name == "INBOX") {
                    return b.name != "INBOX";
                }
                if (b.name == "INBOX") {
                    return false;
                }
                return a.name < b.name;
            });
            std::string full_ref = ref8;
            for (const auto& e : entries) {
                bool matched = false;
                for (const auto& p : patterns) {
                    std::string p8;
                    if (!decode(p, p8)) {
                        continue;
                    }
                    std::string pat = full_ref + p8;
                    // INBOX in the pattern in any case
                    if (istarts(pat, "INBOX") && (pat.size() == 5 || pat[5] == '/')) {
                        pat = "INBOX" + pat.substr(5);
                    }
                    matched |= pattern_match(pat, e.name);
                }
                if (!matched) {
                    continue;
                }
                const bool selected_by = sel_subscribed ? (e.subscribed || (recursive && e.subscribed_children)) : sel_special ? (e.special || (recursive && e.children)) : (e.exists || e.children);
                if (!selected_by) {
                    continue;
                }
                if (sel_special && !e.special && !recursive) {
                    continue;
                }
                std::vector<std::string> attrs;
                if (!e.exists) {
                    attrs.push_back(rev2 || extended ? "\\NonExistent" : "\\Noselect");
                }
                if (ret_subscribed && e.subscribed) {
                    attrs.push_back("\\Subscribed");
                }
                if (!lsub && (ret_children || !extended)) {
                    attrs.push_back(e.children ? "\\HasChildren" : "\\HasNoChildren");
                }
                for (const auto& a : e.attrs) {
                    const bool is_use = a != "\\Noselect" && a != "\\Noinferiors" && a != "\\Marked" && a != "\\Unmarked";
                    if (!is_use || ret_special || !sel_special) {
                        attrs.push_back(a);
                    }
                }
                if (lsub && !e.subscribed) {
                    // LSUB: a parent of a subscribed child, not subscribed itself
                    if (!e.subscribed_children) {
                        continue;
                    }
                    attrs.clear();
                    attrs.push_back("\\Noselect");
                }
                out += lsub ? "* LSUB (" : "* LIST (";
                for (size_t i = 0; i < attrs.size(); ++i) {
                    if (i) {
                        out += ' ';
                    }
                    out += attrs[i];
                }
                out += ") \"/\" ";
                put_name(out, e.name);
                if (recursive && !e.subscribed && e.subscribed_children && sel_subscribed) {
                    out += " (\"CHILDINFO\" (\"SUBSCRIBED\"))";
                }
                out += "\r\n";
                if (!status_items.empty() && e.exists) {
                    std::string line;
                    if (status_line(string(e.name), status_items, line, false)) {
                        out += line;
                    }
                }
            }
            ok(lsub ? "LSUB completed" : "LIST completed");
        }

        // "* STATUS name (...)\r\n" of a mailbox; false (and a tagged NO
        // when tagged_errors) for one that cannot be opened or an item
        // unknown
        bool status_line(const string& name, const std::vector<std::string>& items, std::string& line, bool tagged_errors) {
            struct Counts {
                uint32_t messages = 0, unseen = 0, deleted = 0, uid_next = 0, uid_validity = 0, recent = 0;
                uint64_t size = 0, modseq = 0;
            } c;
            tracked_ptr<Hub> h = srv->find_hub(user, name);
            if (h) {
                std::lock_guard<std::mutex> g(h->m);
                c.messages = uint32_t(h->msgs.size());
                for (const auto& m : h->msgs) {
                    c.unseen += !(m.flags & FlagSeen);
                    c.deleted += (m.flags & FlagDeleted) != 0;
                    c.size += m.size;
                    c.recent += m.uid >= h->recent_from;
                }
                c.uid_next = h->uid_next;
                c.uid_validity = h->uid_validity;
                c.modseq = h->highest_modseq;
            } else {
                auto mb = cfg->backend->open(user, name);
                if (!mb) {
                    if (tagged_errors) {
                        no_of(mb.error(), "STATUS failed");
                    }
                    return false;
                }
                c.messages = uint32_t(mb->messages.size());
                for (const auto& m : mb->messages) {
                    bool seen = false, del = false;
                    for (const auto& f : m.flags) {
                        seen |= iequal(f.view(), "\\Seen");
                        del |= iequal(f.view(), "\\Deleted");
                    }
                    c.unseen += !seen;
                    c.deleted += del;
                    c.size += m.size;
                }
                c.uid_next = mb->uid_next;
                c.uid_validity = mb->uid_validity;
                c.modseq = mb->highest_modseq;
            }
            line = "* STATUS ";
            put_name(line, name.view());
            line += " (";
            bool first = true;
            for (const auto& it : items) {
                if (!first) {
                    line += ' ';
                }
                first = false;
                line += it;
                line += ' ';
                if (it == "MESSAGES") {
                    put_number(line, c.messages);
                } else if (it == "UNSEEN") {
                    put_number(line, c.unseen);
                } else if (it == "UIDNEXT") {
                    put_number(line, c.uid_next);
                } else if (it == "UIDVALIDITY") {
                    put_number(line, c.uid_validity);
                } else if (it == "DELETED") {
                    put_number(line, c.deleted);
                } else if (it == "SIZE") {
                    put_number(line, c.size);
                } else if (it == "HIGHESTMODSEQ") {
                    put_number(line, c.modseq);
                    condstore = true;
                } else if (it == "RECENT" && !rev2) {
                    put_number(line, c.recent);
                } else {
                    if (tagged_errors) {
                        bad("Unknown STATUS item");
                    }
                    return false;
                }
            }
            line += ")\r\n";
            return true;
        }

        void cmd_status(Lexer& x) {
            std::string_view raw;
            string name;
            if (!x.astring(raw, scratch) || !x.sp() || !x.eat('(')) {
                bad("Invalid STATUS arguments");
                return;
            }
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            std::vector<std::string> items;
            bool first = true;
            while (!x.eat(')')) {
                if (!first && !x.sp()) {
                    bad("Invalid STATUS arguments");
                    return;
                }
                first = false;
                std::string_view w = x.atom();
                if (w.empty()) {
                    bad("Invalid STATUS arguments");
                    return;
                }
                items.push_back(to_upper(w));
            }
            if (!x.at_end() || items.empty()) {
                bad("Invalid STATUS arguments");
                return;
            }
            std::string line;
            if (!status_line(name, items, line, true)) {
                return;
            }
            out += line;
            ok("STATUS completed");
        }

        void cmd_append(Lexer& x) {
            std::string_view raw;
            string name;
            if (!x.astring(raw, scratch) || !x.sp()) {
                bad("Invalid APPEND arguments");
                return;
            }
            if (!name_in(raw, name)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            struct One {
                vector<string> flags;
                DateTime date;
                bool has_date = false;
                std::string_view text;
            };
            vector<One> msgs;
            // 1*( [flags SP] [date-time SP] (literal / "UTF8" SP "(" literal8 ")") )
            for (;;) {
                One m;
                if (x.peek() == '(') {
                    if (!parse_flag_list(x, m.flags) || !x.sp()) {
                        bad("Invalid APPEND flags");
                        return;
                    }
                    for (const auto& f : m.flags) {
                        if (iequal(f.view(), "\\Recent") || (f.view()[0] == '\\' && !system_flag(f.view()))) {
                            bad("Invalid APPEND flag");
                            return;
                        }
                    }
                }
                if (x.peek() == '"') {
                    std::string_view d;
                    if (!x.string(d, scratch) || !parse_date_time(d, m.date) || !x.sp()) {
                        bad("Invalid APPEND date-time");
                        return;
                    }
                    m.has_date = true;
                }
                bool wrapped = false;
                if (x.word("UTF8")) {
                    if (!x.sp() || !x.eat('(')) {
                        bad("Invalid APPEND UTF8 data");
                        return;
                    }
                    wrapped = true;
                } else if (x.word("CATENATE")) {
                    no("CATENATE is not supported", "CANNOT");
                    return;
                }
                if (!x.literal(m.text, true)) {
                    bad("APPEND needs a literal");
                    return;
                }
                if (wrapped && !x.eat(')')) {
                    bad("Invalid APPEND UTF8 data");
                    return;
                }
                if (m.text.empty()) {
                    no("Empty message", "CANNOT");
                    return;
                }
                msgs.push_back(std::move(m));
                if (x.at_end()) {
                    break;
                }
                if (!x.sp()) {
                    bad("Invalid APPEND arguments");
                    return;
                }
            }
            // the mailbox there?
            uint32_t validity = 0;
            std::vector<uint32_t> uids;
            tracked_ptr<Hub> h = srv->find_hub(user, name);
            vector<function<void()>> wakes;
            for (auto& m : msgs) {
                const time_t now = std::time(nullptr);
                DateTime d = m.has_date ? m.date : DateTime{int64_t(now), 0};
                time::datetime when = time::datetime::from_unix(d.unix, time::zone::fixed(duration(std::chrono::minutes(d.offset))));
                auto r = [&] {
                    HubChange guard;
                    return cfg->backend->append(user, name, string(m.text), m.flags, when);
                }();
                if (!r) {
                    if (r.error().code() == make_error_code(errc::nonexistent)) {
                        no("No such mailbox", "TRYCREATE");
                    } else {
                        no_of(r.error(), "APPEND failed");
                    }
                    for (auto& w : wakes) {
                        w();
                    }
                    return;
                }
                uids.push_back(r->uid);
                if (h) {
                    std::lock_guard<std::mutex> g(h->m);
                    h->added(*r, wakes);
                    h->settle();
                    validity = h->uid_validity;
                }
            }
            for (auto& w : wakes) {
                w();
            }
            if (!validity) {
                auto v = cfg->backend->uid_validity(user, name);
                if (v) {
                    validity = *v;
                }
            }
            if (view && mailbox == name) {
                updates(true);
            }
            std::string code = "APPENDUID ";
            put_number(code, validity);
            code += ' ';
            code += sequence_set(vector<uint32_t>(uids.begin(), uids.end())).to_string().view();
            if (validity) {
                ok("APPEND completed", code.c_str());
            } else {
                ok("APPEND completed");
            }
        }

        async::task<void> cmd_idle() {
            out += "+ idling\r\n";
            if (!co_await flush()) {
                ended = true;
                co_return;
            }
            // announcements while idling: written by the wake's task
            tracked_ptr<Session> self = me;
            if (view) {
                std::lock_guard<std::mutex> g(view->hub->m);
                view->wake = function<void()>([self]() { async::go(idle_push(self)); });
            }
            idling.store(true);
            async::go(idle_push(me));   // what is pending already
            // a poll of the backend for changes made outside the server
            async::stop_source poll;
            if (view && cfg->poll_interval > duration::zero()) {
                async::go(idle_poll(me, poll.token()));
            }
            std::string line;
            entry->state.store(ConnEntry::idle);   // a shutdown may end the wait
            auto r = co_await in.read_line(line, cfg->max_command);
            poll.request_stop();
            int expected_state = ConnEntry::idle;
            if (!entry->state.compare_exchange_strong(expected_state, ConnEntry::active)) {
                ended = true;
            }
            {
                auto g = co_await out_lock.scoped_lock();
                idling.store(false);
                if (view) {
                    std::lock_guard<std::mutex> g2(view->hub->m);
                    view->wake = function<void()>();
                }
            }
            if (!r || *r != LineEnd::ok) {
                ended = true;
                co_return;
            }
            updates(true);
            if (iequal(line, "DONE")) {
                ok("IDLE terminated");
            } else {
                bad("Expected DONE");
            }
        }

        tracked_ptr<Session> me;   // the session's own pointer, for its tasks

        static async::task<void> idle_push(tracked_ptr<Session> self) noexcept {
            Session& s = *self;
            auto g = co_await s.out_lock.scoped_lock();
            if (!s.idling.load()) {
                co_return;
            }
            std::string saved_out;
            saved_out.swap(s.out);
            s.updates(true);
            std::string pushed;
            pushed.swap(s.out);
            s.out.swap(saved_out);
            if (!pushed.empty()) {
                (void)co_await write_all(s.conn, pushed);
            }
        }

        static async::task<void> idle_poll(tracked_ptr<Session> self, async::stop_token stop) noexcept {
            Session& s = *self;
            while (!stop.stop_requested()) {
                co_await async::select(stop.channel().on_receive([] {}), async::timeout(sgcl::clock::now() + s.cfg->poll_interval, [] {}));
                if (stop.stop_requested() || !s.idling.load()) {
                    break;
                }
                s.refresh();
                co_await idle_push(self);
            }
        }

        void cmd_quota(Lexer& x, bool root) {
            std::string_view raw;
            if (!x.astring(raw, scratch) && !(root == false && x.source().substr(x.position(), 2) == "\"\"")) {
                bad("Invalid quota arguments");
                return;
            }
            if (!root && raw.empty() && x.source().substr(x.position(), 2) == "\"\"") {
                x.seek(x.position() + 2);
            }
            if (!x.at_end()) {
                bad("Invalid quota arguments");
                return;
            }
            if (!cfg->backend->has_quota()) {
                no("No quota", "CANNOT");
                return;
            }
            if (root) {
                string name;
                if (!name_in(raw, name)) {
                    no("Invalid mailbox name", "CANNOT");
                    return;
                }
                out += "* QUOTAROOT ";
                put_name(out, name.view());
                out += " \"\"\r\n";
            } else if (!raw.empty()) {
                no("No such quota root", "NONEXISTENT");
                return;
            }
            auto q = cfg->backend->quota(user);
            if (!q) {
                no_of(q.error(), "Quota unavailable");
                return;
            }
            out += "* QUOTA \"\" (";
            bool any = false;
            if (q->storage_limit) {
                out += "STORAGE ";
                put_number(out, q->storage_used);
                out += ' ';
                put_number(out, q->storage_limit);
                any = true;
            }
            if (q->messages_limit) {
                if (any) {
                    out += ' ';
                }
                out += "MESSAGE ";
                put_number(out, q->messages_used);
                out += ' ';
                put_number(out, q->messages_limit);
            }
            out += ")\r\n";
            ok(root ? "GETQUOTAROOT completed" : "GETQUOTA completed");
        }

        async::task<void> cmd_compress(Lexer& x) {
            if (!x.word("DEFLATE") || !x.at_end()) {
                bad("Unknown compression");
                co_return;
            }
            if (!cfg->compress) {
                no("Compression is off", "CANNOT");
                co_return;
            }
            if (compressed) {
                no("Already compressed", "COMPRESSIONACTIVE");
                co_return;
            }
            ok("DEFLATE active");
            if (!co_await flush()) {
                ended = true;
                co_return;
            }
            conn = deflate_connection(conn, in.pending());
            in.discard();
            in.reset(conn);
            compressed = true;
            replace_connection();
        }

        // --- selected ---------------------------------------------------------

        // The backend read again for changes from outside the server
        void refresh() {
            if (!view) {
                return;
            }
            tracked_ptr<Hub> h = view->hub;
            vector<function<void()>> wakes;
            {
                std::lock_guard<std::mutex> g(h->m);
                if (!h->dead) {
                    h->refresh(wakes);
                }
            }
            for (auto& w : wakes) {
                // this session's own wake is idle_push; others' are theirs
                w();
            }
        }

        // What happened to the selected mailbox announced: EXPUNGE (or
        // VANISHED) when allowed, EXISTS, RECENT, FETCH of flags, FLAGS
        void updates(bool expunges) {
            if (!view) {
                return;
            }
            Hub& h = *view->hub;
            std::lock_guard<std::mutex> g(h.m);
            View& v = *view;
            if (v.dead) {
                out += "* BYE [NONEXISTENT] The selected mailbox was deleted\r\n";
                ended = true;
                return;
            }
            if (expunges && !v.gone_uids.empty()) {
                std::vector<uint32_t> gone = v.gone_uids;
                v.gone_uids.clear();
                std::sort(gone.begin(), gone.end());
                gone.erase(std::unique(gone.begin(), gone.end()), gone.end());
                std::vector<uint32_t> vanished;
                // descending, so that each number is right when it is sent
                for (size_t i = gone.size(); i-- > 0;) {
                    const uint32_t seq = v.seq_of(gone[i]);
                    if (!seq) {
                        continue;
                    }
                    v.uids.erase(v.uids.begin() + (seq - 1));
                    v.recent.erase(std::remove(v.recent.begin(), v.recent.end(), gone[i]), v.recent.end());
                    if (qresync) {
                        vanished.push_back(gone[i]);
                    } else {
                        out += "* ";
                        put_number(out, seq);
                        out += " EXPUNGE\r\n";
                    }
                }
                if (!vanished.empty()) {
                    out += "* VANISHED ";
                    out += sequence_set(vector<uint32_t>(vanished.begin(), vanished.end())).to_string().view();
                    out += "\r\n";
                }
            }
            if (v.keywords_changed) {
                v.keywords_changed = false;
                put_flags_lines(out, h);
            }
            if (!v.new_uids.empty()) {
                std::vector<uint32_t> fresh = v.new_uids;
                v.new_uids.clear();
                std::sort(fresh.begin(), fresh.end());
                bool any = false;
                for (uint32_t u : fresh) {
                    if (!h.find(u) || v.seq_of(u)) {
                        continue;
                    }
                    if (!v.uids.empty() && u < v.uids.back()) {
                        v.uids.insert(std::lower_bound(v.uids.begin(), v.uids.end(), u), u);
                    } else {
                        v.uids.push_back(u);
                    }
                    if (!v.read_only && u >= h.recent_from) {
                        v.recent.push_back(u);
                    }
                    any = true;
                }
                if (!v.read_only && h.uid_next > h.recent_from && any) {
                    h.recent_from = h.uid_next;
                }
                if (any) {
                    out += "* ";
                    put_number(out, v.uids.size());
                    out += " EXISTS\r\n";
                    if (!rev2) {
                        out += "* ";
                        put_number(out, v.recent.size());
                        out += " RECENT\r\n";
                    }
                }
            }
            if (!v.changed_uids.empty()) {
                std::vector<uint32_t> ch = v.changed_uids;
                v.changed_uids.clear();
                std::sort(ch.begin(), ch.end());
                ch.erase(std::unique(ch.begin(), ch.end()), ch.end());
                for (uint32_t u : ch) {
                    const uint32_t seq = v.seq_of(u);
                    const Msg* m = h.find(u);
                    if (!seq || !m) {
                        continue;
                    }
                    out += "* ";
                    put_number(out, seq);
                    out += " FETCH (UID ";
                    put_number(out, u);
                    out += " FLAGS ";
                    put_flags(out, *m, &v);
                    if (condstore) {
                        out += " MODSEQ (";
                        put_number(out, m->modseq);
                        out += ')';
                    }
                    out += ")\r\n";
                }
            }
        }

        // The messages of a set: (sequence number, UID) of the view, in
        // ascending order; false (BAD sent) for a number past the last
        bool messages_of(const sequence_set& set, bool uid, std::vector<std::pair<uint32_t, uint32_t>>& outv) {
            const View& v = *view;
            if (set.is_saved()) {
                for (uint32_t u : saved) {
                    if (uint32_t s = v.seq_of(u)) {
                        outv.emplace_back(s, u);
                    }
                }
                return true;
            }
            const uint32_t count = uint32_t(v.uids.size());
            if (uid) {
                const uint32_t largest = count ? v.uids.back() : 0;
                if (!count) {
                    return true;
                }
                for (uint32_t i = 0; i < count; ++i) {
                    if (set.contains(v.uids[i], largest)) {
                        outv.emplace_back(i + 1, v.uids[i]);
                    }
                }
                return true;
            }
            for (auto [a, b] : set.ranges()) {
                if ((a && a > count) || (b && b > count)) {
                    bad("Invalid message sequence number");
                    return false;
                }
            }
            if (!count) {
                return true;
            }
            for (uint32_t i = 0; i < count; ++i) {
                if (set.contains(i + 1, count)) {
                    outv.emplace_back(i + 1, v.uids[i]);
                }
            }
            return true;
        }

        bool read_set(Lexer& x, sequence_set& set) {
            if (x.eat('$')) {
                set = sequence_set::saved();
                return true;
            }
            return SequenceAccess::read(set, x);
        }

        void cmd_close(bool expunge) {
            if (expunge && !view->read_only) {
                std::vector<uint32_t> del;
                {
                    std::lock_guard<std::mutex> g(view->hub->m);
                    for (uint32_t u : view->uids) {
                        const Msg* m = view->hub->find(u);
                        if (m && (m->flags & FlagDeleted)) {
                            del.push_back(u);
                        }
                    }
                }
                if (!del.empty()) {
                    (void)expunge_uids(del, false);
                }
            }
            unselect(false);
            ok(expunge ? "CLOSE completed" : "UNSELECT completed");
        }

        // The UIDs expunged through the backend and the hub; the EXPUNGE
        // (or VANISHED) responses of this session when announce
        bool expunge_uids(const std::vector<uint32_t>& uids, bool announce) {
            Hub& h = *view->hub;
            vector<function<void()>> wakes;
            {
                std::lock_guard<std::mutex> g(h.m);
                std::vector<uint32_t> live;
                for (uint32_t u : uids) {
                    if (h.find(u)) {
                        live.push_back(u);
                    }
                }
                if (live.empty()) {
                    return true;
                }
                auto r = [&] {
                    HubChange guard;
                    return cfg->backend->expunge(user, mailbox, vector<uint32_t>(live.begin(), live.end()));
                }();
                if (!r) {
                    no_of(r.error(), "EXPUNGE failed");
                    return false;
                }
                h.removed(live, *r, view.get(), wakes);
                h.settle();
                // this session's own numbers
                std::sort(live.begin(), live.end());
                std::vector<uint32_t> vanished;
                for (size_t i = live.size(); i-- > 0;) {
                    const uint32_t seq = view->seq_of(live[i]);
                    if (!seq) {
                        continue;
                    }
                    view->uids.erase(view->uids.begin() + (seq - 1));
                    view->recent.erase(std::remove(view->recent.begin(), view->recent.end(), live[i]), view->recent.end());
                    if (!announce) {
                        continue;
                    }
                    if (qresync) {
                        vanished.push_back(live[i]);
                    } else {
                        out += "* ";
                        put_number(out, seq);
                        out += " EXPUNGE\r\n";
                    }
                }
                if (!vanished.empty()) {
                    out += "* VANISHED ";
                    out += sequence_set(vector<uint32_t>(vanished.begin(), vanished.end())).to_string().view();
                    out += "\r\n";
                }
                saved.erase(std::remove_if(saved.begin(), saved.end(), [&](uint32_t u) { return std::binary_search(live.begin(), live.end(), u); }), saved.end());
            }
            for (auto& w : wakes) {
                w();
            }
            return true;
        }

        void cmd_expunge(Lexer& x, bool uid) {
            sequence_set set;
            if (uid && (!read_set(x, set) || !x.at_end())) {
                bad("Invalid UID EXPUNGE arguments");
                return;
            }
            if (!uid && !x.at_end()) {
                bad("EXPUNGE takes no arguments");
                return;
            }
            if (view->read_only) {
                no("Mailbox is read-only", "READ-ONLY");
                return;
            }
            std::vector<std::pair<uint32_t, uint32_t>> in_set;
            if (uid && !messages_of(set, true, in_set)) {
                return;
            }
            std::vector<uint32_t> del;
            {
                std::lock_guard<std::mutex> g(view->hub->m);
                if (uid) {
                    for (auto [s, u] : in_set) {
                        const Msg* m = view->hub->find(u);
                        if (m && (m->flags & FlagDeleted)) {
                            del.push_back(u);
                        }
                    }
                } else {
                    for (uint32_t u : view->uids) {
                        const Msg* m = view->hub->find(u);
                        if (m && (m->flags & FlagDeleted)) {
                            del.push_back(u);
                        }
                    }
                }
            }
            updates(true);
            if (!expunge_uids(del, true)) {
                return;
            }
            if (condstore) {
                std::string code = "HIGHESTMODSEQ ";
                {
                    std::lock_guard<std::mutex> g(view->hub->m);
                    put_number(code, view->hub->highest_modseq);
                }
                ok("EXPUNGE completed", code.c_str());
            } else {
                ok("EXPUNGE completed");
            }
        }

        // The content of a message, read through the backend
        expected<string, io::error> content_of(uint32_t uid) {
            return cfg->backend->read(user, mailbox, uid);
        }

        // SEARCH's work: the program read and judged; the matches as
        // (seq, uid) in ascending order
        bool run_search(Lexer& x, SearchParse& sp, std::vector<std::pair<uint32_t, uint32_t>>& matches, uint64_t& max_modseq) {
            SearchKey root;
            if (!parse_search_keys(x, root, sp, scratch, 0)) {
                bad("Invalid search criteria");
                return false;
            }
            if (!x.at_end()) {
                bad("Invalid search criteria");
                return false;
            }
            if (sp.uses_modseq) {
                condstore = true;
            }
            // a snapshot of the messages' data under the lock
            struct Item {
                uint32_t seq, uid;
                uint8_t flags;
                std::vector<std::string> keywords;
                uint64_t size, modseq;
                DateTime date;
                bool recent;
            };
            std::vector<Item> items;
            uint32_t largest_uid = 0;
            {
                Hub& h = *view->hub;
                std::lock_guard<std::mutex> g(h.m);
                items.reserve(view->uids.size());
                for (size_t i = 0; i < view->uids.size(); ++i) {
                    const uint32_t u = view->uids[i];
                    const Msg* m = h.find(u);
                    if (!m) {
                        continue;
                    }
                    items.push_back(Item{uint32_t(i + 1), u, m->flags, m->keywords, m->size, m->modseq, m->date,
                                         std::binary_search(view->recent.begin(), view->recent.end(), u)});
                }
                largest_uid = view->uids.empty() ? 0 : view->uids.back();
            }
            const int64_t now = int64_t(std::time(nullptr));
            std::vector<uint32_t> saved_sorted = saved;
            std::sort(saved_sorted.begin(), saved_sorted.end());
            max_modseq = 0;
            for (const auto& it : items) {
                SearchMessage m;
                m.seq = it.seq;
                m.uid = it.uid;
                m.flags = it.flags;
                m.keywords = &it.keywords;
                m.size = it.size;
                m.internal_date = it.date;
                m.modseq = it.modseq;
                m.recent = it.recent;
                m.largest_seq = uint32_t(view->uids.size());
                m.largest_uid = largest_uid;
                m.now = now;
                m.saved = &saved_sorted;
                string content;
                std::string bytes;
                bool loaded = false;
                if (sp.needs_content) {
                    m.content = [&]() -> const std::string* {
                        if (!loaded) {
                            loaded = true;
                            auto c = content_of(it.uid);
                            if (!c) {
                                return nullptr;
                            }
                            bytes = std::string(c->view());
                        }
                        return &bytes;
                    };
                }
                std::unique_ptr<SearchText> text;
                if (judge(root, m, text)) {
                    matches.emplace_back(it.seq, it.uid);
                    max_modseq = std::max(max_modseq, it.modseq);
                }
            }
            return true;
        }

        // CHARSET of SEARCH, SORT, THREAD: one this side reads into UTF-8,
        // else BADCHARSET
        bool charset_of(std::string_view cs, SearchParse& sp) {
            if (iequal(cs, "UTF-8") || iequal(cs, "US-ASCII")) {
                return true;
            }
            if (txt::encoding_from_name(string(cs))) {
                sp.charset = std::string(cs);
                return true;
            }
            no("Unknown charset", "BADCHARSET (US-ASCII UTF-8)");
            return false;
        }

        void cmd_search(Lexer& x, bool uid) {
            // [RETURN (options)] [CHARSET name] keys
            bool esearch = rev2;
            bool r_min = false, r_max = false, r_all = false, r_count = false, r_save = false;
            if (x.word("RETURN")) {
                esearch = true;
                if (!x.sp() || !x.eat('(')) {
                    bad("Invalid RETURN options");
                    return;
                }
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid RETURN options");
                        return;
                    }
                    first = false;
                    std::string_view w = x.atom();
                    if (iequal(w, "MIN")) {
                        r_min = true;
                    } else if (iequal(w, "MAX")) {
                        r_max = true;
                    } else if (iequal(w, "ALL")) {
                        r_all = true;
                    } else if (iequal(w, "COUNT")) {
                        r_count = true;
                    } else if (iequal(w, "SAVE")) {
                        r_save = true;
                    } else {
                        bad("Unknown RETURN option");
                        return;
                    }
                }
                if (!r_min && !r_max && !r_count && !r_save) {
                    r_all = true;
                }
                if (!r_min && !r_max && !r_count && !r_all && r_save) {
                    // SAVE alone: nothing returned
                }
                if (!x.sp()) {
                    bad("Missing search criteria");
                    return;
                }
            } else if (rev2) {
                r_all = true;
            }
            SearchParse sp;
            if (x.word("CHARSET")) {
                std::string_view cs;
                if (!x.sp() || !x.astring(cs, scratch) || !x.sp()) {
                    bad("Invalid CHARSET");
                    return;
                }
                std::string c(cs);
                if (!charset_of(c, sp)) {
                    return;
                }
            }
            std::vector<std::pair<uint32_t, uint32_t>> matches;
            uint64_t modseq = 0;
            if (!run_search(x, sp, matches, modseq)) {
                if (r_save) {
                    saved.clear();
                }
                return;
            }
            if (r_save) {
                saved.clear();
                if (r_min && !matches.empty() && !r_all && !r_count && !r_max) {
                    saved.push_back(matches.front().second);
                } else if (r_max && !matches.empty() && !r_all && !r_count && !r_min) {
                    saved.push_back(matches.back().second);
                } else if (r_min && r_max && !r_all && !r_count && !matches.empty()) {
                    saved.push_back(matches.front().second);
                    saved.push_back(matches.back().second);
                } else {
                    for (auto& m : matches) {
                        saved.push_back(m.second);
                    }
                }
            }
            if (esearch) {
                if (r_save && !r_min && !r_max && !r_count && !r_all) {
                    updates(uid);
                    ok("SEARCH completed");
                    return;
                }
                out += "* ESEARCH (TAG ";
                put_quoted(out, tag);
                out += ')';
                if (uid) {
                    out += " UID";
                }
                if (!matches.empty()) {
                    if (r_min) {
                        out += " MIN ";
                        put_number(out, uid ? matches.front().second : matches.front().first);
                    }
                    if (r_max) {
                        out += " MAX ";
                        put_number(out, uid ? matches.back().second : matches.back().first);
                    }
                    if (r_all) {
                        out += " ALL ";
                        std::vector<uint32_t> nums;
                        for (auto& m : matches) {
                            nums.push_back(uid ? m.second : m.first);
                        }
                        out += sequence_set(vector<uint32_t>(nums.begin(), nums.end())).to_string().view();
                    }
                }
                if (r_count) {
                    out += " COUNT ";
                    put_number(out, matches.size());
                }
                if (sp.uses_modseq && !matches.empty()) {
                    out += " MODSEQ ";
                    put_number(out, modseq);
                }
                out += "\r\n";
            } else {
                out += "* SEARCH";
                for (auto& m : matches) {
                    out += ' ';
                    put_number(out, uid ? m.second : m.first);
                }
                if (sp.uses_modseq && !matches.empty()) {
                    out += " (MODSEQ ";
                    put_number(out, modseq);
                    out += ')';
                }
                out += "\r\n";
            }
            updates(uid);
            ok("SEARCH completed");
        }

        // The sort information of a message, read once and kept in its cache
        const SortInfo* sort_info_of(uint32_t uid, Msg& snapshot) {
            auto cache = snapshot.cache;
            {
                std::lock_guard<std::mutex> g(cache->m);
                if (cache->sort) {
                    return cache->sort.get();
                }
            }
            auto c = content_of(uid);
            auto info = std::make_unique<SortInfo>(c ? sort_info(c->view(), snapshot.date) : SortInfo{snapshot.date, {}, false, {}, {}, {}, {}, {}, {}, {}});
            std::lock_guard<std::mutex> g(cache->m);
            if (!cache->sort) {
                cache->sort = std::move(info);
            }
            return cache->sort.get();
        }

        // SORT and THREAD: the matches with what they are sorted by
        bool sort_items_of(Lexer& x, std::vector<SortItem>& items, std::vector<std::shared_ptr<MsgCache>>& keep) {
            std::string_view cs;
            if (!x.astring(cs, scratch) || !x.sp()) {
                bad("Invalid charset");
                return false;
            }
            SearchParse sp;
            if (!charset_of(std::string(cs), sp)) {
                return false;
            }
            std::vector<std::pair<uint32_t, uint32_t>> matches;
            uint64_t modseq;
            if (!run_search(x, sp, matches, modseq)) {
                return false;
            }
            std::vector<Msg> snap;
            {
                std::lock_guard<std::mutex> g(view->hub->m);
                for (auto [s, u] : matches) {
                    const Msg* m = view->hub->find(u);
                    if (m) {
                        snap.push_back(*m);
                    }
                }
            }
            size_t k = 0;
            for (auto [s, u] : matches) {
                if (k >= snap.size() || snap[k].uid != u) {
                    continue;
                }
                Msg& m = snap[k++];
                keep.push_back(m.cache);
                items.push_back(SortItem{s, u, m.size, m.date, sort_info_of(u, m)});
            }
            return true;
        }

        void cmd_sort(Lexer& x, bool uid) {
            std::vector<SortKey> keys;
            if (!parse_sort_keys(x, keys) || !x.sp()) {
                bad("Invalid sort criteria");
                return;
            }
            std::vector<SortItem> items;
            std::vector<std::shared_ptr<MsgCache>> keep;
            if (!sort_items_of(x, items, keep)) {
                return;
            }
            sort_items(items, keys);
            out += "* SORT";
            for (const auto& it : items) {
                out += ' ';
                put_number(out, uid ? it.uid : it.seq);
            }
            out += "\r\n";
            updates(uid);
            ok("SORT completed");
        }

        void cmd_thread(Lexer& x, bool uid) {
            std::string_view alg = x.atom();
            const bool refs = iequal(alg, "REFERENCES");
            if (!refs && !iequal(alg, "ORDEREDSUBJECT")) {
                bad("Unknown threading algorithm");
                return;
            }
            if (!x.sp()) {
                bad("Invalid THREAD arguments");
                return;
            }
            std::vector<SortItem> items;
            std::vector<std::shared_ptr<MsgCache>> keep;
            if (!sort_items_of(x, items, keep)) {
                return;
            }
            auto roots = refs ? thread_references(items) : thread_ordered_subject(items);
            out += "* THREAD ";
            for (const auto& r : roots) {
                put_thread(out, *r, items, uid);
            }
            out += "\r\n";
            updates(uid);
            ok("THREAD completed");
        }

        // --- FETCH -------------------------------------------------------------

        struct FetchItem {
            enum class Kind : uint8_t { uid, flags, internal_date, size, envelope, body, body_structure, modseq, section, binary, binary_size, rfc822, rfc822_header, rfc822_text };
            Kind kind;
            Section section;
            bool peek = false;
            bool partial = false;
            uint64_t offset = 0, length = 0;
            std::string name;   // as the response names it
        };

        bool parse_fetch_item(Lexer& x, std::vector<FetchItem>& items) {
            using K = FetchItem::Kind;
            std::string_view w = x.run([](unsigned char c) { return atom_char(c) && c != '[' && c != '<'; });
            const std::string u = to_upper(w);
            auto add = [&](K k) {
                FetchItem it{};
                it.kind = k;
                items.push_back(it);
            };
            if (u == "ALL" || u == "FAST" || u == "FULL") {
                add(K::flags);
                add(K::internal_date);
                add(K::size);
                if (u != "FAST") {
                    add(K::envelope);
                }
                if (u == "FULL") {
                    add(K::body);
                }
                return true;
            }
            if (u == "UID") {
                add(K::uid);
            } else if (u == "FLAGS") {
                add(K::flags);
            } else if (u == "INTERNALDATE") {
                add(K::internal_date);
            } else if (u == "RFC822.SIZE") {
                add(K::size);
            } else if (u == "ENVELOPE") {
                add(K::envelope);
            } else if (u == "BODYSTRUCTURE") {
                add(K::body_structure);
            } else if (u == "MODSEQ") {
                add(K::modseq);
                condstore = true;
            } else if (u == "RFC822") {
                add(K::rfc822);
            } else if (u == "RFC822.HEADER") {
                add(K::rfc822_header);
            } else if (u == "RFC822.TEXT") {
                add(K::rfc822_text);
            } else if (u == "BODY" && x.peek() != '[') {
                add(K::body);
            } else if (u == "BODY" || u == "BODY.PEEK" || u == "BINARY" || u == "BINARY.PEEK" || u == "BINARY.SIZE") {
                FetchItem it{};
                it.kind = u == "BINARY.SIZE" ? K::binary_size : u[1] == 'I' ? K::binary : K::section;
                it.peek = u == "BODY.PEEK" || u == "BINARY.PEEK";
                std::string_view sec;
                if (!section_text(x, sec) || !parse_section(sec, it.section)) {
                    return false;
                }
                if (it.kind != K::section && it.section.text != Section::Text::all) {
                    return false;   // BINARY takes part numbers alone
                }
                if (x.eat('<')) {
                    if (it.kind == K::binary_size || !x.number(it.offset) || !x.eat('.') || !x.number(it.length) || it.length == 0 || !x.eat('>')) {
                        return false;
                    }
                    it.partial = true;
                }
                it.name = (it.kind == K::section ? "BODY[" : it.kind == K::binary ? "BINARY[" : "BINARY.SIZE[") + section_name(it.section) + "]";
                if (it.partial) {
                    it.name += '<';
                    put_number(it.name, it.offset);
                    it.name += '>';
                }
                items.push_back(std::move(it));
            } else {
                return false;
            }
            return true;
        }

        // ENVELOPE, BODY and BODYSTRUCTURE of a message, read once and kept
        bool structure_of(Msg& m, const string& content, int which, std::string& o) {
            const int k = utf8 ? 1 : 0;
            auto& cache = *m.cache;
            {
                std::lock_guard<std::mutex> g(cache.m);
                if (cache.parsed[k]) {
                    o += which == 0 ? cache.envelope[k] : which == 1 ? cache.body[k] : cache.structure[k];
                    return true;
                }
            }
            if (content.empty() && m.size) {
                return false;
            }
            Structure st(content.view());
            std::string env, body, bs;
            put_envelope(env, st.root, utf8);
            put_body(body, content.view(), st.root, false, utf8);
            put_body(bs, content.view(), st.root, true, utf8);
            std::lock_guard<std::mutex> g(cache.m);
            cache.envelope[k] = std::move(env);
            cache.body[k] = std::move(body);
            cache.structure[k] = std::move(bs);
            cache.parsed[k] = true;
            o += which == 0 ? cache.envelope[k] : which == 1 ? cache.body[k] : cache.structure[k];
            return true;
        }

        async::task<void> cmd_fetch(Lexer& x, bool uid) {
            using K = FetchItem::Kind;
            sequence_set set;
            if (!read_set(x, set) || !x.sp()) {
                bad("Invalid FETCH arguments");
                co_return;
            }
            std::vector<FetchItem> items;
            bool ok_items = true;
            if (x.eat('(')) {
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        ok_items = false;
                        break;
                    }
                    first = false;
                    if (!parse_fetch_item(x, items)) {
                        ok_items = false;
                        break;
                    }
                }
            } else {
                ok_items = parse_fetch_item(x, items);
            }
            if (!ok_items) {
                bad("Invalid FETCH items");
                co_return;
            }
            // (CHANGEDSINCE n [VANISHED])
            uint64_t changed_since = 0;
            bool has_changed_since = false, vanished = false;
            if (x.eat(' ')) {
                if (!x.eat('(')) {
                    bad("Invalid FETCH modifiers");
                    co_return;
                }
                bool first = true;
                while (!x.eat(')')) {
                    if (!first && !x.sp()) {
                        bad("Invalid FETCH modifiers");
                        co_return;
                    }
                    first = false;
                    if (x.word("CHANGEDSINCE")) {
                        if (!x.sp() || !x.number(changed_since)) {
                            bad("Invalid CHANGEDSINCE");
                            co_return;
                        }
                        has_changed_since = true;
                        condstore = true;
                    } else if (x.word("VANISHED")) {
                        vanished = true;
                    } else {
                        bad("Unknown FETCH modifier");
                        co_return;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid FETCH arguments");
                co_return;
            }
            if (vanished && (!uid || !qresync || !has_changed_since)) {
                bad("VANISHED needs UID FETCH, QRESYNC and CHANGEDSINCE");
                co_return;
            }
            if (has_changed_since) {
                bool has = false;
                for (auto& it : items) {
                    has |= it.kind == K::modseq;
                }
                if (!has) {
                    items.push_back(FetchItem{K::modseq, {}, false, false, 0, 0, {}});
                }
            }
            if (uid) {
                bool has = false;
                for (auto& it : items) {
                    has |= it.kind == K::uid;
                }
                if (!has) {
                    items.insert(items.begin(), FetchItem{K::uid, {}, false, false, 0, 0, {}});
                }
            }
            std::vector<std::pair<uint32_t, uint32_t>> msgs;
            if (!messages_of(set, uid, msgs)) {
                co_return;
            }
            bool needs_content = false, sets_seen = false, has_flags = false;
            for (const auto& it : items) {
                switch (it.kind) {
                    case K::envelope:
                    case K::body:
                    case K::body_structure:
                    case K::section:
                    case K::binary:
                    case K::binary_size:
                    case K::rfc822:
                    case K::rfc822_header:
                    case K::rfc822_text: needs_content = true; break;
                    default: break;
                }
                sets_seen |= ((it.kind == K::section || it.kind == K::binary) && !it.peek) || it.kind == K::rfc822 || it.kind == K::rfc822_text;
                has_flags |= it.kind == K::flags;
            }
            if (view->read_only) {
                sets_seen = false;
            }
            if (vanished) {
                std::vector<uint32_t> gone;
                {
                    std::lock_guard<std::mutex> g(view->hub->m);
                    const uint32_t top = view->hub->uid_next > 1 ? view->hub->uid_next - 1 : 0;
                    for (auto [a, b] : set.ranges()) {
                        uint32_t lo = a ? a : top, hi = b ? b : top;
                        if (lo > hi) {
                            std::swap(lo, hi);
                        }
                        vanished_between(*view->hub, lo, std::min(hi, top), gone);
                    }
                }
                if (!gone.empty()) {
                    out += "* VANISHED (EARLIER) ";
                    out += sequence_set(vector<uint32_t>(gone.begin(), gone.end())).to_string().view();
                    out += "\r\n";
                }
            }
            // \Seen set first, for the messages read whole or in part
            if (sets_seen) {
                std::vector<uint32_t> to_mark;
                {
                    std::lock_guard<std::mutex> g(view->hub->m);
                    for (auto [s, u] : msgs) {
                        const Msg* m = view->hub->find(u);
                        if (m && !(m->flags & FlagSeen)) {
                            to_mark.push_back(u);
                        }
                    }
                }
                if (!to_mark.empty()) {
                    store_flags(to_mark, FlagSeen, {}, 1, 0, false, nullptr);
                }
            }
            bool expunged = false;
            for (auto [seq, u] : msgs) {
                Msg m;
                {
                    std::lock_guard<std::mutex> g(view->hub->m);
                    const Msg* p = view->hub->find(u);
                    if (!p) {
                        expunged = true;
                        continue;
                    }
                    m = *p;
                }
                if (has_changed_since && m.modseq <= changed_since) {
                    continue;
                }
                string content;
                if (needs_content) {
                    auto c = content_of(u);
                    if (!c) {
                        expunged = true;
                        continue;
                    }
                    content = *c;
                }
                out += "* ";
                put_number(out, seq);
                out += " FETCH (";
                bool first = true;
                auto sep = [&] {
                    if (!first) {
                        out += ' ';
                    }
                    first = false;
                };
                const bool marked = sets_seen;
                for (const auto& it : items) {
                    sep();
                    switch (it.kind) {
                        case K::uid:
                            out += "UID ";
                            put_number(out, u);
                            break;
                        case K::flags:
                            out += "FLAGS ";
                            put_flags(out, m, view.get());
                            break;
                        case K::internal_date:
                            out += "INTERNALDATE \"";
                            out += format_date_time(m.date);
                            out += '"';
                            break;
                        case K::size:
                            out += "RFC822.SIZE ";
                            put_number(out, m.size);
                            break;
                        case K::modseq:
                            out += "MODSEQ (";
                            put_number(out, m.modseq);
                            out += ')';
                            break;
                        case K::envelope:
                            out += "ENVELOPE ";
                            structure_of(m, content, 0, out);
                            break;
                        case K::body:
                            out += "BODY ";
                            structure_of(m, content, 1, out);
                            break;
                        case K::body_structure:
                            out += "BODYSTRUCTURE ";
                            structure_of(m, content, 2, out);
                            break;
                        case K::rfc822:
                            out += "RFC822 ";
                            put_literal(out, content.view(), false);
                            break;
                        case K::rfc822_header:
                        case K::rfc822_text: {
                            Structure st(content.view());
                            Section sec;
                            sec.text = it.kind == K::rfc822_header ? Section::Text::header : Section::Text::text;
                            std::string bytes;
                            section_bytes(content.view(), st.root, sec, bytes);
                            out += it.kind == K::rfc822_header ? "RFC822.HEADER " : "RFC822.TEXT ";
                            put_literal(out, bytes, false);
                            break;
                        }
                        case K::section:
                        case K::binary:
                        case K::binary_size: {
                            std::string bytes;
                            bool found;
                            bool cte_ok = true;
                            if (it.section.path.empty() && it.section.text == Section::Text::all) {
                                if (it.kind == K::section) {
                                    bytes.assign(content.data(), content.size());
                                    found = true;
                                } else {
                                    Structure st(content.view());
                                    found = true;
                                    if (st.root.multipart()) {
                                        bytes.assign(content.data(), content.size());
                                    } else {
                                        cte_ok = decoded_content(content.view().substr(st.root.body_begin, st.root.body_end - st.root.body_begin), st.root.encoding, bytes);
                                    }
                                }
                            } else {
                                Structure st(content.view());
                                if (it.kind == K::section) {
                                    found = section_bytes(content.view(), st.root, it.section, bytes);
                                } else {
                                    const Part* p = resolve_part(st.root, it.section.path);
                                    found = p != nullptr;
                                    if (p) {
                                        cte_ok = decoded_content(content.view().substr(p->body_begin, p->body_end - p->body_begin), p->encoding, bytes);
                                    }
                                }
                            }
                            (void)found;
                            if (!cte_ok) {
                                out.clear();
                                no("Unknown Content-Transfer-Encoding", "UNKNOWN-CTE");
                                co_return;
                            }
                            if (it.kind == K::binary_size) {
                                out += it.name;
                                out += ' ';
                                put_number(out, bytes.size());
                                break;
                            }
                            std::string_view v = bytes;
                            if (it.partial) {
                                v = it.offset >= v.size() ? std::string_view() : v.substr(size_t(it.offset), size_t(std::min<uint64_t>(it.length, v.size() - it.offset)));
                            }
                            out += it.name;
                            out += ' ';
                            if (it.kind == K::binary && v.find('\0') != std::string_view::npos) {
                                put_literal(out, v, false, true);
                            } else {
                                put_literal(out, v, false);
                            }
                            break;
                        }
                    }
                }
                if (marked && !has_flags) {
                    sep();
                    out += "FLAGS ";
                    put_flags(out, m, view.get());
                }
                out += ")\r\n";
                if (out.size() > (size_t(256) << 10)) {
                    if (!co_await flush()) {
                        ended = true;
                        co_return;
                    }
                }
            }
            updates(uid);
            if (expunged) {
                no("Some messages were expunged", "EXPUNGEISSUE");
            } else {
                ok("FETCH completed");
            }
        }

        // --- STORE ---------------------------------------------------------------

        // Flags changed through the backend and the hub: mode 0 replaces, 1
        // adds, 2 removes; messages whose modseq is past unchanged_since
        // (when given) left alone, their UIDs in modified. The FETCH
        // responses of this session when answer.
        void store_flags(const std::vector<uint32_t>& uids, uint8_t sys, const std::vector<std::string>& keywords, int mode, uint64_t unchanged_since,
                         bool check_unchanged, std::vector<uint32_t>* modified, bool answer = false, bool silent = true, bool uid_cmd = false) {
            Hub& h = *view->hub;
            vector<function<void()>> wakes;
            std::vector<uint32_t> changed_list;
            {
                std::lock_guard<std::mutex> g(h.m);
                vector<flag_update> updates_list;
                std::vector<std::pair<uint8_t, std::vector<std::string>>> next;
                std::vector<uint32_t> touched;
                for (uint32_t u : uids) {
                    Msg* m = h.find(u);
                    if (!m) {
                        continue;
                    }
                    if (check_unchanged && m->modseq > unchanged_since) {
                        if (modified) {
                            modified->push_back(u);
                        }
                        continue;
                    }
                    uint8_t f = m->flags;
                    std::vector<std::string> kw = m->keywords;
                    if (mode == 0) {
                        f = sys;
                        kw.clear();
                        for (const auto& k : keywords) {
                            kw.push_back(k);
                        }
                    } else if (mode == 1) {
                        f |= sys;
                        for (const auto& k : keywords) {
                            bool has = false;
                            for (const auto& e : kw) {
                                has |= iequal(e, k);
                            }
                            if (!has) {
                                kw.push_back(k);
                            }
                        }
                    } else {
                        f &= uint8_t(~sys);
                        kw.erase(std::remove_if(kw.begin(), kw.end(),
                                                [&](const std::string& e) {
                                                    for (const auto& k : keywords) {
                                                        if (iequal(e, k)) {
                                                            return true;
                                                        }
                                                    }
                                                    return false;
                                                }),
                                 kw.end());
                    }
                    if (f == m->flags && kw == m->keywords) {
                        // no change: still answered (RFC 9051 §6.4.6)
                        if (answer && !silent) {
                            touched.push_back(u);
                        }
                        continue;
                    }
                    flag_update fu;
                    fu.uid = u;
                    fu.flags = flags_of(f, kw);
                    updates_list.push_back(fu);
                    next.emplace_back(f, std::move(kw));
                    changed_list.push_back(u);
                }
                if (!updates_list.empty()) {
                    auto r = [&] {
                    HubChange guard;
                    return cfg->backend->store(user, mailbox, updates_list);
                }();
                    if (!r) {
                        if (answer) {
                            no_of(r.error(), "STORE failed");
                        }
                        return;
                    }
                    for (size_t i = 0; i < changed_list.size(); ++i) {
                        Msg* m = h.find(changed_list[i]);
                        if (m) {
                            m->flags = next[i].first;
                            m->keywords = std::move(next[i].second);
                            m->modseq = *r;
                        }
                    }
                    if (*r > h.highest_modseq) {
                        h.highest_modseq = *r;
                    }
                    h.changed(changed_list, view.get(), wakes);
                    h.settle();
                }
                if (answer) {
                    std::vector<uint32_t> say = changed_list;
                    say.insert(say.end(), touched.begin(), touched.end());
                    std::sort(say.begin(), say.end());
                    for (uint32_t u : say) {
                        const uint32_t seq = view->seq_of(u);
                        const Msg* m = h.find(u);
                        if (!seq || !m) {
                            continue;
                        }
                        if (silent && !condstore) {
                            continue;
                        }
                        out += "* ";
                        put_number(out, seq);
                        out += " FETCH (";
                        bool first = true;
                        if (uid_cmd || qresync) {
                            out += "UID ";
                            put_number(out, u);
                            first = false;
                        }
                        if (!silent) {
                            if (!first) {
                                out += ' ';
                            }
                            out += "FLAGS ";
                            put_flags(out, *m, view.get());
                            first = false;
                        }
                        if (condstore) {
                            if (!first) {
                                out += ' ';
                            }
                            out += "MODSEQ (";
                            put_number(out, m->modseq);
                            out += ')';
                        }
                        out += ")\r\n";
                    }
                }
            }
            for (auto& w : wakes) {
                w();
            }
        }

        void cmd_store(Lexer& x, bool uid) {
            sequence_set set;
            if (!read_set(x, set) || !x.sp()) {
                bad("Invalid STORE arguments");
                return;
            }
            uint64_t unchanged_since = 0;
            bool check = false;
            if (x.peek() == '(') {
                x.eat('(');
                if (!x.word("UNCHANGEDSINCE") || !x.sp() || !x.number(unchanged_since) || !x.eat(')') || !x.sp()) {
                    bad("Invalid STORE modifiers");
                    return;
                }
                check = true;
                condstore = true;
            }
            std::string_view item = x.atom();
            std::string u = to_upper(item);
            int mode;
            if (!u.empty() && u[0] == '+') {
                mode = 1;
                u.erase(0, 1);
            } else if (!u.empty() && u[0] == '-') {
                mode = 2;
                u.erase(0, 1);
            } else {
                mode = 0;
            }
            bool silent = false;
            if (u == "FLAGS.SILENT") {
                silent = true;
            } else if (u != "FLAGS") {
                bad("Invalid STORE item");
                return;
            }
            if (!x.sp()) {
                bad("Invalid STORE arguments");
                return;
            }
            vector<string> flags;
            if (x.peek() == '(') {
                if (!parse_flag_list(x, flags)) {
                    bad("Invalid STORE flags");
                    return;
                }
            } else {
                while (!x.at_end()) {
                    std::string_view f;
                    if (!x.flag(f)) {
                        bad("Invalid STORE flags");
                        return;
                    }
                    flags.push_back(string(f));
                    if (!x.eat(' ')) {
                        break;
                    }
                }
            }
            if (!x.at_end()) {
                bad("Invalid STORE arguments");
                return;
            }
            uint8_t sys = 0;
            std::vector<std::string> kw;
            for (const auto& f : flags) {
                const uint8_t b = system_flag(f.view());
                if (b) {
                    sys |= b;
                } else if (iequal(f.view(), "\\Recent")) {
                    continue;   // the server's own, silently ignored
                } else if (f.view()[0] == '\\') {
                    bad("Invalid flag");
                    return;
                } else {
                    kw.emplace_back(f.view());
                }
            }
            if (view->read_only) {
                no("Mailbox is read-only", "READ-ONLY");
                return;
            }
            std::vector<std::pair<uint32_t, uint32_t>> msgs;
            if (!messages_of(set, uid, msgs)) {
                return;
            }
            std::vector<uint32_t> uids;
            for (auto [s, u2] : msgs) {
                uids.push_back(u2);
            }
            std::vector<uint32_t> modified;
            store_flags(uids, sys, kw, mode, unchanged_since, check, &modified, true, silent, uid);
            updates(uid);
            if (!modified.empty()) {
                std::vector<uint32_t> nums;
                for (uint32_t m : modified) {
                    nums.push_back(uid ? m : view->seq_of(m));
                }
                std::string code = "MODIFIED " + std::string(sequence_set(vector<uint32_t>(nums.begin(), nums.end())).to_string().view());
                ok("Conditional STORE failed", code.c_str());
            } else {
                ok("STORE completed");
            }
        }

        // --- COPY, MOVE --------------------------------------------------------

        void cmd_copy(Lexer& x, bool uid, bool move) {
            sequence_set set;
            std::string_view raw;
            if (!read_set(x, set) || !x.sp() || !x.astring(raw, scratch) || !x.at_end()) {
                bad(move ? "Invalid MOVE arguments" : "Invalid COPY arguments");
                return;
            }
            string to;
            if (!name_in(raw, to)) {
                no("Invalid mailbox name", "CANNOT");
                return;
            }
            if (move && view->read_only) {
                no("Mailbox is read-only", "READ-ONLY");
                return;
            }
            std::vector<std::pair<uint32_t, uint32_t>> msgs;
            if (!messages_of(set, uid, msgs)) {
                return;
            }
            std::vector<uint32_t> src;
            {
                std::lock_guard<std::mutex> g(view->hub->m);
                for (auto [s, u] : msgs) {
                    if (view->hub->find(u)) {
                        src.push_back(u);
                    }
                }
            }
            if (src.empty()) {
                updates(uid);
                ok(move ? "No messages moved" : "No messages copied");
                return;
            }
            auto r = [&] {
                    HubChange guard;
                    return cfg->backend->copy(user, mailbox, vector<uint32_t>(src.begin(), src.end()), to);
                }();
            if (!r) {
                if (r.error().code() == make_error_code(errc::nonexistent)) {
                    no("No such mailbox", "TRYCREATE");
                } else {
                    no_of(r.error(), move ? "MOVE failed" : "COPY failed");
                }
                return;
            }
            uint32_t validity = 0;
            tracked_ptr<Hub> dst = to == mailbox ? view->hub : srv->find_hub(user, to);
            vector<function<void()>> wakes;
            if (dst) {
                std::lock_guard<std::mutex> g(dst->m);
                for (const auto& s : *r) {
                    dst->added(s, wakes);
                }
                dst->settle();
                validity = dst->uid_validity;
            } else {
                auto v = cfg->backend->uid_validity(user, to);
                if (v) {
                    validity = *v;
                }
            }
            for (auto& w : wakes) {
                w();
            }
            std::vector<uint32_t> from_uids, to_uids;
            for (size_t i = 0; i < r->size() && i < src.size(); ++i) {
                from_uids.push_back(src[i]);
                to_uids.push_back((*r)[i].uid);
            }
            std::string code;
            if (validity && !from_uids.empty()) {
                code = "COPYUID ";
                put_number(code, validity);
                code += ' ';
                // in pairs, in order (RFC 4315): written number by number
                // when they do not run alike
                code += _pairs(from_uids);
                code += ' ';
                code += _pairs(to_uids);
            }
            if (move) {
                if (!code.empty()) {
                    out += "* OK [" + code + "] Moved\r\n";
                }
                if (!expunge_uids(src, true)) {
                    return;
                }
                updates(true);
                ok("MOVE completed");
                return;
            }
            updates(uid);
            if (code.empty()) {
                ok("COPY completed");
            } else {
                ok("COPY completed", code.c_str());
            }
        }

        // A list of UIDs in its order, runs of consecutive ones as ranges
        static std::string _pairs(const std::vector<uint32_t>& v) {
            std::string out;
            size_t i = 0;
            while (i < v.size()) {
                size_t j = i;
                while (j + 1 < v.size() && v[j + 1] == v[j] + 1) {
                    ++j;
                }
                if (!out.empty()) {
                    out += ',';
                }
                put_number(out, v[i]);
                if (j > i) {
                    out += ':';
                    put_number(out, v[j]);
                }
                i = j + 1;
            }
            return out;
        }
    };

    // A connection served to its end
    inline async::task<void> serve_connection(tracked_ptr<ServerImpl> s, tracked_ptr<ServerSettings> cfg, net::connection c, bool tls) noexcept {
        tracked_ptr session = make_tracked<Session>(s, cfg, c, tls);
        session->me = session;
        session->entry = make_tracked<ConnEntry>();
        session->entry->c = c;
        {
            std::lock_guard<std::mutex> g(s->lock);
            session->conn_id = ++s->next_id;
            s->connections.emplace(session->conn_id, session->entry);
        }
        const size_t active = s->active.fetch_add(1) + 1;
        if (cfg->max_connections && active > cfg->max_connections) {
            session->out = "* BYE [UNAVAILABLE] Too many connections\r\n";
            (void)co_await session->flush();
        } else {
            co_await Session::run(session);
        }
        session->leave();
        (void)co_await session->conn.async_close();
        {
            std::lock_guard<std::mutex> g(s->lock);
            s->connections.erase(session->conn_id);
        }
        session->me = tracked_ptr<Session>();
        s->active.fetch_sub(1);
        s->running.done();
    }
}
