//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "wire.h"
#include "../envelope.h"
#include "../../../core/aliases.h"
#include "../../../core/function.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../encoding/base64.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// The server's side of an SMTP session as a machine without I/O: bytes
// from the client fed in, the replies to write taken out, and a step that
// says what the driver must do (wait for more, run the handler on a
// message, start TLS, close). The commands of RFC 5321 with ESMTP's
// parameters (SIZE, BODY, SMTPUTF8, RET, ENVID, NOTIFY, ORCPT, AUTH=),
// pipelining (every command of the buffer answered in its turn, the
// replies written together), DATA with its dots, BDAT with LAST, AUTH
// PLAIN and LOGIN, STARTTLS with what came after it in clear text
// thrown away (CVE-2011-0411), and the limits.
namespace sgcl::net::smtp::detail {
    struct MachineSettings {
        string hostname = string("localhost");
        uint64_t max_message_bytes = uint64_t(32) << 20;
        size_t max_recipients = 100;
        size_t max_line_bytes = 2048;
        size_t max_errors = 10;
        size_t max_junk_commands = 100;
        bool tls_offered = false;      // STARTTLS can be taken (a config is there, the session is not TLS yet)
        bool auth_offered = false;     // a callback is there
        bool allow_insecure_auth = false;
        function<bool(const string&, const string&)> auth;
        function<smtp::reply(const smtp::envelope&)> on_sender;
        function<smtp::reply(const smtp::envelope&, const string&)> on_recipient;
    };

    enum class MachineStep : uint8_t {
        more,       // wait for input (after writing out)
        message,    // a message is whole: data() and the envelope for the handler, then message_done()
        starttls,   // write out, then the handshake, then tls_started()
        close       // write out, then close
    };

    class ServerMachine {
    public:
        MachineSettings cfg;
        smtp::envelope env;    // the session's facts (client, tls) and the transaction's
        std::string out;       // replies to write

        // The greeting
        void greet() {
            _reply("220 " + std::string(cfg.hostname.view()) + " ESMTP ready");
        }

        void feed(std::string_view bytes) {
            _in.append(bytes.data(), bytes.size());
        }

        SGCL_INLINE_HOT size_t buffered() const noexcept {
            return _in.size() - _at;
        }

        // The data of the message the step said is whole
        std::string& data() noexcept {
            return _data;
        }

        // What the handler answered (code 0: accepted); the transaction ends
        void message_done(const smtp::reply& r) {
            if (r.code == 0) {
                _reply("250 2.0.0 OK: message accepted");
            } else {
                _reply(_reply_text(r));
            }
            _end_transaction();
        }

        // The handshake of STARTTLS done: the session starts again (RFC 3207
        // §4.2: the client's EHLO, nothing of before kept)
        void tls_started() {
            env.tls = true;
            env.helo = string();
            env.user = string();
            cfg.tls_offered = false;
            _greeted = false;
            _end_transaction();
        }

        // A reply of the driver's own (a timeout, the shutdown): 421, then close
        void shutdown(std::string_view text) {
            _reply(std::string(text));
            _closing = true;
        }

        // As much of the input as there is, in order
        MachineStep step() {
            for (;;) {
                if (_closing) {
                    return MachineStep::close;
                }
                switch (_mode) {
                    case Mode::command: {
                        std::string line;
                        if (!_line(line)) {
                            if (_closing) {
                                return MachineStep::close;
                            }
                            return MachineStep::more;
                        }
                        auto s = _command(line);
                        if (s != MachineStep::more) {
                            return s;
                        }
                        break;
                    }
                    case Mode::data:
                        if (!_data_lines()) {
                            return MachineStep::more;
                        }
                        if (_message_ready) {
                            _message_ready = false;
                            return MachineStep::message;
                        }
                        break;
                    case Mode::bdat:
                        if (!_bdat_bytes()) {
                            return MachineStep::more;
                        }
                        if (_message_ready) {
                            _message_ready = false;
                            return MachineStep::message;
                        }
                        break;
                    case Mode::auth_plain:
                    case Mode::auth_user:
                    case Mode::auth_pass: {
                        std::string line;
                        if (!_line(line)) {
                            return _closing ? MachineStep::close : MachineStep::more;
                        }
                        _auth_line(line);
                        break;
                    }
                    case Mode::skip_line: {
                        // the rest of a line past the limit
                        size_t nl = std::string_view(_in.data() + _at, _in.size() - _at).find('\n');
                        if (nl == std::string_view::npos) {
                            _at = _in.size();
                            _compact();
                            return MachineStep::more;
                        }
                        _at += nl + 1;
                        _mode = _after_skip;
                        break;
                    }
                }
            }
        }

