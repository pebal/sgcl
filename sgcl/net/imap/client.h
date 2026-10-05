//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "criteria.h"
#include "error.h"
#include "types.h"
#include "detail/deflate_conn.h"
#include "detail/io.h"
#include "detail/response.h"
#include "detail/syntax.h"
#include "detail/words.h"
#include "../connection.h"
#include "../error.h"
#include "../socket.h"
#include "../tls.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../async/mutex.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../async/timer.h"
#include "../../core/aliases.h"
#include "../../core/clock.h"
#include "../../core/detail/handle_word.h"
#include "../../core/duration.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../time/datetime.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::net::imap {
    // How the connection is protected: TLS from the first byte (imaps://,
    // port 993, RFC 8314), STARTTLS required before the credentials go,
    // or none (a test's loopback); automatic is TLS for imaps:// and port
    // 993, STARTTLS for the rest
    enum class security : uint8_t { automatic, tls, starttls, none };

    // The SASL mechanism of the login; automatic chooses by what the
    // options hold and the server offers: OAUTHBEARER, then XOAUTH2, for
    // a token; AUTHENTICATE PLAIN, then the LOGIN command, then
    // AUTHENTICATE LOGIN, for a password
    enum class mechanism : uint8_t { automatic, plain, login, xoauth2, oauthbearer, login_command };

    // How STORE changes the flags
    enum class store_mode : uint8_t { replace, add, remove };

    // What SORT orders by (RFC 5256 §3, RFC 5957's DISPLAY*)
    enum class order : uint8_t { arrival, cc, date, from, size, subject, to, display_from, display_to };

    // The algorithm of THREAD (RFC 5256)
    enum class threading : uint8_t { references, ordered_subject };

    class client;

    // What fetch() asks for, beside the UID it always does
    struct fetch_options {
        bool flags = true;
        bool envelope = false;
        bool body_structure = false;
        bool size = false;                  // RFC822.SIZE
        bool internal_date = false;
        bool modseq = false;                // CONDSTORE
        vector<string> sections;            // BODY.PEEK[section] each: "" the whole message, "HEADER", "TEXT", "1.2", "HEADER.FIELDS (FROM TO)"
        bool binary = false;                // the sections as BINARY.PEEK (RFC 3516): decoded from their transfer encoding by the server
        bool mark_seen = false;             // BODY[...] instead of BODY.PEEK: the messages marked \Seen
        uint64_t partial_offset = 0;        // <offset.length> of every section
        uint64_t partial_length = 0;        // zero: the whole section
        uint64_t changed_since = 0;         // CHANGEDSINCE (CONDSTORE): only the messages changed past this mod-sequence
    };

    // What list() asks for
    struct list_options {
        string reference;                   // the reference name the pattern is taken under
        bool subscribed = false;            // the subscribed mailboxes only (LIST (SUBSCRIBED), LSUB without LIST-EXTENDED)
        bool special_use = false;           // the special-use mailboxes only (RFC 6154)
        bool status = false;                // each mailbox's status with it (LIST-STATUS, RFC 5819)
    };

    // What select() asks for: EXAMINE, and QRESYNC's state known from an
    // earlier session (RFC 7162 §3.2.5)
    struct select_options {
        bool read_only = false;
        uint32_t uid_validity = 0;          // with modseq: QRESYNC
        uint64_t modseq = 0;
        sequence_set known_uids;            // the UIDs known; empty: every one below UIDNEXT
    };

    namespace detail {
        // A command as it goes out: its bytes, and the offsets after each
        // synchronizing literal's head where the writing waits for "+"
        struct Command {
            std::string text;
            std::vector<size_t> waits;
            bool plus = false;
            bool minus = false;
            bool utf8 = false;

            void raw(std::string_view s) {
                text.append(s.data(), s.size());
            }

            void sp() {
                text += ' ';
            }

            void number(uint64_t n) {
                put_number(text, n);
            }

            void literal(std::string_view s, bool eight = false) {
                const bool nonsync = plus || (minus && s.size() <= 4096);
                put_literal(text, s, nonsync, eight);
                if (!nonsync) {
                    waits.push_back(text.size() - s.size());
                }
            }

            void astring(std::string_view s) {
                switch (form_of(s, utf8, true)) {
                    case StringForm::atom: raw(s); break;
                    case StringForm::quoted: put_quoted(text, s); break;
                    case StringForm::literal: literal(s); break;
                }
            }

            void string(std::string_view s) {
                if (form_of(s, utf8, false) == StringForm::literal) {
                    literal(s);
                } else {
                    put_quoted(text, s);
                }
            }

            void mailbox(std::string_view name) {
                if (utf8) {
                    astring(name);
                } else {
                    astring(utf7_encode(name));
                }
            }

            // The criteria's text, its string arguments written as this
            // connection allows them
            void criteria(std::string_view t) {
                if (t.empty()) {
                    raw("ALL");
                    return;
                }
                size_t i = 0;
                while (i < t.size()) {
                    if (t[i] == '\0') {
                        const size_t end = t.find('\0', i + 1);
                        string(t.substr(i + 1, end - i - 1));
                        i = end + 1;
                    } else {
                        const size_t next = t.find('\0', i);
                        raw(t.substr(i, next == std::string_view::npos ? std::string_view::npos : next - i));
                        i = next == std::string_view::npos ? t.size() : next;
                    }
                }
            }

            void end() {
                text += "\r\n";
            }
        };

        // A command's answer: its tagged response, and the untagged ones
        // that came while it ran
        struct Done {
            Status status = Status::none;
            std::string code;
            std::string code_data;
            std::string text;
            std::vector<std::string> untagged;
        };

        struct Pending {
            std::string tag;
            bool done = false;
            Done result;
        };

        struct ClientImpl {
            net::connection conn;
            Reader in{size_t(64) << 10};
            std::vector<std::string> caps;
            std::string host;
            bool utf8 = false;
            bool rev2 = false;
            bool condstore = false;
            bool qresync = false;
            bool tls = false;
            bool compressed = false;
            bool preauth = false;
            std::atomic<uint32_t> tags{0};
            async::mutex write_lock;
            async::mutex read_lock;
            std::mutex state_lock;
            std::vector<std::shared_ptr<Pending>> pending;
            // the selected mailbox, as the untagged responses keep it
            selected mailbox;
            bool has_mailbox = false;
            // what the server pushed: to the callback, and to an IDLE's list
            function<void(const update&)> on_update;
            vector<update> idle_updates;
            bool idling = false;
            std::atomic<bool> closed{false};
            size_t continuations = 0;       // "+" read and not yet taken by the writer waiting for one
            std::string bye;
            vector<pair<string, string>> server_id;
            duration timeout = std::chrono::seconds(30);
            size_t max_literal = size_t(256) << 20;
            std::string scratch;

            bool has(std::string_view cap) const noexcept {
                for (const auto& c : caps) {
                    if (iequal(c, cap)) {
                        return true;
                    }
                }
                return false;
            }

            bool has_prefix(std::string_view prefix) const noexcept {
                for (const auto& c : caps) {
                    if (istarts(c, prefix)) {
                        return true;
                    }
                }
                return false;
            }

            Command command(std::string_view name) {
                Command c;
                c.plus = has("LITERAL+");
                c.minus = has("LITERAL-") || has("IMAP4REV2") || rev2;
                c.utf8 = utf8;
                const uint32_t n = tags.fetch_add(1) + 1;
                c.text = "A";
                put_number(c.text, n);
                c.text += ' ';
                c.text.append(name.data(), name.size());
                return c;
            }

            static std::string_view tag_of(const Command& c) noexcept {
                return std::string_view(c.text).substr(0, c.text.find(' '));
            }
        };

        inline io::error closed_error(const ClientImpl& c, const char* op) noexcept {
            if (!c.bye.empty()) {
                return imap_error(errc::bye, op, string(c.bye));
            }
            return io::error(io::errc::closed, op, string(c.host));
        }

        // An untagged response taken in: the state of the selected mailbox,
        // the capabilities, the server's goodbye; an update for the
        // program when it is one
        inline void take_untagged(ClientImpl& c, std::string_view raw, const Response& r) {
            optional<update> u;
            {
                std::lock_guard<std::mutex> g(c.state_lock);
                if (r.status != Status::none) {
                    if (r.status == Status::bye) {
                        c.bye = std::string(r.text.empty() ? std::string_view("BYE") : r.text);
                        update x;
                        x.kind = update::kind::bye;
                        x.text = str(r.text);
                        u = x;
                    } else if (iequal(r.code, "ALERT")) {
                        update x;
                        x.kind = update::kind::alert;
                        x.text = str(r.text);
                        u = x;
                    } else if (iequal(r.code, "CAPABILITY")) {
                        parse_capabilities(r.code_data, c.caps);
                    } else if (c.has_mailbox) {
                        Lexer x(r.code_data);
                        if (iequal(r.code, "UIDNEXT")) {
                            x.number32(c.mailbox.uid_next);
                        } else if (iequal(r.code, "UIDVALIDITY")) {
                            x.number32(c.mailbox.uid_validity);
                        } else if (iequal(r.code, "HIGHESTMODSEQ")) {
                            x.number(c.mailbox.highest_modseq);
                        } else if (iequal(r.code, "UNSEEN")) {
                            x.number32(c.mailbox.first_unseen);
                        } else if (iequal(r.code, "PERMANENTFLAGS")) {
                            vector<string> f;
                            if (parse_flag_list(x, f)) {
                                c.mailbox.permanent_flags = f;
                            }
                        } else if (iequal(r.code, "NOMODSEQ")) {
                            c.mailbox.highest_modseq = 0;
                        }
                    }
                } else if (r.numbered) {
                    if (iequal(r.name, "EXISTS")) {
                        c.mailbox.exists = r.number;
                        update x;
                        x.kind = update::kind::exists;
                        x.number = r.number;
                        u = x;
                    } else if (iequal(r.name, "RECENT")) {
                        c.mailbox.recent = r.number;
                        update x;
                        x.kind = update::kind::recent;
                        x.number = r.number;
                        u = x;
                    } else if (iequal(r.name, "EXPUNGE")) {
                        if (c.mailbox.exists) {
                            --c.mailbox.exists;
                        }
                        update x;
                        x.kind = update::kind::expunge;
                        x.number = r.number;
                        u = x;
                    } else if (iequal(r.name, "FETCH") && (c.on_update || c.idling)) {
                        // read as an update only for someone listening (a
                        // command's own FETCH is read by the command)
                        Lexer x(raw.substr(r.data_at));
                        message m;
                        std::string scratch;
                        if (parse_fetch(x, m, scratch)) {
                            if (m.modseq > c.mailbox.highest_modseq && c.mailbox.highest_modseq) {
                                c.mailbox.highest_modseq = m.modseq;
                            }
                            update y;
                            y.kind = update::kind::fetch;
                            y.number = r.number;
                            y.uid = m.uid;
                            y.flags = m.flags;
                            y.modseq = m.modseq;
                            u = y;
                        }
                    }
                } else if (iequal(r.name, "CAPABILITY")) {
                    parse_capabilities(raw.substr(r.data_at), c.caps);
                } else if (iequal(r.name, "FLAGS")) {
                    Lexer x(raw.substr(r.data_at));
                    vector<string> f;
                    if (parse_flag_list(x, f)) {
                        c.mailbox.flags = f;
                        update y;
                        y.kind = update::kind::flags;
                        y.flags = f;
                        u = y;
                    }
                } else if (iequal(r.name, "VANISHED")) {
                    Lexer x(raw.substr(r.data_at));
                    bool earlier;
                    sequence_set s;
                    if (parse_vanished(x, earlier, s) && !earlier) {
                        uint32_t n = 0;
                        for (auto [a, b] : s.ranges()) {
                            n += (a > b ? a - b : b - a) + 1;
                        }
                        c.mailbox.exists = c.mailbox.exists > n ? c.mailbox.exists - n : 0;
                        update y;
                        y.kind = update::kind::vanished;
                        y.uids = s;
                        u = y;
                    }
                } else if (iequal(r.name, "ENABLED")) {
                    std::vector<std::string> list;
                    parse_capabilities(raw.substr(r.data_at), list);
                    for (const auto& e : list) {
                        if (e == "IMAP4REV2") {
                            c.rev2 = c.utf8 = true;
                        } else if (e == "UTF8=ACCEPT") {
                            c.utf8 = true;
                        } else if (e == "QRESYNC") {
                            c.qresync = c.condstore = true;
                        } else if (e == "CONDSTORE") {
                            c.condstore = true;
                        }
                    }
                }
                if (u && c.idling) {
                    c.idle_updates.push_back(*u);
                }
            }
            if (u && c.on_update) {
                c.on_update(*u);
            }
        }

        // One response whole: its lines and literals, the last CRLF off;
        // false at the end of the stream
        inline async::task<expected<bool, io::error>> read_response(ClientImpl& c, std::string& out) noexcept {
            out.clear();
            for (;;) {
                const size_t start = out.size();
                auto r = co_await c.in.read_line(out, out.size() + (size_t(1) << 20));
                if (!r) {
                    co_return unexpected(r.error());
                }
                if (*r == LineEnd::eof) {
                    co_return false;
                }
                if (*r == LineEnd::too_long) {
                    co_return unexpected(imap_error(errc::malformed_response, "read", string("a response line past 1 MB")));
                }
                LiteralHead h = literal_at_end(std::string_view(out).substr(start));
                if (!h.found) {
                    co_return true;
                }
                if (h.overflow || h.size > c.max_literal) {
                    co_return unexpected(imap_error(errc::too_big, "read", string("a literal past the client's limit")));
                }
                out += "\r\n";
                auto lit = co_await c.in.read_exact(out, size_t(h.size));
                if (!lit) {
                    co_return unexpected(lit.error());
                }
                if (!*lit) {
                    co_return false;
                }
            }
        }

        // A response read and taken: a tagged one completes its command
        // (this one's or another's), an untagged one goes to every command
        // in flight and to the state; a continuation is returned to the
        // caller (a command writing a literal, AUTHENTICATE, IDLE)
        enum class Read : uint8_t { tagged, untagged, continuation };

        // A response whole in buf taken in: the tagged answer of its command,
        // an untagged one to the commands in flight and the state, a
        // continuation counted for the writer waiting for one
        inline expected<Read, io::error> take(ClientImpl& c, std::string& buf, std::string& continuation) {
            Response r;
            if (!parse_response(buf, r)) {
                return unexpected(imap_error(errc::malformed_response, "read", str(std::string_view(buf).substr(0, 200))));
            }
            if (r.kind == Response::Kind::continuation) {
                continuation = std::string(r.text);
                std::lock_guard<std::mutex> g(c.state_lock);
                ++c.continuations;
                return Read::continuation;
            }
            if (r.kind == Response::Kind::tagged) {
                std::lock_guard<std::mutex> g(c.state_lock);
                for (auto& p : c.pending) {
                    if (p->tag == r.tag && !p->done) {
                        p->done = true;
                        p->result.status = r.status;
                        p->result.code = std::string(r.code);
                        p->result.code_data = std::string(r.code_data);
                        p->result.text = std::string(r.text);
                        if (iequal(r.code, "CAPABILITY")) {
                            parse_capabilities(r.code_data, c.caps);
                        }
                        break;
                    }
                }
                return Read::tagged;
            }
            take_untagged(c, buf, r);
            std::lock_guard<std::mutex> g(c.state_lock);
            Pending* only = nullptr;
            size_t waiting = 0;
            for (auto& p : c.pending) {
                if (!p->done) {
                    only = p.get();
                    ++waiting;
                }
            }
            if (waiting == 1) {
                only->result.untagged.push_back(std::move(buf));   // the one command in flight takes it whole
                buf = std::string();
            } else {
                for (auto& p : c.pending) {
                    if (!p->done) {
                        p->result.untagged.push_back(buf);
                    }
                }
            }
            return Read::untagged;
        }

        // A response read and taken (see take)
        inline async::task<expected<Read, io::error>> read_one(ClientImpl& c, std::string& buf, std::string& continuation) noexcept {
            auto got = co_await read_response(c, buf);
            if (!got) {
                c.closed.store(true);
                co_return unexpected(got.error());
            }
            if (!*got) {
                c.closed.store(true);
                co_return unexpected(closed_error(c, "read"));
            }
            co_return take(c, buf, continuation);
        }

        // The next response when the buffer holds it whole, taken without a
        // wait (and without a frame of a coroutine: a FETCH of a thousand
        // messages is a thousand responses); nullopt when it must be read
        inline optional<expected<Read, io::error>> take_buffered(ClientImpl& c, std::string& buf, std::string& continuation) {
            const size_t n = c.in.complete_response(c.max_literal);
            if (!n) {
                return nullopt;
            }
            c.in.take_response(buf, n);
            return take(c, buf, continuation);
        }

        inline void deadline(ClientImpl& c) noexcept {
            c.conn.set_read_deadline(c.timeout > duration::zero() ? sgcl::clock::now() + c.timeout : time_point());
        }

        // A command run: written (waiting for "+" before each synchronizing
        // literal), its responses read until its tagged answer; a NO or BAD
        // is the error of its code, op naming the command
        inline async::task<expected<Done, io::error>> run(tracked_ptr<ClientImpl> self, Command cmd, string op, bool tolerate_no = false) noexcept {
            ClientImpl& c = *self;
            if (c.closed.load()) {
                co_return unexpected(closed_error(c, op.c_str()));
            }
            cmd.end();
            auto p = std::make_shared<Pending>();
            p->tag = std::string(ClientImpl::tag_of(cmd));
            {
                std::lock_guard<std::mutex> g(c.state_lock);
                c.pending.push_back(p);
            }
            auto forget = [&] {
                std::lock_guard<std::mutex> g(c.state_lock);
                c.pending.erase(std::remove(c.pending.begin(), c.pending.end(), p), c.pending.end());
            };
            std::string buf, cont;
            {
                auto wg = co_await c.write_lock.scoped_lock();
                size_t at = 0;
                for (size_t w : cmd.waits) {
                    std::string part = cmd.text.substr(at, w - at);
                    auto wr = co_await write_all(c.conn, part);
                    if (!wr) {
                        forget();
                        c.closed.store(true);
                        co_return unexpected(wr.error());
                    }
                    at = w;
                    // the server's "+" (or its refusal), read by this task or
                    // by another reading for its own command meanwhile
                    auto take = [&] {
                        std::lock_guard<std::mutex> g(c.state_lock);
                        if (c.continuations) {
                            --c.continuations;
                            return true;
                        }
                        return p->done;
                    };
                    if (!take()) {
                        auto rg = co_await c.read_lock.scoped_lock();
                        deadline(c);
                        while (!take()) {
                            auto one = co_await read_one(c, buf, cont);
                            if (!one) {
                                forget();
                                co_return unexpected(one.error());
                            }
                        }
                    }
                    if (p->done) {
                        break;
                    }
                }
                if (!p->done) {
                    std::string part = cmd.text.substr(at);
                    auto wr = co_await write_all(c.conn, part);
                    if (!wr) {
                        forget();
                        c.closed.store(true);
                        co_return unexpected(wr.error());
                    }
                }
            }
            for (;;) {
                {
                    std::lock_guard<std::mutex> g(c.state_lock);
                    if (p->done) {
                        break;
                    }
                }
                auto rg = co_await c.read_lock.scoped_lock();
                deadline(c);
                while (!p->done) {
                    if (auto fast = take_buffered(c, buf, cont)) {
                        if (!*fast) {
                            forget();
                            co_return unexpected(fast->error());
                        }
                        continue;
                    }
                    auto one = co_await read_one(c, buf, cont);
                    if (!one) {
                        forget();
                        co_return unexpected(one.error());
                    }
                }
            }
            forget();
            Done d = std::move(p->result);
            if (d.status == Status::ok || (tolerate_no && d.status == Status::no)) {
                co_return d;
            }
            const errc e = code_of(d.code, d.status == Status::bad);
            std::string what = d.code.empty() ? d.text : "[" + d.code + (d.code_data.empty() ? "" : " " + d.code_data) + "] " + d.text;
            co_return unexpected(imap_error(e, op, str(what)));
        }

        // The untagged responses of a kind among those a command collected,
        // with their data
        template<class F>
        inline bool each_untagged(const Done& d, std::string_view name, F f) {
            for (const auto& raw : d.untagged) {
                Response r;
                if (!parse_response(raw, r) || r.status != Status::none || !iequal(r.name, name)) {
                    continue;
                }
                Lexer x(std::string_view(raw).substr(r.data_at));
                x.set_lenient(true);
                if (!f(x, r)) {
                    return false;
                }
            }
            return true;
        }

        inline io::error malformed(const char* op) noexcept {
            return imap_error(errc::malformed_response, op);
        }

        inline std::string sasl_base64(std::string_view s) {
            std::string out;
            encode_base64(s, out);
            return out;
        }
    }

    // An IMAP client connection (RFC 9051, IMAP4rev2, and IMAP4rev1
    // servers): connected and logged in by connect(), then the commands as
    // methods. A handle of one word: copies are the same connection, safe
    // from many tasks and threads (independent commands from several tasks
    // go out pipelined, RFC 9051 §5.5). Messages are named by their UIDs.
    class client {
    public:
        // How the connection is made and the user logged in
        struct options {
            string user;                                    // the login; from the URL's user name when it has one
            string password;                                // LOGIN or AUTHENTICATE PLAIN; from the URL's password
            string token;                                   // an OAuth 2.0 access token: AUTHENTICATE OAUTHBEARER (RFC 7628) or XOAUTH2
            imap::mechanism mechanism = imap::mechanism::automatic;
            imap::security security = imap::security::automatic;
            net::tls::config tls;                           // imaps:// and STARTTLS; the server name the address's host when none is set
            duration timeout = std::chrono::seconds(30);    // the connect and the login together; then each command's wait for its answer; zero: none
            async::stop_token stop;                         // the connect cancelled
            // How the connection is made; tcp::connect by default
            function<async::task<expected<net::connection, io::error>>(const string&, async::stop_token)> dial;
            bool compress = false;                          // COMPRESS=DEFLATE (RFC 4978) when the server offers it
            vector<pair<string, string>> id;                // sent by ID (RFC 2971) when not empty: {"name", "my-app"}
            function<void(const imap::update&)> on_update;  // what the server pushes (EXISTS, EXPUNGE, FETCH, VANISHED, ALERT, BYE), on the task that read it
            size_t max_literal_bytes = size_t(256) << 20;   // the largest literal taken from the server
        };

        client() noexcept = default;

        // A connection to "host:port" (143 when none is given; 993 with
        // security::tls), or an imap:// or imaps:// URL whose user name and
        // password log in and whose path names a mailbox to select; logged
        // in when credentials are given
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

        // The same over a connection there is (a test's pipe in memory, one
        // through a proxy); TLS (security::tls) or STARTTLS over it as the
        // options say. The transport is closed when it fails
        static expected<client, io::error> connect(const net::connection& transport, const options& o) {
            return async_connect(transport, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept {
            string address = transport.remote_endpoint().to_string();
            return _co_connect(std::move(address), std::move(transport), std::move(o));
        }

        // --- the connection ---

        // The server's capabilities, upper-cased ("IMAP4REV2", "IDLE",
        // "AUTH=PLAIN")
        vector<string> capabilities() const {
            std::lock_guard<std::mutex> g(_c->state_lock);
            vector<string> out;
            for (const auto& c : _c->caps) {
                out.push_back(string(c));
            }
            return out;
        }

        // Whether the server has a capability, in any case
        bool has(const string& capability) const {
            std::lock_guard<std::mutex> g(_c->state_lock);
            return _c->has(capability.view());
        }

        // The server's answer to ID (RFC 2971), when options::id asked
        vector<pair<string, string>> server_id() const {
            std::lock_guard<std::mutex> g(_c->state_lock);
            return _c->server_id;
        }

        // Whether the connection is over TLS
        SGCL_INLINE_HOT bool is_tls() const noexcept {
            return _c->tls;
        }

        // Whether COMPRESS=DEFLATE is on
        SGCL_INLINE_HOT bool is_compressed() const noexcept {
            return _c->compressed;
        }

        // NOOP: the server's pending updates read (EXISTS, EXPUNGE, FETCH)
        // `noop(...)` on this thread, `co_await async_noop(...)` in a task
        expected<void, io::error> noop() const {
            return async_noop().wait();
        }

        async::task<expected<void, io::error>> async_noop() const noexcept {
            return _co_simple(_c, string("NOOP"));
        }

        // LOGOUT, then the connection closed
        // `logout(...)` on this thread, `co_await async_logout(...)` in a task
        expected<void, io::error> logout() const {
            return async_logout().wait();
        }

        async::task<expected<void, io::error>> async_logout() const noexcept {
            return _co_logout(_c);
        }

        // The connection closed now, without LOGOUT; the commands in
        // progress end with io::errc::closed
        expected<void, io::error> close() const noexcept {
            _c->closed.store(true);
            return _c->conn.close();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _c->closed.load() || _c->conn.is_closed();
        }

        // ENABLE (RFC 5161): the capabilities the server enabled of those
        // asked ("CONDSTORE", "QRESYNC", "UTF8=ACCEPT"); the client turns on
        // IMAP4rev2, UTF8=ACCEPT and QRESYNC by itself when offered
        // `enable(...)` on this thread, `co_await async_enable(...)` in a task
        expected<vector<string>, io::error> enable(const vector<string>& capabilities) const {
            return async_enable(capabilities).wait();
        }

        async::task<expected<vector<string>, io::error>> async_enable(vector<string> capabilities) const noexcept {
            return _co_enable(_c, std::move(capabilities));
        }

        // --- mailboxes ---

        // The mailboxes whose names match the pattern ("*" every one, "%"
        // one level: "INBOX/%"), names in UTF-8, INBOX first
        // `list(...)` on this thread, `co_await async_list(...)` in a task
        expected<vector<list_entry>, io::error> list(const string& pattern = string("*")) const {
            return async_list(pattern, list_options()).wait();
        }

        expected<vector<list_entry>, io::error> list(const string& pattern, const list_options& o) const {
            return async_list(pattern, o).wait();
        }

        async::task<expected<vector<list_entry>, io::error>> async_list(string pattern = string("*")) const noexcept {
            return _co_list(_c, std::move(pattern), list_options());
        }

        async::task<expected<vector<list_entry>, io::error>> async_list(string pattern, list_options o) const noexcept {
            return _co_list(_c, std::move(pattern), std::move(o));
        }

        // STATUS of a mailbox: every item the server has (MESSAGES, UIDNEXT,
        // UIDVALIDITY, UNSEEN, DELETED and SIZE, HIGHESTMODSEQ)
        // `status(...)` on this thread, `co_await async_status(...)` in a task
        expected<imap::status, io::error> status(const string& mailbox) const {
            return async_status(mailbox).wait();
        }

        async::task<expected<imap::status, io::error>> async_status(string mailbox) const noexcept {
            return _co_status(_c, std::move(mailbox));
        }

        // CREATE, with a special use (imap::special_use::sent ...,
        // CREATE-SPECIAL-USE) when one is given
        // `create(...)` on this thread, `co_await async_create(...)` in a task
        expected<void, io::error> create(const string& mailbox, const string& use = string()) const {
            return async_create(mailbox, use).wait();
        }

        async::task<expected<void, io::error>> async_create(string mailbox, string use = string()) const noexcept {
            return _co_mailbox_command(_c, string("CREATE"), std::move(mailbox), std::move(use));
        }

        // DELETE
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(const string& mailbox) const {
            return async_remove(mailbox).wait();
        }

        async::task<expected<void, io::error>> async_remove(string mailbox) const noexcept {
            return _co_mailbox_command(_c, string("DELETE"), std::move(mailbox), string());
        }

        // RENAME (INBOX's messages moved into the new name, INBOX staying)
        // `rename(...)` on this thread, `co_await async_rename(...)` in a task
        expected<void, io::error> rename(const string& from, const string& to) const {
            return async_rename(from, to).wait();
        }

        async::task<expected<void, io::error>> async_rename(string from, string to) const noexcept {
            return _co_rename(_c, std::move(from), std::move(to));
        }

        // SUBSCRIBE, UNSUBSCRIBE
        // `subscribe(...)` on this thread, `co_await async_subscribe(...)` in a task
        expected<void, io::error> subscribe(const string& mailbox) const {
            return async_subscribe(mailbox).wait();
        }

        async::task<expected<void, io::error>> async_subscribe(string mailbox) const noexcept {
            return _co_mailbox_command(_c, string("SUBSCRIBE"), std::move(mailbox), string());
        }

        // `unsubscribe(...)` on this thread, `co_await async_unsubscribe(...)` in a task
        expected<void, io::error> unsubscribe(const string& mailbox) const {
            return async_unsubscribe(mailbox).wait();
        }

        async::task<expected<void, io::error>> async_unsubscribe(string mailbox) const noexcept {
            return _co_mailbox_command(_c, string("UNSUBSCRIBE"), std::move(mailbox), string());
        }

        // SELECT (EXAMINE when read_only): the mailbox opened for the
        // commands on messages; with QRESYNC's state, the UIDs expunged and
        // the messages changed since
        // `select(...)` on this thread, `co_await async_select(...)` in a task
        expected<selected, io::error> select(const string& mailbox) const {
            return async_select(mailbox, select_options()).wait();
        }

        expected<selected, io::error> select(const string& mailbox, const select_options& o) const {
            return async_select(mailbox, o).wait();
        }

        async::task<expected<selected, io::error>> async_select(string mailbox) const noexcept {
            return _co_select(_c, std::move(mailbox), select_options());
        }

        async::task<expected<selected, io::error>> async_select(string mailbox, select_options o) const noexcept {
            return _co_select(_c, std::move(mailbox), std::move(o));
        }

        // EXAMINE: select read only
        // `examine(...)` on this thread, `co_await async_examine(...)` in a task
        expected<selected, io::error> examine(const string& mailbox) const {
            select_options o;
            o.read_only = true;
            return async_select(mailbox, o).wait();
        }

        async::task<expected<selected, io::error>> async_examine(string mailbox) const noexcept {
            select_options o;
            o.read_only = true;
            return _co_select(_c, std::move(mailbox), std::move(o));
        }

        // UNSELECT (RFC 3691): the mailbox closed, nothing expunged; CLOSE:
        // the messages marked \Deleted expunged first
        // `unselect(...)` on this thread, `co_await async_unselect(...)` in a task
        expected<void, io::error> unselect() const {
            return async_unselect().wait();
        }

        async::task<expected<void, io::error>> async_unselect() const noexcept {
            return _co_unselect(_c, false);
        }

        // `close_mailbox(...)` on this thread, `co_await async_close_mailbox(...)` in a task
        expected<void, io::error> close_mailbox() const {
            return async_close_mailbox().wait();
        }

        async::task<expected<void, io::error>> async_close_mailbox() const noexcept {
            return _co_unselect(_c, true);
        }

        // What the client knows of the selected mailbox now: SELECT's
        // answer, with the counts the server's updates moved since
        selected mailbox() const {
            std::lock_guard<std::mutex> g(_c->state_lock);
            return _c->mailbox;
        }

        // --- messages (by UID) ---

        // UID FETCH: the messages of the set with what the options ask
        // (their flags by default), in the server's order
        // `fetch(...)` on this thread, `co_await async_fetch(...)` in a task
        expected<vector<message>, io::error> fetch(const sequence_set& uids) const {
            return async_fetch(uids, fetch_options()).wait();
        }

        expected<vector<message>, io::error> fetch(const sequence_set& uids, const fetch_options& o) const {
            return async_fetch(uids, o).wait();
        }

        async::task<expected<vector<message>, io::error>> async_fetch(sequence_set uids) const noexcept {
            return _co_fetch(_c, std::move(uids), fetch_options());
        }

        async::task<expected<vector<message>, io::error>> async_fetch(sequence_set uids, fetch_options o) const noexcept {
            return _co_fetch(_c, std::move(uids), std::move(o));
        }

        // The whole message of a UID (BODY.PEEK[]); errc::expunged when the
        // server has none of it
        // `fetch_message(...)` on this thread, `co_await async_fetch_message(...)` in a task
        expected<string, io::error> fetch_message(uint32_t uid) const {
            return async_fetch_message(uid).wait();
        }

        async::task<expected<string, io::error>> async_fetch_message(uint32_t uid) const noexcept {
            return _co_fetch_message(_c, uid);
        }

        // UID SEARCH: the UIDs of the messages the criteria take, ascending;
        // the text form takes IMAP's search keys as written ("UNSEEN FROM
        // alice")
        // `search(...)` on this thread, `co_await async_search(...)` in a task
        expected<vector<uint32_t>, io::error> search(const criteria& c = criteria()) const {
            return async_search(c).wait();
        }

        expected<vector<uint32_t>, io::error> search(const string& keys) const {
            return async_search(keys).wait();
        }

        async::task<expected<vector<uint32_t>, io::error>> async_search(criteria c = criteria()) const noexcept {
            return _co_search(_c, string(detail::CriteriaAccess::text(c)), false);
        }

        async::task<expected<vector<uint32_t>, io::error>> async_search(string keys) const noexcept {
            return _co_search(_c, std::move(keys), true);
        }

        // How many messages the criteria take (ESEARCH's COUNT when the
        // server has it)
        // `count(...)` on this thread, `co_await async_count(...)` in a task
        expected<size_t, io::error> count(const criteria& c = criteria()) const {
            return async_count(c).wait();
        }

        async::task<expected<size_t, io::error>> async_count(criteria c = criteria()) const noexcept {
            return _co_count(_c, string(detail::CriteriaAccess::text(c)));
        }

        // UID SORT (RFC 5256): the UIDs the criteria take in the order
        // asked, ties by arrival; descending reverses every key
        // `sort(...)` on this thread, `co_await async_sort(...)` in a task
        expected<vector<uint32_t>, io::error> sort(const vector<order>& by, const criteria& c = criteria(), bool descending = false) const {
            return async_sort(by, c, descending).wait();
        }

        async::task<expected<vector<uint32_t>, io::error>> async_sort(vector<order> by, criteria c = criteria(), bool descending = false) const noexcept {
            return _co_sort(_c, std::move(by), string(detail::CriteriaAccess::text(c)), descending);
        }

        // UID THREAD (RFC 5256): the messages the criteria take as threads
        // `threads(...)` on this thread, `co_await async_threads(...)` in a task
        expected<vector<thread>, io::error> threads(const criteria& c = criteria(), threading algorithm = threading::references) const {
            return async_threads(c, algorithm).wait();
        }

        async::task<expected<vector<thread>, io::error>> async_threads(criteria c = criteria(), threading algorithm = threading::references) const noexcept {
            return _co_threads(_c, string(detail::CriteriaAccess::text(c)), algorithm);
        }

        // UID STORE: the flags of the messages replaced, added to or
        // removed from; with unchanged_since (CONDSTORE) only the messages
        // not changed past it: the UIDs left alone (MODIFIED) returned
        // `store(...)` on this thread, `co_await async_store(...)` in a task
        expected<vector<uint32_t>, io::error> store(const sequence_set& uids, store_mode mode, const vector<string>& flags, uint64_t unchanged_since = 0) const {
            return async_store(uids, mode, flags, unchanged_since).wait();
        }

        async::task<expected<vector<uint32_t>, io::error>> async_store(sequence_set uids, store_mode mode, vector<string> flags, uint64_t unchanged_since = 0) const noexcept {
            return _co_store(_c, std::move(uids), mode, std::move(flags), unchanged_since);
        }

        // The flags added: `c.add_flags(uids, {imap::flag::seen})`
        // `add_flags(...)` on this thread, `co_await async_add_flags(...)` in a task
        expected<void, io::error> add_flags(const sequence_set& uids, const vector<string>& flags) const {
            return async_add_flags(uids, flags).wait();
        }

        async::task<expected<void, io::error>> async_add_flags(sequence_set uids, vector<string> flags) const noexcept {
            return _co_store_void(_c, std::move(uids), store_mode::add, std::move(flags));
        }

        // `remove_flags(...)` on this thread, `co_await async_remove_flags(...)` in a task
        expected<void, io::error> remove_flags(const sequence_set& uids, const vector<string>& flags) const {
            return async_remove_flags(uids, flags).wait();
        }

        async::task<expected<void, io::error>> async_remove_flags(sequence_set uids, vector<string> flags) const noexcept {
            return _co_store_void(_c, std::move(uids), store_mode::remove, std::move(flags));
        }

        // `set_flags(...)` on this thread, `co_await async_set_flags(...)` in a task
        expected<void, io::error> set_flags(const sequence_set& uids, const vector<string>& flags) const {
            return async_set_flags(uids, flags).wait();
        }

        async::task<expected<void, io::error>> async_set_flags(sequence_set uids, vector<string> flags) const noexcept {
            return _co_store_void(_c, std::move(uids), store_mode::replace, std::move(flags));
        }

        // UID COPY, UID MOVE (RFC 6851; COPY, STORE \Deleted and UID
        // EXPUNGE where the server has no MOVE): the UIDs the messages got
        // there (COPYUID, RFC 4315)
        // `copy(...)` on this thread, `co_await async_copy(...)` in a task
        expected<copy_result, io::error> copy(const sequence_set& uids, const string& mailbox) const {
            return async_copy(uids, mailbox).wait();
        }

        async::task<expected<copy_result, io::error>> async_copy(sequence_set uids, string mailbox) const noexcept {
            return _co_copy(_c, std::move(uids), std::move(mailbox), false);
        }

        // `move(...)` on this thread, `co_await async_move(...)` in a task
        expected<copy_result, io::error> move(const sequence_set& uids, const string& mailbox) const {
            return async_move(uids, mailbox).wait();
        }

        async::task<expected<copy_result, io::error>> async_move(sequence_set uids, string mailbox) const noexcept {
            return _co_copy(_c, std::move(uids), std::move(mailbox), true);
        }

        // EXPUNGE: the messages marked \Deleted removed; of the set alone
        // with UID EXPUNGE (UIDPLUS)
        // `expunge(...)` on this thread, `co_await async_expunge(...)` in a task
        expected<void, io::error> expunge() const {
            return async_expunge().wait();
        }

        expected<void, io::error> expunge(const sequence_set& uids) const {
            return async_expunge(uids).wait();
        }

        async::task<expected<void, io::error>> async_expunge() const noexcept {
            return _co_expunge(_c, sequence_set());
        }

        async::task<expected<void, io::error>> async_expunge(sequence_set uids) const noexcept {
            return _co_expunge(_c, std::move(uids));
        }

        // APPEND: a message added to a mailbox with its flags and date (now
        // when none): its UID there (APPENDUID; 0 when the server gave none)
        // `append(...)` on this thread, `co_await async_append(...)` in a task
        expected<uint32_t, io::error> append(const string& mailbox, const string& message, const vector<string>& flags = {},
                                             const optional<time::datetime>& date = nullopt) const {
            return async_append(mailbox, message, flags, date).wait();
        }

        async::task<expected<uint32_t, io::error>> async_append(string mailbox, string message, vector<string> flags = {},
                                                                optional<time::datetime> date = nullopt) const noexcept {
            return _co_append(_c, std::move(mailbox), std::move(message), std::move(flags), std::move(date));
        }

        // IDLE (RFC 2177): waits for the server's first updates of the
        // selected mailbox (a new message, an expunge, flags changed), the
        // timeout (29 minutes, under the servers' 30) or the stop, and
        // returns what came (nothing at a timeout or a stop)
        // `idle(...)` on this thread, `co_await async_idle(...)` in a task
        expected<vector<update>, io::error> idle(duration timeout = std::chrono::minutes(29)) const {
            return async_idle(async::stop_token(), timeout).wait();
        }

        expected<vector<update>, io::error> idle(const async::stop_token& stop, duration timeout = std::chrono::minutes(29)) const {
            return async_idle(stop, timeout).wait();
        }

        async::task<expected<vector<update>, io::error>> async_idle(duration timeout = std::chrono::minutes(29)) const noexcept {
            return _co_idle(_c, async::stop_token(), timeout);
        }

        async::task<expected<vector<update>, io::error>> async_idle(async::stop_token stop, duration timeout = std::chrono::minutes(29)) const noexcept {
            return _co_idle(_c, std::move(stop), timeout);
        }

        // --- the rest ---

        // NAMESPACE (RFC 2342)
        // `namespaces(...)` on this thread, `co_await async_namespaces(...)` in a task
        expected<imap::namespaces, io::error> namespaces() const {
            return async_namespaces().wait();
        }

        async::task<expected<imap::namespaces, io::error>> async_namespaces() const noexcept {
            return _co_namespaces(_c);
        }

        // GETQUOTAROOT (RFC 9208): the quota roots of a mailbox with their
        // resources
        // `quota(...)` on this thread, `co_await async_quota(...)` in a task
        expected<vector<imap::quota>, io::error> quota(const string& mailbox = string("INBOX")) const {
            return async_quota(mailbox).wait();
        }

        async::task<expected<vector<imap::quota>, io::error>> async_quota(string mailbox = string("INBOX")) const noexcept {
            return _co_quota(_c, std::move(mailbox));
        }

        // A command the class has no method for, as written after the tag
        // ("XLIST \"\" *", "GETMETADATA ..."): its untagged responses as
        // they came (literals inline), or the error of its NO or BAD
        // `command(...)` on this thread, `co_await async_command(...)` in a task
        expected<vector<string>, io::error> command(const string& line) const {
            return async_command(line).wait();
        }

        async::task<expected<vector<string>, io::error>> async_command(string line) const noexcept {
            return _co_raw(_c, std::move(line));
        }

        // The unseen messages of a mailbox whole, with their flags and
        // envelopes, in one call: SELECT, UID SEARCH UNSEEN, UID FETCH; the
        // messages stay unseen (BODY.PEEK)
        // `fetch_unseen(...)` on this thread, `co_await async_fetch_unseen(...)` in a task
        expected<vector<message>, io::error> fetch_unseen(const string& mailbox = string("INBOX")) const {
            return async_fetch_unseen(mailbox).wait();
        }

        async::task<expected<vector<message>, io::error>> async_fetch_unseen(string mailbox = string("INBOX")) const noexcept {
            return _co_fetch_unseen(_c, std::move(mailbox));
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_c;
        }

    private:
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit client(tracked_ptr<detail::ClientImpl> c) noexcept
        : _c(std::move(c)) {
        }

        SGCL_INLINE_HOT client(sgcl::detail::FromWord, const tracked_ptr<detail::ClientImpl>& w) noexcept
        : _c(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ClientImpl>& _handle_word() noexcept {
            return _c;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ClientImpl>& _handle_word() const noexcept {
            return _c;
        }

        tracked_ptr<detail::ClientImpl> _c;

        using Done = detail::Done;

        // --- connect -----------------------------------------------------------

        static async::task<expected<client, io::error>> _co_connect(string address, net::connection transport, options o) noexcept {
            using namespace detail;
            if (o.stop.stop_requested()) {
                if (transport) {
                    (void)transport.close();
                }
                co_return unexpected(io::error(std::make_error_code(std::errc::operation_canceled), "connect", address));
            }
            // the address: an imap:// or imaps:// URL, or host[:port]
            std::string host, port_text;
            string select_after;
            bool implicit = o.security == security::tls;
            std::string_view a = address.view();
            if (istarts(a, "imap://") || istarts(a, "imaps://")) {
                auto u = net::url::parse(address);
                if (!u) {
                    co_return unexpected(net::detail::net_error(net::errc::invalid_url, "connect", address));
                }
                const bool imaps = iequal(u->scheme().view(), "imaps");
                if (imaps && o.security == security::automatic) {
                    implicit = true;
                }
                host = std::string(u->hostname().view());
                if (u->host_address() && u->host_address()->is_v6()) {
                    host = "[" + host + "]";
                }
                port_text = u->port() ? std::to_string(*u->port()) : (imaps || implicit ? "993" : "143");
                if (o.user.empty() && !u->username().empty()) {
                    o.user = string(net::detail::url_unescape(u->username().view()));
                }
                if (o.password.empty() && !u->password().empty()) {
                    o.password = string(net::detail::url_unescape(u->password().view()));
                }
                std::string path(u->path().view());
                if (path.size() > 1) {
                    select_after = string(net::detail::url_unescape(std::string_view(path).substr(1)));
                }
            } else {
                // host:port, [v6]:port, or a host alone
                const size_t colon = a.rfind(':');
                const bool v6 = !a.empty() && a.front() == '[';
                if (colon != std::string_view::npos && (!v6 || a.find(']') < colon) && a.find(':') == colon) {
                    host = std::string(a.substr(0, colon));
                    port_text = std::string(a.substr(colon + 1));
                } else if (v6 && colon != std::string_view::npos && a.find(']') < colon) {
                    host = std::string(a.substr(0, colon));
                    port_text = std::string(a.substr(colon + 1));
                } else {
                    host = std::string(a);
                    port_text = implicit ? "993" : "143";
                }
                if (port_text == "993" && o.security == security::automatic) {
                    implicit = true;
                }
            }
            std::string bare_host = host;
            if (bare_host.size() > 2 && bare_host.front() == '[') {
                bare_host = bare_host.substr(1, bare_host.size() - 2);
            }
            const string target(host + ":" + port_text);
            tracked_ptr c = make_tracked<ClientImpl>();
            c->host = target.view();
            c->timeout = o.timeout;
            c->on_update = o.on_update;
            c->max_literal = o.max_literal_bytes ? o.max_literal_bytes : 1;
            const time_point connect_deadline = o.timeout > duration::zero() ? sgcl::clock::now() + o.timeout : time_point();
            if (!transport) {
                // the dial stopped by the options' token or the timeout, whichever first
                async::stop_source dial_stop;
                if (o.timeout > duration::zero()) {
                    dial_stop.stop_after(o.timeout);
                }
                if (o.stop.stop_possible()) {
                    async::go(_link_stop(o.stop, dial_stop));
                }
                auto t = o.dial ? co_await o.dial(target, dial_stop.token()) : co_await net::tcp::async_connect(target, dial_stop.token());
                dial_stop.request_stop();   // the link's task ends with it
                if (!t) {
                    co_return unexpected(t.error());
                }
                transport = *t;
            }
            net::tls::config tls = o.tls;
            if (tls.server_name.empty()) {
                tls.server_name = string(bare_host);
            }
            if (connect_deadline != time_point()) {
                tls.handshake_timeout = std::max(duration(connect_deadline - sgcl::clock::now()), duration(std::chrono::milliseconds(1)));
            }
            if (implicit) {
                auto t = co_await net::tls::async_client(transport, tls);
                if (!t) {
                    co_return unexpected(t.error());
                }
                transport = *t;
                c->tls = true;
            }
            c->conn = transport;
            c->in.reset(transport);
            transport.set_read_deadline(connect_deadline);
            auto fail = [&](io::error e) {
                (void)c->conn.close();
                return unexpected(e);
            };
            // the greeting
            std::string buf;
            auto g = co_await read_response(*c, buf);
            if (!g || !*g) {
                co_return fail(g ? closed_error(*c, "greeting") : g.error());
            }
            Response r;
            if (!parse_response(buf, r) || r.kind != Response::Kind::untagged || r.status == Status::none) {
                co_return fail(imap_error(errc::malformed_response, "greeting", str(std::string_view(buf).substr(0, 200))));
            }
            if (r.status == Status::bye) {
                co_return fail(imap_error(errc::bye, "greeting", str(r.text)));
            }
            c->preauth = r.status == Status::preauth;
            if (iequal(r.code, "CAPABILITY")) {
                parse_capabilities(r.code_data, c->caps);
            }
            client cl(c);
            if (c->caps.empty()) {
                auto cap = co_await _co_capability(c);
                if (!cap) {
                    co_return fail(cap.error());
                }
            }
            // STARTTLS
            const bool want_starttls = !c->tls && !c->preauth && (o.security == security::starttls || o.security == security::automatic);
            if (want_starttls) {
                if (!c->has("STARTTLS")) {
                    co_return fail(imap_error(errc::starttls_unavailable, "starttls", string(c->host)));
                }
                auto st = co_await run(c, c->command("STARTTLS"), string("STARTTLS"));
                if (!st) {
                    co_return fail(st.error());
                }
                c->in.discard();   // what came behind the OK in the clear is dropped
                auto t = co_await net::tls::async_client(c->conn, tls);
                if (!t) {
                    co_return fail(t.error());
                }
                c->conn = *t;
                c->in.reset(*t);
                c->tls = true;
                c->caps.clear();
                auto cap = co_await _co_capability(c);
                if (!cap) {
                    co_return fail(cap.error());
                }
            }
            // the login
            if (!c->preauth && (!o.user.empty() || !o.token.empty())) {
                auto a = co_await _co_login(c, o);
                if (!a) {
                    co_return fail(a.error());
                }
                if (!a->empty()) {
                    parse_capabilities(*a, c->caps);
                } else {
                    c->caps.clear();
                    auto cap = co_await _co_capability(c);
                    if (!cap) {
                        co_return fail(cap.error());
                    }
                }
            }
            c->conn.set_read_deadline(time_point());
            const bool authenticated = c->preauth || !o.user.empty() || !o.token.empty();
            if (authenticated) {
                // ENABLE what the client speaks
                std::string want;
                if (c->has("IMAP4REV2")) {
                    want += " IMAP4rev2";
                } else if (c->has("UTF8=ACCEPT")) {
                    want += " UTF8=ACCEPT";
                }
                if (c->has("QRESYNC")) {
                    want += " QRESYNC";
                } else if (c->has("CONDSTORE")) {
                    want += " CONDSTORE";
                }
                if (!want.empty() && c->has("ENABLE")) {
                    Command cmd = c->command("ENABLE");
                    cmd.raw(want);
                    auto e = co_await run(c, std::move(cmd), string("ENABLE"));
                    if (!e) {
                        co_return fail(e.error());
                    }
                }
                if (o.compress && c->has("COMPRESS=DEFLATE")) {
                    auto z = co_await run(c, c->command("COMPRESS DEFLATE"), string("COMPRESS"));
                    if (!z) {
                        co_return fail(z.error());
                    }
                    c->conn = deflate_connection(c->conn, c->in.pending());
                    c->in.discard();
                    c->in.reset(c->conn);
                    c->compressed = true;
                }
            }
            if (!o.id.empty() && c->has("ID")) {
                Command cmd = c->command("ID (");
                bool first = true;
                for (const auto& [k, v] : o.id) {
                    if (!first) {
                        cmd.sp();
                    }
                    first = false;
                    cmd.string(k.view());
                    cmd.sp();
                    cmd.string(v.view());
                }
                cmd.raw(")");
                auto d = co_await run(c, std::move(cmd), string("ID"));
                if (d) {
                    each_untagged(*d, "ID", [&](Lexer& x, const Response&) {
                        vector<pair<string, string>> got;
                        if (parse_id(x, got, c->scratch)) {
                            c->server_id = got;
                        }
                        return true;
                    });
                }
            }
            if (!select_after.empty() && authenticated) {
                auto s = co_await _co_select(c, select_after, select_options());
                if (!s) {
                    co_return fail(s.error());
                }
            }
            co_return cl;
        }

        // The dial's source stopped when the program's token is
        static async::task<void> _link_stop(async::stop_token from, async::stop_source to) noexcept {
            co_await async::select(from.channel().on_receive([] {}), to.token().channel().on_receive([] {}));
            to.request_stop();
        }

        static async::task<expected<void, io::error>> _co_capability(tracked_ptr<detail::ClientImpl> c) noexcept {
            auto d = co_await detail::run(c, c->command("CAPABILITY"), string("CAPABILITY"));
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        // The login by the mechanism chosen: the capabilities of the tagged
        // OK when it gave them
        static async::task<expected<std::string, io::error>> _co_login(tracked_ptr<detail::ClientImpl> c, options o) noexcept {
            using namespace detail;
            mechanism m = o.mechanism;
            if (m == mechanism::automatic) {
                if (!o.token.empty()) {
                    m = c->has("AUTH=OAUTHBEARER") ? mechanism::oauthbearer : mechanism::xoauth2;
                } else if (c->has("AUTH=PLAIN")) {
                    m = mechanism::plain;
                } else if (!c->has("LOGINDISABLED")) {
                    m = mechanism::login_command;
                } else if (c->has("AUTH=LOGIN")) {
                    m = mechanism::login;
                } else {
                    co_return unexpected(imap_error(errc::privacy_required, "login", string(c->host)));
                }
            }
            if (m == mechanism::login_command) {
                Command cmd = c->command("LOGIN");
                cmd.sp();
                cmd.astring(o.user.view());
                cmd.sp();
                cmd.astring(o.password.view());
                auto d = co_await run(c, std::move(cmd), string("LOGIN"));
                if (!d) {
                    co_return unexpected(_auth_error(d.error()));
                }
                co_return iequal(d->code, "CAPABILITY") ? d->code_data : std::string();
            }
            // AUTHENTICATE with the initial response where SASL-IR allows
            std::string first;
            const char* name = "PLAIN";
            switch (m) {
                case mechanism::plain:
                    first = std::string("\0", 1) + std::string(o.user.view()) + std::string("\0", 1) + std::string(o.password.view());
                    break;
                case mechanism::login:
                    name = "LOGIN";
                    break;
                case mechanism::xoauth2:
                    name = "XOAUTH2";
                    first = "user=" + std::string(o.user.view()) + "\x01" + "auth=Bearer " + std::string(o.token.view()) + "\x01\x01";
                    break;
                case mechanism::oauthbearer:
                    name = "OAUTHBEARER";
                    first = "n,a=" + std::string(o.user.view()) + ",\x01" + "host=" + std::string(c->host.substr(0, c->host.rfind(':'))) + "\x01" + "port=" +
                            c->host.substr(c->host.rfind(':') + 1) + "\x01" + "auth=Bearer " + std::string(o.token.view()) + "\x01\x01";
                    break;
                default: break;
            }
            co_return co_await _co_authenticate(c, name, first, m == mechanism::login, o);
        }

        static io::error _auth_error(const io::error& e) noexcept {
            if (e.code() == make_error_code(errc::no)) {
                return detail::imap_error(errc::authentication_failed, e.op(), e.path());
            }
            return e;
        }

        static async::task<expected<std::string, io::error>> _co_authenticate(tracked_ptr<detail::ClientImpl> c, const char* name, std::string first, bool login,
                                                                              options o) noexcept {
            using namespace detail;
            ClientImpl& ci = *c;
            const bool ir = !login && ci.has("SASL-IR");
            Command cmd = ci.command("AUTHENTICATE ");
            cmd.raw(name);
            if (ir) {
                cmd.sp();
                cmd.raw(first.empty() ? std::string("=") : sasl_base64(first));
            }
            cmd.end();
            const std::string tag(ClientImpl::tag_of(cmd));
            auto p = std::make_shared<Pending>();
            p->tag = tag;
            {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.pending.push_back(p);
            }
            auto forget = [&] {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.pending.erase(std::remove(ci.pending.begin(), ci.pending.end(), p), ci.pending.end());
            };
            auto wg = co_await ci.write_lock.scoped_lock();
            auto rg = co_await ci.read_lock.scoped_lock();
            auto wr = co_await write_all(ci.conn, cmd.text);
            if (!wr) {
                forget();
                co_return unexpected(wr.error());
            }
            std::string buf, cont;
            int step = 0;
            while (!p->done) {
                deadline(ci);
                auto one = co_await read_one(ci, buf, cont);
                if (!one) {
                    forget();
                    co_return unexpected(one.error());
                }
                if (*one != Read::continuation) {
                    continue;
                }
                {
                    std::lock_guard<std::mutex> g(ci.state_lock);
                    ci.continuations = 0;
                }
                // the server's challenge: the next answer, or an empty one
                std::string answer;
                if (login) {
                    answer = sasl_base64(step == 0 ? std::string(o.user.view()) : std::string(o.password.view()));
                } else if (step == 0 && !ir) {
                    answer = sasl_base64(first);
                } else {
                    answer = "";   // an error challenge (XOAUTH2, OAUTHBEARER): the empty answer
                }
                ++step;
                if (step > 4) {
                    answer = "*";
                }
                answer += "\r\n";
                auto w2 = co_await write_all(ci.conn, answer);
                if (!w2) {
                    forget();
                    co_return unexpected(w2.error());
                }
            }
            forget();
            if (p->result.status != Status::ok) {
                const errc e = p->result.status == Status::bad ? errc::bad : code_of(p->result.code, false);
                co_return unexpected(imap_error(e == errc::no ? errc::authentication_failed : e, "AUTHENTICATE", str(p->result.text)));
            }
            co_return iequal(p->result.code, "CAPABILITY") ? p->result.code_data : std::string();
        }

        // --- simple commands -------------------------------------------------------

        static async::task<expected<void, io::error>> _co_simple(tracked_ptr<detail::ClientImpl> c, string name) noexcept {
            auto d = co_await detail::run(c, c->command(name.view()), name);
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_logout(tracked_ptr<detail::ClientImpl> c) noexcept {
            auto d = co_await detail::run(c, c->command("LOGOUT"), string("LOGOUT"));
            c->closed.store(true);
            (void)co_await c->conn.async_close();
            if (!d && d.error().code() != make_error_code(errc::bye) && !d.error().is_closed()) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<vector<string>, io::error>> _co_enable(tracked_ptr<detail::ClientImpl> c, vector<string> caps) noexcept {
            using namespace detail;
            Command cmd = c->command("ENABLE");
            for (const auto& x : caps) {
                cmd.sp();
                cmd.raw(x.view());
            }
            auto d = co_await run(c, std::move(cmd), string("ENABLE"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<string> out;
            for (const auto& raw : d->untagged) {
                Response r;
                if (parse_response(raw, r) && iequal(r.name, "ENABLED")) {
                    std::vector<std::string> list;
                    parse_capabilities(std::string_view(raw).substr(r.data_at), list);
                    for (auto& e : list) {
                        out.push_back(string(e));
                    }
                }
            }
            co_return out;
        }

        static async::task<expected<vector<list_entry>, io::error>> _co_list(tracked_ptr<detail::ClientImpl> c, string pattern, list_options o) noexcept {
            using namespace detail;
            const bool extended = c->has("LIST-EXTENDED") || c->has("IMAP4REV2");
            const bool lsub = o.subscribed && !extended;
            Command cmd = c->command(lsub ? "LSUB " : "LIST ");
            if (!lsub && (o.subscribed || o.special_use)) {
                cmd.raw("(");
                if (o.subscribed) {
                    cmd.raw("SUBSCRIBED");
                }
                if (o.special_use) {
                    cmd.raw(o.subscribed ? " SPECIAL-USE" : "SPECIAL-USE");
                }
                cmd.raw(") ");
            }
            if (o.reference.empty()) {
                cmd.raw("\"\"");
            } else {
                cmd.mailbox(o.reference.view());
            }
            cmd.sp();
            // the pattern: list-mailbox characters, else quoted
            std::string p = c->utf8 ? std::string(pattern.view()) : utf7_encode(pattern.view());
            bool plain = !p.empty();
            for (unsigned char ch : p) {
                plain &= list_char(ch);
            }
            if (plain) {
                cmd.raw(p);
            } else {
                cmd.string(p);
            }
            const bool status = o.status && (c->has("LIST-STATUS") || c->has("IMAP4REV2"));
            if (!lsub && status) {
                cmd.raw(" RETURN (STATUS (MESSAGES UIDNEXT UIDVALIDITY UNSEEN");
                if (c->has("STATUS=SIZE") || c->has("IMAP4REV2")) {
                    cmd.raw(" SIZE");
                }
                if (c->condstore) {
                    cmd.raw(" HIGHESTMODSEQ");
                }
                cmd.raw("))");
            }
            auto d = co_await run(c, std::move(cmd), string(lsub ? "LSUB" : "LIST"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<list_entry> out;
            const char* name = lsub ? "LSUB" : "LIST";
            std::string scratch;
            for (const auto& raw : d->untagged) {
                Response r;
                if (!parse_response(raw, r) || r.status != Status::none) {
                    continue;
                }
                Lexer x(std::string_view(raw).substr(r.data_at));
                x.set_lenient(true);
                if (iequal(r.name, name)) {
                    list_entry e;
                    if (!parse_list(x, e, c->utf8, scratch)) {
                        co_return unexpected(malformed(name));
                    }
                    out.push_back(e);
                } else if (iequal(r.name, "STATUS") && status) {
                    string n;
                    imap::status s;
                    if (!parse_status(x, n, s, c->utf8, scratch)) {
                        co_return unexpected(malformed("STATUS"));
                    }
                    for (auto& e : out) {
                        if (e.name == n) {
                            e.status = s;
                        }
                    }
                }
            }
            if (o.special_use) {
                // a server without the selection option: the special ones kept
                vector<list_entry> keep;
                for (const auto& e : out) {
                    for (const char* a : {"\\All", "\\Archive", "\\Drafts", "\\Flagged", "\\Junk", "\\Sent", "\\Trash"}) {
                        if (e.has_attribute(string(a))) {
                            keep.push_back(e);
                            break;
                        }
                    }
                }
                out = keep;
            }
            co_return out;
        }

        static async::task<expected<imap::status, io::error>> _co_status(tracked_ptr<detail::ClientImpl> c, string mailbox) noexcept {
            using namespace detail;
            Command cmd = c->command("STATUS ");
            cmd.mailbox(mailbox.view());
            cmd.raw(" (MESSAGES UIDNEXT UIDVALIDITY UNSEEN");
            if (c->has("IMAP4REV2") || c->rev2) {
                cmd.raw(" DELETED SIZE");
            } else if (c->has("STATUS=SIZE")) {
                cmd.raw(" SIZE");
            }
            if (!c->rev2) {
                cmd.raw(" RECENT");
            }
            if (c->has("CONDSTORE") || c->has("QRESYNC")) {
                cmd.raw(" HIGHESTMODSEQ");
            }
            cmd.raw(")");
            auto d = co_await run(c, std::move(cmd), string("STATUS " + std::string(mailbox.view())));
            if (!d) {
                co_return unexpected(d.error());
            }
            imap::status out;
            bool found = false;
            std::string scratch;
            if (!each_untagged(*d, "STATUS", [&](Lexer& x, const Response&) {
                    string n;
                    imap::status s;
                    if (!parse_status(x, n, s, c->utf8, scratch)) {
                        return false;
                    }
                    if (n == mailbox || (iequal(n.view(), "INBOX") && iequal(mailbox.view(), "INBOX"))) {
                        out = s;
                        found = true;
                    }
                    return true;
                })) {
                co_return unexpected(malformed("STATUS"));
            }
            if (!found) {
                co_return unexpected(malformed("STATUS"));
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_mailbox_command(tracked_ptr<detail::ClientImpl> c, string name, string mailbox, string use) noexcept {
            using namespace detail;
            Command cmd = c->command(name.view());
            cmd.sp();
            cmd.mailbox(mailbox.view());
            if (!use.empty()) {
                cmd.raw(" (USE (");
                cmd.raw(use.view());
                cmd.raw("))");
            }
            auto d = co_await run(c, std::move(cmd), string(std::string(name.view()) + " " + std::string(mailbox.view())));
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_rename(tracked_ptr<detail::ClientImpl> c, string from, string to) noexcept {
            using namespace detail;
            Command cmd = c->command("RENAME ");
            cmd.mailbox(from.view());
            cmd.sp();
            cmd.mailbox(to.view());
            auto d = co_await run(c, std::move(cmd), string("RENAME " + std::string(from.view())));
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<selected, io::error>> _co_select(tracked_ptr<detail::ClientImpl> c, string mailbox, select_options o) noexcept {
            using namespace detail;
            Command cmd = c->command(o.read_only ? "EXAMINE " : "SELECT ");
            cmd.mailbox(mailbox.view());
            const bool qr = o.uid_validity && o.modseq && c->qresync;
            if (qr) {
                cmd.raw(" (QRESYNC (");
                cmd.number(o.uid_validity);
                cmd.sp();
                cmd.number(o.modseq);
                if (!o.known_uids.empty()) {
                    cmd.sp();
                    cmd.raw(o.known_uids.to_string().view());
                }
                cmd.raw("))");
            } else if (c->has("CONDSTORE") && !c->condstore) {
                cmd.raw(" (CONDSTORE)");
            }
            {
                std::lock_guard<std::mutex> g(c->state_lock);
                c->mailbox = selected();
                c->mailbox.name = mailbox;
                c->has_mailbox = true;
            }
            auto d = co_await run(c, std::move(cmd), string(std::string(o.read_only ? "EXAMINE " : "SELECT ") + std::string(mailbox.view())));
            if (!d) {
                std::lock_guard<std::mutex> g(c->state_lock);
                c->has_mailbox = false;
                c->mailbox = selected();
                co_return unexpected(d.error());
            }
            selected out;
            std::string scratch;
            {
                std::lock_guard<std::mutex> g(c->state_lock);
                c->mailbox.read_only = iequal(d->code, "READ-ONLY");
                out = c->mailbox;
            }
            if (qr) {
                for (const auto& raw : d->untagged) {
                    Response r;
                    if (!parse_response(raw, r) || r.status != Status::none) {
                        continue;
                    }
                    Lexer x(std::string_view(raw).substr(r.data_at));
                    x.set_lenient(true);
                    if (iequal(r.name, "VANISHED")) {
                        bool earlier;
                        sequence_set s;
                        if (parse_vanished(x, earlier, s)) {
                            for (auto [a, b] : s.ranges()) {
                                out.vanished.add(a, b);
                            }
                        }
                    } else if (r.numbered && iequal(r.name, "FETCH")) {
                        message m;
                        if (parse_fetch(x, m, scratch)) {
                            m.seq = r.number;
                            out.changed.push_back(m);
                        }
                    }
                }
                std::lock_guard<std::mutex> g(c->state_lock);
                c->mailbox.vanished = out.vanished;
                c->mailbox.changed = out.changed;
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_unselect(tracked_ptr<detail::ClientImpl> c, bool expunge) noexcept {
            using namespace detail;
            const bool has_unselect = c->has("UNSELECT") || c->has("IMAP4REV2");
            expected<Done, io::error> d;
            if (expunge || has_unselect) {
                d = co_await run(c, c->command(expunge ? "CLOSE" : "UNSELECT"), string(expunge ? "CLOSE" : "UNSELECT"));
            } else {
                // without UNSELECT: an EXAMINE that fails leaves nothing selected
                Command cmd = c->command("EXAMINE ");
                cmd.raw("\"\"");
                d = co_await run(c, std::move(cmd), string("UNSELECT"), true);
            }
            {
                std::lock_guard<std::mutex> g(c->state_lock);
                c->has_mailbox = false;
                c->mailbox = selected();
            }
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        // --- FETCH ---------------------------------------------------------------

        static async::task<expected<vector<message>, io::error>> _co_fetch(tracked_ptr<detail::ClientImpl> c, sequence_set uids, fetch_options o) noexcept {
            using namespace detail;
            if (uids.empty()) {
                co_return vector<message>();
            }
            Command cmd = c->command("UID FETCH ");
            cmd.raw(uids.to_string().view());
            cmd.raw(" (UID");
            if (o.flags) {
                cmd.raw(" FLAGS");
            }
            if (o.envelope) {
                cmd.raw(" ENVELOPE");
            }
            if (o.body_structure) {
                cmd.raw(" BODYSTRUCTURE");
            }
            if (o.size) {
                cmd.raw(" RFC822.SIZE");
            }
            if (o.internal_date) {
                cmd.raw(" INTERNALDATE");
            }
            if (o.modseq || o.changed_since) {
                cmd.raw(" MODSEQ");
            }
            const bool binary = o.binary && (c->has("BINARY") || c->has("IMAP4REV2"));
            for (const auto& s : o.sections) {
                cmd.raw(binary ? (o.mark_seen ? " BINARY[" : " BINARY.PEEK[") : (o.mark_seen ? " BODY[" : " BODY.PEEK["));
                cmd.raw(s.view());
                cmd.raw("]");
                if (o.partial_length) {
                    cmd.raw("<");
                    cmd.number(o.partial_offset);
                    cmd.raw(".");
                    cmd.number(o.partial_length);
                    cmd.raw(">");
                }
            }
            cmd.raw(")");
            if (o.changed_since) {
                cmd.raw(" (CHANGEDSINCE ");
                cmd.number(o.changed_since);
                cmd.raw(")");
            }
            auto d = co_await run(c, std::move(cmd), string("UID FETCH"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<message> out;
            out.reserve(d->untagged.size());
            std::string scratch;
            // the messages of the set (a FETCH another session's change
            // pushed meanwhile names a UID outside it, or none)
            uint32_t largest = 0;
            for (auto [a, b] : uids.ranges()) {
                largest = std::max({largest, a, b});
            }
            for (const auto& raw : d->untagged) {
                Response r;
                if (!parse_response(raw, r) || r.status != Status::none || !r.numbered || !iequal(r.name, "FETCH")) {
                    continue;
                }
                Lexer x(std::string_view(raw).substr(r.data_at));
                x.set_lenient(true);
                message m;
                if (!parse_fetch(x, m, scratch)) {
                    co_return unexpected(malformed("UID FETCH"));
                }
                m.seq = r.number;
                if (!m.uid || !uids.contains(m.uid, std::max(largest, m.uid))) {
                    continue;
                }
                // a message's items may come in more than one response: the
                // last message's merged at once, an earlier one's looked for
                // (responses come in the order of the numbers, so rarely)
                message* same = nullptr;
                if (!out.empty() && out.back().uid == m.uid) {
                    same = &out.back();
                } else if (!out.empty() && m.uid < out.back().uid) {
                    for (auto& e : out) {
                        if (e.uid == m.uid) {
                            same = &e;
                            break;
                        }
                    }
                }
                if (!same) {
                    out.push_back(std::move(m));
                    continue;
                }
                message& e = *same;
                if (!m.flags.empty()) {
                    e.flags = m.flags;
                }
                for (auto& sec : m.sections) {
                    e.sections.push_back(sec);
                }
                if (m.envelope) {
                    e.envelope = m.envelope;
                }
                if (m.body_structure) {
                    e.body_structure = m.body_structure;
                }
                if (m.internal_date) {
                    e.internal_date = m.internal_date;
                }
                e.size = m.size ? m.size : e.size;
                e.modseq = m.modseq ? m.modseq : e.modseq;
            }
            co_return out;
        }

        static async::task<expected<string, io::error>> _co_fetch_message(tracked_ptr<detail::ClientImpl> c, uint32_t uid) noexcept {
            fetch_options o;
            o.flags = false;
            o.sections = {string()};
            auto r = co_await _co_fetch(c, sequence_set(uid), o);
            if (!r) {
                co_return unexpected(r.error());
            }
            for (const auto& m : *r) {
                if (m.uid == uid) {
                    co_return m.text();
                }
            }
            co_return unexpected(detail::imap_error(errc::expunged, "UID FETCH", string(std::to_string(uid))));
        }

        static async::task<expected<vector<message>, io::error>> _co_fetch_unseen(tracked_ptr<detail::ClientImpl> c, string mailbox) noexcept {
            auto s = co_await _co_select(c, mailbox, select_options());
            if (!s) {
                co_return unexpected(s.error());
            }
            auto uids = co_await _co_search(c, string("UNSEEN"), true);
            if (!uids) {
                co_return unexpected(uids.error());
            }
            if (uids->empty()) {
                co_return vector<message>();
            }
            fetch_options o;
            o.envelope = true;
            o.internal_date = true;
            o.size = true;
            o.sections = {string()};
            co_return co_await _co_fetch(c, sequence_set(*uids), o);
        }

        // --- SEARCH, SORT, THREAD ---------------------------------------------------

        // The criteria with CHARSET UTF-8 when they hold 8-bit text and
        // the connection has no UTF-8 of its own
        static void _put_criteria(detail::Command& cmd, std::string_view text, bool raw, const detail::ClientImpl& c) {
            if (raw) {
                cmd.raw(text);
                return;
            }
            cmd.criteria(text);
        }

        static bool _eight_bit(std::string_view s) noexcept {
            for (unsigned char ch : s) {
                if (ch >= 0x80) {
                    return true;
                }
            }
            return false;
        }

        static async::task<expected<vector<uint32_t>, io::error>> _co_search(tracked_ptr<detail::ClientImpl> c, string keys, bool raw) noexcept {
            using namespace detail;
            Command cmd = c->command("UID SEARCH ");
            const bool esearch = c->rev2;   // a rev2 server answers ESEARCH anyway
            if (esearch) {
                cmd.raw("RETURN (ALL) ");
            }
            if (!c->utf8 && _eight_bit(keys.view())) {
                cmd.raw("CHARSET UTF-8 ");
            }
            _put_criteria(cmd, keys.view(), raw, *c);
            auto d = co_await run(c, std::move(cmd), string("UID SEARCH"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<uint32_t> out;
            std::string scratch;
            for (const auto& rawr : d->untagged) {
                Response r;
                if (!parse_response(rawr, r) || r.status != Status::none) {
                    continue;
                }
                Lexer x(std::string_view(rawr).substr(r.data_at));
                x.set_lenient(true);
                if (iequal(r.name, "SEARCH")) {
                    uint64_t modseq = 0;
                    if (!parse_numbers(x, out, modseq)) {
                        co_return unexpected(malformed("UID SEARCH"));
                    }
                } else if (iequal(r.name, "ESEARCH")) {
                    Esearch e;
                    if (!parse_esearch(x, e, scratch)) {
                        co_return unexpected(malformed("UID SEARCH"));
                    }
                    for (auto [a, b] : e.all.ranges()) {
                        if (!a || !b) {
                            continue;
                        }
                        for (uint64_t n = std::min(a, b); n <= std::max(a, b); ++n) {
                            out.push_back(uint32_t(n));
                        }
                    }
                }
            }
            std::sort(out.begin(), out.end());
            co_return out;
        }

        static async::task<expected<size_t, io::error>> _co_count(tracked_ptr<detail::ClientImpl> c, string keys) noexcept {
            using namespace detail;
            if (!c->has("ESEARCH") && !c->has("IMAP4REV2")) {
                auto r = co_await _co_search(c, keys, false);
                if (!r) {
                    co_return unexpected(r.error());
                }
                co_return r->size();
            }
            Command cmd = c->command("UID SEARCH RETURN (COUNT) ");
            if (!c->utf8 && _eight_bit(keys.view())) {
                cmd.raw("CHARSET UTF-8 ");
            }
            cmd.criteria(keys.view());
            auto d = co_await run(c, std::move(cmd), string("UID SEARCH"));
            if (!d) {
                co_return unexpected(d.error());
            }
            size_t n = 0;
            std::string scratch;
            if (!each_untagged(*d, "ESEARCH", [&](Lexer& x, const Response&) {
                    Esearch e;
                    if (!parse_esearch(x, e, scratch)) {
                        return false;
                    }
                    n = e.count;
                    return true;
                })) {
                co_return unexpected(malformed("UID SEARCH"));
            }
            co_return n;
        }

        static async::task<expected<vector<uint32_t>, io::error>> _co_sort(tracked_ptr<detail::ClientImpl> c, vector<order> by, string keys, bool descending) noexcept {
            using namespace detail;
            if (!c->has("SORT")) {
                co_return unexpected(imap_error(errc::not_supported, "UID SORT", string(c->host)));
            }
            Command cmd = c->command("UID SORT (");
            if (by.empty()) {
                by.push_back(order::arrival);
            }
            for (size_t i = 0; i < by.size(); ++i) {
                if (i) {
                    cmd.sp();
                }
                if (descending) {
                    cmd.raw("REVERSE ");
                }
                static constexpr const char* names[] = {"ARRIVAL", "CC", "DATE", "FROM", "SIZE", "SUBJECT", "TO", "DISPLAYFROM", "DISPLAYTO"};
                cmd.raw(names[size_t(by[i])]);
            }
            cmd.raw(") UTF-8 ");
            cmd.criteria(keys.view());
            auto d = co_await run(c, std::move(cmd), string("UID SORT"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<uint32_t> out;
            if (!each_untagged(*d, "SORT", [&](Lexer& x, const Response&) {
                    uint64_t modseq;
                    return parse_numbers(x, out, modseq);
                })) {
                co_return unexpected(malformed("UID SORT"));
            }
            co_return out;
        }

        static async::task<expected<vector<thread>, io::error>> _co_threads(tracked_ptr<detail::ClientImpl> c, string keys, threading alg) noexcept {
            using namespace detail;
            const char* name = alg == threading::references ? "REFERENCES" : "ORDEREDSUBJECT";
            if (!c->has(std::string("THREAD=") + name)) {
                co_return unexpected(imap_error(errc::not_supported, "UID THREAD", string(c->host)));
            }
            Command cmd = c->command("UID THREAD ");
            cmd.raw(name);
            cmd.raw(" UTF-8 ");
            cmd.criteria(keys.view());
            auto d = co_await run(c, std::move(cmd), string("UID THREAD"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<thread> out;
            if (!each_untagged(*d, "THREAD", [&](Lexer& x, const Response&) { return parse_threads(x, out); })) {
                co_return unexpected(malformed("UID THREAD"));
            }
            co_return out;
        }

        // --- STORE, COPY, MOVE, EXPUNGE, APPEND ------------------------------------

        static void _put_flags(detail::Command& cmd, const vector<string>& flags) {
            cmd.raw("(");
            for (size_t i = 0; i < flags.size(); ++i) {
                if (i) {
                    cmd.sp();
                }
                cmd.raw(flags[i].view());
            }
            cmd.raw(")");
        }

        static bool _valid_flags(const vector<string>& flags) noexcept {
            for (const auto& f : flags) {
                std::string_view v = f.view();
                if (v.empty()) {
                    return false;
                }
                size_t i = v[0] == '\\' ? 1 : 0;
                if (i == v.size()) {
                    return false;
                }
                for (; i < v.size(); ++i) {
                    if (!detail::atom_char((unsigned char)v[i])) {
                        return false;
                    }
                }
            }
            return true;
        }

        static async::task<expected<vector<uint32_t>, io::error>> _co_store(tracked_ptr<detail::ClientImpl> c, sequence_set uids, store_mode mode, vector<string> flags,
                                                                            uint64_t unchanged_since) noexcept {
            using namespace detail;
            if (!_valid_flags(flags)) {
                co_return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "UID STORE", string("a flag that is not an atom")));
            }
            if (uids.empty()) {
                co_return vector<uint32_t>();
            }
            Command cmd = c->command("UID STORE ");
            cmd.raw(uids.to_string().view());
            if (unchanged_since) {
                cmd.raw(" (UNCHANGEDSINCE ");
                cmd.number(unchanged_since);
                cmd.raw(")");
            }
            cmd.raw(mode == store_mode::add ? " +FLAGS.SILENT " : mode == store_mode::remove ? " -FLAGS.SILENT " : " FLAGS.SILENT ");
            _put_flags(cmd, flags);
            auto d = co_await run(c, std::move(cmd), string("UID STORE"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<uint32_t> modified;
            if (iequal(d->code, "MODIFIED")) {
                Lexer x(d->code_data);
                sequence_set s;
                if (SequenceAccess::read(s, x)) {
                    modified = s.expand(UINT32_MAX - 1);
                }
            }
            co_return modified;
        }

        static async::task<expected<void, io::error>> _co_store_void(tracked_ptr<detail::ClientImpl> c, sequence_set uids, store_mode mode, vector<string> flags) noexcept {
            auto r = co_await _co_store(c, std::move(uids), mode, std::move(flags), 0);
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<copy_result, io::error>> _co_copy(tracked_ptr<detail::ClientImpl> c, sequence_set uids, string mailbox, bool move) noexcept {
            using namespace detail;
            if (uids.empty()) {
                co_return copy_result();
            }
            const bool has_move = c->has("MOVE") || c->has("IMAP4REV2");
            Command cmd = c->command(move && has_move ? "UID MOVE " : "UID COPY ");
            cmd.raw(uids.to_string().view());
            cmd.sp();
            cmd.mailbox(mailbox.view());
            auto d = co_await run(c, std::move(cmd), string(std::string(move ? "UID MOVE " : "UID COPY ") + std::string(mailbox.view())));
            if (!d) {
                co_return unexpected(d.error());
            }
            copy_result out;
            if (iequal(d->code, "COPYUID")) {
                parse_copyuid(d->code_data, out);
            } else {
                for (const auto& raw : d->untagged) {
                    Response r;
                    if (parse_response(raw, r) && r.status == Status::ok && iequal(r.code, "COPYUID")) {
                        parse_copyuid(r.code_data, out);
                    }
                }
            }
            if (move && !has_move) {
                auto s = co_await _co_store(c, uids, store_mode::add, {string("\\Deleted")}, 0);
                if (!s) {
                    co_return unexpected(s.error());
                }
                auto e = co_await _co_expunge(c, uids);
                if (!e) {
                    co_return unexpected(e.error());
                }
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_expunge(tracked_ptr<detail::ClientImpl> c, sequence_set uids) noexcept {
            using namespace detail;
            if (uids.empty()) {
                auto d = co_await run(c, c->command("EXPUNGE"), string("EXPUNGE"));
                if (!d) {
                    co_return unexpected(d.error());
                }
                co_return expected<void, io::error>();
            }
            if (!c->has("UIDPLUS") && !c->has("IMAP4REV2")) {
                co_return unexpected(imap_error(errc::not_supported, "UID EXPUNGE", string(c->host)));
            }
            Command cmd = c->command("UID EXPUNGE ");
            cmd.raw(uids.to_string().view());
            auto d = co_await run(c, std::move(cmd), string("UID EXPUNGE"));
            if (!d) {
                co_return unexpected(d.error());
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<uint32_t, io::error>> _co_append(tracked_ptr<detail::ClientImpl> c, string mailbox, string message, vector<string> flags,
                                                                     optional<time::datetime> date) noexcept {
            using namespace detail;
            if (!_valid_flags(flags)) {
                co_return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "APPEND", string("a flag that is not an atom")));
            }
            Command cmd = c->command("APPEND ");
            cmd.mailbox(mailbox.view());
            if (!flags.empty()) {
                cmd.sp();
                _put_flags(cmd, flags);
            }
            if (date) {
                cmd.raw(" \"");
                cmd.raw(format_date_time(DateTime{date->unix(), int(date->offset().seconds() / 60)}));
                cmd.raw("\"");
            }
            cmd.sp();
            // a message with NUL as literal8 where BINARY allows it
            const bool nul = message.view().find('\0') != std::string_view::npos;
            const bool eight_headers = c->utf8 && _eight_bit(message.view());
            if (eight_headers && c->has("UTF8=ACCEPT")) {
                cmd.raw("UTF8 (");
                cmd.literal(message.view(), true);
                cmd.raw(")");
            } else {
                cmd.literal(message.view(), nul && (c->has("BINARY") || c->has("IMAP4REV2")));
            }
            auto d = co_await run(c, std::move(cmd), string("APPEND " + std::string(mailbox.view())));
            if (!d) {
                co_return unexpected(d.error());
            }
            if (iequal(d->code, "APPENDUID")) {
                uint32_t validity;
                vector<uint32_t> uids;
                if (parse_appenduid(d->code_data, validity, uids) && !uids.empty()) {
                    co_return uids.back();
                }
            }
            co_return uint32_t(0);
        }

        // --- IDLE ------------------------------------------------------------------

        static async::task<void> _idle_watch(tracked_ptr<detail::ClientImpl> c, async::stop_token stop, time_point until, async::event done,
                                             std::shared_ptr<std::atomic<bool>> sent) noexcept {
            if (stop.stop_possible()) {
                co_await async::select(stop.channel().on_receive([] {}), async::timeout(until, [] {}), done.on_set([] {}));
            } else {
                co_await async::select(async::timeout(until, [] {}), done.on_set([] {}));
            }
            if (!sent->exchange(true)) {
                std::string d = "DONE\r\n";
                (void)co_await detail::write_all(c->conn, d);
            }
        }

        static async::task<expected<vector<update>, io::error>> _co_idle(tracked_ptr<detail::ClientImpl> c, async::stop_token stop, duration timeout) noexcept {
            using namespace detail;
            ClientImpl& ci = *c;
            if (!ci.has("IDLE") && !ci.has("IMAP4REV2")) {
                // no IDLE: a NOOP after the wait
                const time_point until = sgcl::clock::now() + std::min(timeout, duration(std::chrono::seconds(30)));
                if (stop.stop_possible()) {
                    co_await async::select(stop.channel().on_receive([] {}), async::timeout(until, [] {}));
                } else {
                    co_await async::sleep_until(until);
                }
                {
                    std::lock_guard<std::mutex> g(ci.state_lock);
                    ci.idling = true;
                    ci.idle_updates = vector<update>();
                }
                auto n = co_await _co_simple(c, string("NOOP"));
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.idling = false;
                if (!n) {
                    co_return unexpected(n.error());
                }
                co_return ci.idle_updates;
            }
            Command cmd = ci.command("IDLE");
            cmd.end();
            auto p = std::make_shared<Pending>();
            p->tag = std::string(ClientImpl::tag_of(cmd));
            {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.pending.push_back(p);
                ci.idle_updates = vector<update>();
            }
            auto forget = [&] {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.pending.erase(std::remove(ci.pending.begin(), ci.pending.end(), p), ci.pending.end());
                ci.idling = false;
            };
            auto wg = co_await ci.write_lock.scoped_lock();
            auto rg = co_await ci.read_lock.scoped_lock();
            auto wr = co_await write_all(ci.conn, cmd.text);
            if (!wr) {
                forget();
                co_return unexpected(wr.error());
            }
            std::string buf, cont;
            // the server's "+ idling"
            deadline(ci);
            for (;;) {
                auto one = co_await read_one(ci, buf, cont);
                if (!one) {
                    forget();
                    co_return unexpected(one.error());
                }
                if (*one == Read::continuation || p->done) {
                    break;
                }
            }
            {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.continuations = 0;
            }
            if (p->done) {
                forget();
                co_return unexpected(imap_error(code_of(p->result.code, p->result.status == Status::bad), "IDLE", str(p->result.text)));
            }
            {
                std::lock_guard<std::mutex> g(ci.state_lock);
                ci.idling = true;
            }
            const time_point until = sgcl::clock::now() + timeout;
            async::event done;
            auto sent = std::make_shared<std::atomic<bool>>(false);
            async::go(_idle_watch(c, stop, until, done, sent));
            ci.conn.set_read_deadline(until + std::chrono::seconds(60));
            while (!p->done) {
                auto one = co_await read_one(ci, buf, cont);
                if (!one) {
                    done.set();
                    forget();
                    co_return unexpected(one.error());
                }
                if (*one == Read::untagged) {
                    bool any;
                    {
                        std::lock_guard<std::mutex> g(ci.state_lock);
                        any = !ci.idle_updates.empty();
                    }
                    if (any && !sent->exchange(true)) {
                        std::string d = "DONE\r\n";
                        auto w2 = co_await write_all(ci.conn, d);
                        if (!w2) {
                            done.set();
                            forget();
                            co_return unexpected(w2.error());
                        }
                    }
                }
            }
            done.set();
            vector<update> out;
            {
                std::lock_guard<std::mutex> g(ci.state_lock);
                out = ci.idle_updates;
                ci.idle_updates = vector<update>();
            }
            forget();
            if (p->result.status != Status::ok) {
                co_return unexpected(imap_error(code_of(p->result.code, p->result.status == Status::bad), "IDLE", str(p->result.text)));
            }
            co_return out;
        }

        // --- the rest ----------------------------------------------------------------

        static async::task<expected<imap::namespaces, io::error>> _co_namespaces(tracked_ptr<detail::ClientImpl> c) noexcept {
            using namespace detail;
            auto d = co_await run(c, c->command("NAMESPACE"), string("NAMESPACE"));
            if (!d) {
                co_return unexpected(d.error());
            }
            imap::namespaces out;
            std::string scratch;
            if (!each_untagged(*d, "NAMESPACE", [&](Lexer& x, const Response&) { return parse_namespace(x, out, scratch); })) {
                co_return unexpected(malformed("NAMESPACE"));
            }
            co_return out;
        }

        static async::task<expected<vector<imap::quota>, io::error>> _co_quota(tracked_ptr<detail::ClientImpl> c, string mailbox) noexcept {
            using namespace detail;
            if (!c->has("QUOTA")) {
                co_return unexpected(imap_error(errc::not_supported, "GETQUOTAROOT", string(c->host)));
            }
            Command cmd = c->command("GETQUOTAROOT ");
            cmd.mailbox(mailbox.view());
            auto d = co_await run(c, std::move(cmd), string("GETQUOTAROOT"));
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<imap::quota> out;
            std::string scratch;
            if (!each_untagged(*d, "QUOTA", [&](Lexer& x, const Response&) {
                    imap::quota q;
                    if (!parse_quota(x, q, scratch)) {
                        return false;
                    }
                    out.push_back(q);
                    return true;
                })) {
                co_return unexpected(malformed("GETQUOTAROOT"));
            }
            co_return out;
        }

        static async::task<expected<vector<string>, io::error>> _co_raw(tracked_ptr<detail::ClientImpl> c, string line) noexcept {
            using namespace detail;
            for (char ch : line.view()) {
                if (ch == '\r' || ch == '\n') {
                    co_return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "command", string("a line with CR or LF")));
                }
            }
            Command cmd = c->command(line.view());
            auto d = co_await run(c, std::move(cmd), line);
            if (!d) {
                co_return unexpected(d.error());
            }
            vector<string> out;
            for (const auto& raw : d->untagged) {
                out.push_back(string(raw));
            }
            co_return out;
        }
    };
}