    private:
        enum class Mode : uint8_t { command, data, bdat, auth_plain, auth_user, auth_pass, skip_line };

        std::string _in;
        size_t _at = 0;
        Mode _mode = Mode::command;
        Mode _after_skip = Mode::command;
        bool _greeted = false;
        bool _esmtp = false;
        bool _in_mail = false;
        bool _closing = false;
        bool _message_ready = false;
        bool _too_big = false;
        bool _bdat_bad = false;      // a BDAT that is answered with a refusal after its bytes
        bool _bdat_last = false;
        bool _bdat_started = false;  // a chunk of this transaction taken
        bool _binary = false;        // BODY=BINARYMIME: BDAT only
        uint64_t _bdat_left = 0;
        size_t _errors = 0;
        size_t _junk = 0;
        std::string _data;
        std::string _auth_user;

        // A client's bytes in a reply: no control that could end its line
        static std::string _printable(std::string s) {
            for (char& c : s) {
                if (uint8_t(c) < 0x20 || c == 0x7F) {
                    c = '?';
                }
            }
            return s;
        }

        void _reply(const std::string& line) {
            out += line;
            out += "\r\n";
        }

        static std::string _reply_text(const smtp::reply& r) {
            std::string code = std::to_string(r.code);
            std::string out;
            auto lines = r.text.split('\n');
            size_t n = 0;
            for (auto& l : lines) {
                (void)l;
                ++n;
            }
            if (n == 0) {
                out = code;
                if (!r.enhanced.empty()) {
                    out += ' ';
                    out += r.enhanced.view();
                }
                return out;
            }
            size_t i = 0;
            for (auto& l : lines) {
                out += code;
                out += ++i < n ? '-' : ' ';
                if (!r.enhanced.empty()) {
                    out += r.enhanced.view();
                    out += ' ';
                }
                for (char c : l.view()) {
                    out += (c == '\r') ? ' ' : c;
                }
                if (i < n) {
                    out += "\r\n";
                }
            }
            return out;
        }

        void _compact() {
            if (_at == _in.size()) {
                _in.clear();
                _at = 0;
            } else if (_at > 65536 && _at * 2 > _in.size()) {
                _in.erase(0, _at);
                _at = 0;
            }
        }

        // A command line; a line past the limit answered and skipped
        bool _line(std::string& line) {
            std::string_view v(_in.data() + _at, _in.size() - _at);
            size_t nl = v.find('\n');
            if (nl == std::string_view::npos) {
                if (v.size() > cfg.max_line_bytes + 1) {   // the line and the CR of its end
                    _error("500 5.5.2 Line too long");
                    _after_skip = _mode;
                    _mode = Mode::skip_line;
                    _at = _in.size();
                    _compact();
                }
                return false;
            }
            size_t end = nl;
            if (end && v[end - 1] == '\r') {
                --end;
            }
            if (end > cfg.max_line_bytes) {
                _at += nl + 1;
                _error("500 5.5.2 Line too long");
                return !_closing && _line(line);   // the next line, if one waits
            }
            line.assign(v.data(), end);
            _at += nl + 1;
            _compact();
            return true;
        }

        void _error(const std::string& line) {
            _reply(line);
            if (++_errors >= cfg.max_errors) {
                _reply("421 4.7.0 Too many errors, closing");
                _closing = true;
            }
        }

        void _junk_command() {
            if (++_junk > cfg.max_junk_commands) {
                _reply("421 4.7.0 Too many commands without a message, closing");
                _closing = true;
            }
        }

        void _end_transaction() {
            _in_mail = false;
            _too_big = false;
            _binary = false;
            _bdat_started = false;
            _data.clear();
            string helo = env.helo, user = env.user;
            endpoint client = env.client;
            bool tls = env.tls;
            env = smtp::envelope();
            env.helo = helo;
            env.user = user;
            env.client = client;
            env.tls = tls;
        }

        // "FROM:<a@b> SIZE=1" after the verb: the path and the parameters;
        // false when it is not one. The source route of RFC 5321 §4.1.2's
        // obsolete form taken off; "MAIL FROM: <a@b>" (a space) taken too.
        static bool _path(std::string_view arg, std::string_view prefix, std::string& path, std::string_view& params) {
            if (!smtp_istarts(arg, prefix)) {
                return false;
            }
            arg.remove_prefix(prefix.size());
            while (!arg.empty() && arg.front() == ' ') {
                arg.remove_prefix(1);
            }
            if (arg.empty() || arg.front() != '<') {
                return false;
            }
            // the closing '>' outside a quoted local part
            size_t i = 1;
            bool quoted = false;
            for (; i < arg.size(); ++i) {
                char c = arg[i];
                if (quoted && c == '\\') {
                    ++i;
                    continue;
                }
                if (c == '"') {
                    quoted = !quoted;
                } else if (c == '>' && !quoted) {
                    break;
                }
            }
            if (i >= arg.size()) {
                return false;
            }
            std::string_view p = arg.substr(1, i - 1);
            if (!p.empty() && p.front() == '@') {
                size_t colon = p.find(':');
                if (colon == std::string_view::npos) {
                    return false;
                }
                p.remove_prefix(colon + 1);
            }
            path.assign(p);
            params = arg.substr(i + 1);
            if (!params.empty() && params.front() != ' ') {
                return false;
            }
            while (!params.empty() && params.front() == ' ') {
                params.remove_prefix(1);
            }
            return true;
        }

        // A mailbox of a path: local@domain, or the "Postmaster" of RCPT
        static bool _mailbox(std::string_view a, bool allow_postmaster) {
            if (allow_postmaster && smtp_iequal(a, "postmaster")) {
                return true;
            }
            size_t at = a.rfind('@');
            if (at == std::string_view::npos || at == 0 || at + 1 == a.size()) {
                return false;
            }
            for (char c : a) {
                uint8_t u = uint8_t(c);
                if (u < 0x20 || u == 0x7F || c == ' ' || c == '<' || c == '>') {
                    if (c == ' ' && a.front() == '"') {
                        continue;
                    }
                    return false;
                }
            }
            return true;
        }

        MachineStep _command(std::string_view line) {
            size_t sp = line.find(' ');
            std::string verb = smtp_uppered(line.substr(0, sp));
            std::string_view arg = sp == std::string_view::npos ? std::string_view() : line.substr(sp + 1);
            if (verb == "EHLO" || verb == "HELO") {
                while (!arg.empty() && arg.back() == ' ') {
                    arg.remove_suffix(1);
                }
                if (arg.empty()) {
                    _error("501 5.5.4 " + verb + " needs a domain or an address");
                    return MachineStep::more;
                }
                _end_transaction();
                env.helo = string(arg);
                _greeted = true;
                _esmtp = verb == "EHLO";
                _junk_command();
                if (!_esmtp) {
                    _reply("250 " + std::string(cfg.hostname.view()));
                    return MachineStep::more;
                }
                std::string r = "250-" + std::string(cfg.hostname.view()) + " greets " + std::string(arg.substr(0, 255));
                for (char& c : r) {
                    if (uint8_t(c) < 0x20) {
                        c = ' ';
                    }
                }
                r += "\r\n250-PIPELINING\r\n250-SIZE " + std::to_string(cfg.max_message_bytes);
                r += "\r\n250-8BITMIME\r\n250-SMTPUTF8\r\n250-ENHANCEDSTATUSCODES\r\n250-CHUNKING\r\n250-BINARYMIME\r\n250-DSN";
                if (cfg.tls_offered && !env.tls) {
                    r += "\r\n250-STARTTLS";
                }
                if (_auth_allowed()) {
                    r += "\r\n250-AUTH PLAIN LOGIN";
                }
                r += "\r\n250 HELP";
                _reply(r);
                return MachineStep::more;
            }
            if (verb == "NOOP") {
                _reply("250 2.0.0 OK");
                _junk_command();
                return MachineStep::more;
            }
            if (verb == "QUIT") {
                _reply("221 2.0.0 Bye");
                _closing = true;
                return MachineStep::close;
            }
            if (verb == "RSET") {
                if (!arg.empty()) {
                    _error("501 5.5.4 RSET takes no argument");
                    return MachineStep::more;
                }
                _end_transaction();
                _reply("250 2.0.0 OK");
                _junk_command();
                return MachineStep::more;
            }
            if (verb == "VRFY") {
                _reply("252 2.5.2 Cannot VRFY the user, but will accept the message and attempt delivery");
                _junk_command();
                return MachineStep::more;
            }
            if (verb == "EXPN") {
                _error("502 5.5.1 EXPN not implemented");
                return MachineStep::more;
            }
            if (verb == "HELP") {
                _reply("214 2.0.0 See RFC 5321");
                _junk_command();
                return MachineStep::more;
            }
            if (verb == "STARTTLS") {
                if (!cfg.tls_offered || env.tls) {
                    _error("502 5.5.1 STARTTLS not offered");
                    return MachineStep::more;
                }
                if (!arg.empty()) {
                    _error("501 5.5.4 STARTTLS takes no argument");
                    return MachineStep::more;
                }
                if (!_greeted) {
                    _error("503 5.5.1 EHLO first");
                    return MachineStep::more;
                }
                _reply("220 2.0.0 Ready to start TLS");
                // what came pipelined after STARTTLS in clear text is not a
                // command of the encrypted session (CVE-2011-0411)
                _in.clear();
                _at = 0;
                return MachineStep::starttls;
            }
            if (verb == "AUTH") {
                _auth(arg);
                return MachineStep::more;
            }
            if (verb == "MAIL") {
                _mail(arg);
                return MachineStep::more;
            }
            if (verb == "RCPT") {
                _rcpt(arg);
                return MachineStep::more;
            }
            if (verb == "DATA") {
                if (!arg.empty()) {
                    _error("501 5.5.4 DATA takes no argument");
                    return MachineStep::more;
                }
                if (!_in_mail) {
                    _error("503 5.5.1 MAIL first");
                    return MachineStep::more;
                }
                if (env.to.empty()) {
                    _error("554 5.5.1 No valid recipients");
                    return MachineStep::more;
                }
                if (_binary || _bdat_started) {
                    _error("503 5.5.1 BODY=BINARYMIME or BDAT taken: no DATA");
                    return MachineStep::more;
                }
                _reply("354 Start mail input; end with <CRLF>.<CRLF>");
                _mode = Mode::data;
                _data.clear();
                _too_big = false;
                return MachineStep::more;
            }
            if (verb == "BDAT") {
                // BDAT size [LAST]
                std::string_view a = arg;
                uint64_t n = 0;
                size_t digits = 0;
                while (digits < a.size() && a[digits] >= '0' && a[digits] <= '9' && digits < 19) {
                    n = n * 10 + uint64_t(a[digits] - '0');
                    ++digits;
                }
                std::string_view rest = a.substr(digits);
                bool last = false;
                if (smtp_iequal(rest, " LAST")) {
                    last = true;
                } else if (!rest.empty() || digits == 0) {
                    _error("501 5.5.4 BDAT takes a size and LAST");
                    return MachineStep::more;   // the bytes cannot be counted: they read as commands
                }
                _bdat_left = n;
                _bdat_last = last;
                _bdat_bad = false;
                if (!_in_mail) {
                    _bdat_bad = true;
                } else if (env.to.empty()) {
                    _bdat_bad = true;
                }
                _mode = Mode::bdat;
                return MachineStep::more;
            }
            if (line.empty()) {
                _error("500 5.5.2 Empty command");
                return MachineStep::more;
            }
            _error("500 5.5.1 Command not recognized");
            return MachineStep::more;
        }

        bool _auth_allowed() const noexcept {
            return cfg.auth_offered && (env.tls || cfg.allow_insecure_auth);
        }

        void _mail(std::string_view arg) {
            if (!_greeted) {
                _error("503 5.5.1 EHLO or HELO first");
                return;
            }
            if (_in_mail) {
                _error("503 5.5.1 A transaction is open: RSET first");
                return;
            }
            std::string path;
            std::string_view params;
            if (!_path(arg, "FROM:", path, params)) {
                _error("501 5.5.4 Syntax: MAIL FROM:<address>");
                return;
            }
            if (!path.empty() && !_mailbox(path, false)) {
                _error("553 5.1.7 Bad sender address syntax");
                return;
            }
            smtp::envelope e = env;
            e.from = string(path);
            e.to = vector<string>();
            // the parameters
            while (!params.empty()) {
                size_t sp = params.find(' ');
                std::string_view p = params.substr(0, sp);
                params = sp == std::string_view::npos ? std::string_view() : params.substr(sp + 1);
                if (p.empty()) {
                    continue;
                }
                size_t eq = p.find('=');
                std::string key = smtp_uppered(p.substr(0, eq));
                std::string_view value = eq == std::string_view::npos ? std::string_view() : p.substr(eq + 1);
                if (!_esmtp) {
                    _error("555 5.5.4 Parameters need EHLO");
                    return;
                }
                if (key == "SIZE") {
                    uint64_t n = 0;
                    if (value.empty() || value.size() > 19) {
                        _error("501 5.5.4 Bad SIZE");
                        return;
                    }
                    for (char c : value) {
                        if (c < '0' || c > '9') {
                            _error("501 5.5.4 Bad SIZE");
                            return;
                        }
                        n = n * 10 + uint64_t(c - '0');
                    }
                    if (cfg.max_message_bytes && n > cfg.max_message_bytes) {
                        _error("552 5.3.4 Message size exceeds the limit");
                        return;
                    }
                    e.size = n;
                } else if (key == "BODY") {
                    std::string b = smtp_uppered(value);
                    if (b != "7BIT" && b != "8BITMIME" && b != "BINARYMIME") {
                        _error("501 5.5.4 Bad BODY");
                        return;
                    }
                    e.body = string(b);
                } else if (key == "SMTPUTF8" && value.empty()) {
                    e.smtputf8 = true;
                } else if (key == "RET") {
                    std::string r = smtp_uppered(value);
                    if (r != "FULL" && r != "HDRS") {
                        _error("501 5.5.4 Bad RET");
                        return;
                    }
                    e.ret = string(r);
                } else if (key == "ENVID" && !value.empty() && value.size() <= 100) {
                    e.envid = string(xtext_decode(value));
                } else if (key == "AUTH") {
                    // RFC 4954 §5: the identity of the submitter, taken and not used
                } else {
                    _error("555 5.5.4 Unknown MAIL parameter " + _printable(key.substr(0, 32)));
                    return;
                }
            }
            if (!smtp_ascii(path) && !e.smtputf8) {
                _error("553 5.6.7 An address past ASCII needs SMTPUTF8");
                return;
            }
            if (cfg.on_sender) {
                smtp::reply r = cfg.on_sender(e);
                if (r.code != 0 && !r.positive()) {
                    _reply(_reply_text(r));
                    return;
                }
                env = std::move(e);
                _in_mail = true;
                _binary = env.body == "BINARYMIME";
                _junk = 0;
                _reply(r.code ? _reply_text(r) : std::string("250 2.1.0 Sender OK"));
                return;
            }
            env = std::move(e);
            _in_mail = true;
            _binary = env.body == "BINARYMIME";
            _junk = 0;
            _reply("250 2.1.0 Sender OK");
        }

        void _rcpt(std::string_view arg) {
            if (!_in_mail) {
                _error("503 5.5.1 MAIL first");
                return;
            }
            std::string path;
            std::string_view params;
            if (!_path(arg, "TO:", path, params)) {
                _error("501 5.5.4 Syntax: RCPT TO:<address>");
                return;
            }
            if (path.empty() || !_mailbox(path, true)) {
                _error("553 5.1.3 Bad recipient address syntax");
                return;
            }
            if (!smtp_ascii(path) && !env.smtputf8) {
                _error("553 5.6.7 An address past ASCII needs SMTPUTF8");
                return;
            }
            if (env.to.size() >= cfg.max_recipients) {
                _reply("452 4.5.3 Too many recipients");
                return;
            }
            std::string notify, orcpt;
            while (!params.empty()) {
                size_t sp = params.find(' ');
                std::string_view p = params.substr(0, sp);
                params = sp == std::string_view::npos ? std::string_view() : params.substr(sp + 1);
                if (p.empty()) {
                    continue;
                }
                size_t eq = p.find('=');
                std::string key = smtp_uppered(p.substr(0, eq));
                std::string_view value = eq == std::string_view::npos ? std::string_view() : p.substr(eq + 1);
                if (key == "NOTIFY" && !value.empty()) {
                    notify = smtp_uppered(value);
                    bool ok = notify == "NEVER";
                    if (!ok) {
                        ok = true;
                        std::string_view n = notify;
                        while (!n.empty()) {
                            size_t c = n.find(',');
                            std::string_view w = n.substr(0, c);
                            ok &= w == "SUCCESS" || w == "FAILURE" || w == "DELAY";
                            n = c == std::string_view::npos ? std::string_view() : n.substr(c + 1);
                        }
                    }
                    if (!ok) {
                        _error("501 5.5.4 Bad NOTIFY");
                        return;
                    }
                } else if (key == "ORCPT" && value.find(';') != std::string_view::npos) {
                    size_t semi = value.find(';');
                    orcpt = std::string(value.substr(0, semi + 1)) + xtext_decode(value.substr(semi + 1));
                } else {
                    _error("555 5.5.4 Unknown RCPT parameter " + _printable(key.substr(0, 32)));
                    return;
                }
            }
            string rcpt(path);
            if (cfg.on_recipient) {
                smtp::reply r = cfg.on_recipient(env, rcpt);
                if (r.code != 0 && !r.positive()) {
                    _reply(_reply_text(r));
                    return;
                }
                if (r.code) {
                    _add_rcpt(rcpt, notify, orcpt);
                    _reply(_reply_text(r));
                    return;
                }
            }
            _add_rcpt(rcpt, notify, orcpt);
            _reply("250 2.1.5 Recipient OK");
        }

        void _add_rcpt(const string& rcpt, const std::string& notify, const std::string& orcpt) {
            const size_t i = env.to.size();
            env.to.push_back(rcpt);
            if (!notify.empty() || !env.notify.empty()) {
                env.notify.resize(i);
                env.notify.push_back(string(notify));
            }
            if (!orcpt.empty() || !env.orcpt.empty()) {
                env.orcpt.resize(i);
                env.orcpt.push_back(string(orcpt));
            }
        }

        // DATA's lines up to the lone '.': the dots of RFC 5321 §4.5.2 taken
        // off, line breaks kept as CRLF; past the limit the bytes read and
        // dropped, the message refused at its end (RFC 1870 §6.3); true
        // when the end came
        bool _data_lines() {
            for (;;) {
                std::string_view v(_in.data() + _at, _in.size() - _at);
                size_t nl = v.find('\n');
                if (nl == std::string_view::npos) {
                    // a line not ended yet: what of it cannot be the end is
                    // taken now, a CR at its end kept for the LF it may be half of
                    size_t skip = 0;
                    if (_data_line_start) {
                        if (v.empty()) {
                            return false;
                        }
                        if (v[0] == '.') {
                            if (v.size() < 2 || (v[1] == '\r' && v.size() < 3)) {
                                return false;   // ".", ".\r": the end, perhaps
                            }
                            skip = 1;
                        }
                    }
                    size_t take = v.size() - (v.back() == '\r' ? 1 : 0);
                    if (take > skip) {
                        _put(v.substr(skip, take - skip));
                        _at += take;
                        _data_line_start = false;
                        _compact();
                    }
                    return false;
                }
                std::string_view line = v.substr(0, nl);
                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }
                _at += nl + 1;
                if (_data_line_start && line == ".") {
                    _compact();
                    _mode = Mode::command;
                    _data_line_start = true;
                    if (_too_big) {
                        _reply("552 5.3.4 Message size exceeds the limit");
                        _end_transaction();
                        return true;
                    }
                    _message_ready = true;
                    return true;
                }
                if (_data_line_start && !line.empty() && line.front() == '.') {
                    line.remove_prefix(1);
                }
                _put(line);
                _put("\r\n");
                _data_line_start = true;
            }
        }

        bool _data_line_start = true;

        void _put(std::string_view s) {
            if (_too_big) {
                return;
            }
            if (cfg.max_message_bytes && _data.size() + s.size() > cfg.max_message_bytes) {
                _too_big = true;
                _data.clear();
                return;
            }
            _data.append(s.data(), s.size());
        }

        bool _bdat_bytes() {
            size_t k = size_t(std::min<uint64_t>(_bdat_left, _in.size() - _at));
            if (!_bdat_bad) {
                _put(std::string_view(_in.data() + _at, k));
            }
            _at += k;
            _bdat_left -= k;
            _compact();
            if (_bdat_left) {
                return false;
            }
            _mode = Mode::command;
            if (_bdat_bad) {
                if (!_in_mail) {
                    _error("503 5.5.1 MAIL first");
                } else {
                    _error("554 5.5.1 No valid recipients");
                }
                return true;
            }
            _bdat_started = true;
            if (!_bdat_last) {
                _reply("250 2.0.0 Chunk received");
                return true;
            }
            if (_too_big) {
                _reply("552 5.3.4 Message size exceeds the limit");
                _end_transaction();
                return true;
            }
            _message_ready = true;
            return true;
        }

        void _auth(std::string_view arg) {
            if (!_auth_allowed()) {
                _error(cfg.auth_offered ? "538 5.7.11 Encryption required for requested authentication mechanism" : "502 5.5.1 AUTH not offered");
                return;
            }
            if (!_greeted || !_esmtp) {
                _error("503 5.5.1 EHLO first");
                return;
            }
            if (!env.user.empty()) {
                _error("503 5.5.1 Already authenticated");
                return;
            }
            if (_in_mail) {
                _error("503 5.5.1 AUTH not inside a transaction");
                return;
            }
            size_t sp = arg.find(' ');
            std::string mech = smtp_uppered(arg.substr(0, sp));
            std::string_view initial = sp == std::string_view::npos ? std::string_view() : arg.substr(sp + 1);
            if (mech == "PLAIN") {
                if (initial.empty()) {
                    _reply("334 ");
                    _mode = Mode::auth_plain;
                    return;
                }
                _plain(initial);
            } else if (mech == "LOGIN") {
                if (!initial.empty()) {
                    auto u = _b64(initial);
                    if (!u) {
                        _error("501 5.5.2 Cannot decode the response");
                        return;
                    }
                    _auth_user = *u;
                    _reply("334 UGFzc3dvcmQ6");
                    _mode = Mode::auth_pass;
                    return;
                }
                _reply("334 VXNlcm5hbWU6");
                _mode = Mode::auth_user;
            } else {
                _error("504 5.5.4 Unrecognized authentication mechanism");
            }
        }

        static optional<std::string> _b64(std::string_view s) {
            if (s == "=") {
                return std::string();
            }
            auto d = encoding::base64::standard.decode(string(s));
            if (!d) {
                return nullopt;
            }
            return std::string(reinterpret_cast<const char*>(d->data()), d->size());
        }

        void _check(const std::string& user, const std::string& password) {
            _mode = Mode::command;
            if (cfg.auth && cfg.auth(string(user), string(password))) {
                env.user = string(user);
                _reply("235 2.7.0 Authentication successful");
                return;
            }
            _error("535 5.7.8 Authentication credentials invalid");
        }

        void _plain(std::string_view b64) {
            auto t = _b64(b64);
            if (!t) {
                _mode = Mode::command;
                _error("501 5.5.2 Cannot decode the response");
                return;
            }
            // authzid NUL authcid NUL passwd (RFC 4616 §2)
            size_t z1 = t->find('\0');
            size_t z2 = z1 == std::string::npos ? std::string::npos : t->find('\0', z1 + 1);
            if (z2 == std::string::npos) {
                _mode = Mode::command;
                _error("501 5.5.2 Malformed PLAIN response");
                return;
            }
            std::string authz = t->substr(0, z1);
            std::string user = t->substr(z1 + 1, z2 - z1 - 1);
            std::string pass = t->substr(z2 + 1);
            if (!authz.empty() && authz != user) {
                _mode = Mode::command;
                _error("535 5.7.8 Authorization identity not allowed");
                return;
            }
            _check(user, pass);
        }

        void _auth_line(std::string_view line) {
            if (line == "*") {
                _mode = Mode::command;
                _error("501 5.0.0 Authentication cancelled");
                return;
            }
            if (_mode == Mode::auth_plain) {
                _plain(line);
            } else if (_mode == Mode::auth_user) {
                auto u = _b64(line);
                if (!u) {
                    _mode = Mode::command;
                    _error("501 5.5.2 Cannot decode the response");
                    return;
                }
                _auth_user = *u;
                _reply("334 UGFzc3dvcmQ6");
                _mode = Mode::auth_pass;
            } else {
                auto p = _b64(line);
                if (!p) {
                    _mode = Mode::command;
                    _error("501 5.5.2 Cannot decode the response");
                    return;
                }
                _check(_auth_user, *p);
                _auth_user.clear();
            }
        }
    };
}
