//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::pop3 on any bytes; the first byte picks what the rest is:
//   0  the lines of a client to the server's session machine (a maildrop of
//      three messages in a memory_backend, alice logged in or not by the
//      second byte): every reply is CRLF lines, each status line +OK, -ERR
//      or "+ ", and the client's reader takes the whole output as the
//      replies it is (status lines and multi-line bodies in turn)
//   1  a server's bytes to the client's reader: lines and multi-line
//      bodies taken until nothing whole is left; never past the buffer
//   2  a message through the multi-line writer and the reader back: the
//      text with its line breaks made CRLF and a CRLF at its end
// The readers run on the input's own bytes or a malloc'd block of exactly
// them, never a managed copy: a read past the end is ASan's to see.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/pop3_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace pd = sgcl::net::pop3::detail;

    // The bytes in a malloc'd block of exactly their size, never a managed
    // or a std::string's copy: a read past their end is ASan's to see
    class Exact {
    public:
        explicit Exact(std::string_view s)
        : _p(static_cast<char*>(std::malloc(s.size()))), _n(s.size()) {
            std::copy_n(s.data(), s.size(), _p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(_p);
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // A reader over bytes held in memory: the wire's buffer filled by hand
    struct Reader {
        tracked_ptr<pd::Pop3Wire> w = make_tracked<pd::Pop3Wire>(net::connection());

        explicit Reader(std::string_view bytes) {
            w->buf.assign(bytes.data(), bytes.size());
        }
    };

    void session(std::string_view text, uint8_t flags) {
        net::imap::memory_backend mail;
        mail.add_user("alice", "secret");
        (void)mail.append("alice", "INBOX", "From: a@example.com\r\nSubject: one\r\n\r\nbody\r\n.dot\r\n");
        (void)mail.append("alice", "INBOX", "From: b@example.com\nSubject: lf\n\n..two\n");
        (void)mail.append("alice", "INBOX", "");
        tracked_ptr cfg = make_tracked<pd::Pop3ServerSettings>();
        net::imap::backend b(mail);
        cfg->backend = net::imap::detail::BackendAccess::get(b);
        cfg->greeting = "ready";
        cfg->hostname = "fuzz.example";
        cfg->on_error = [](const string&) {};
        if (flags & 2) {
            cfg->apop_secret = [](const string&) -> optional<string> { return string("secret"); };
        }
        tracked_ptr server = make_tracked<pd::Pop3ServerImpl>();
        tracked_ptr s = make_tracked<pd::Pop3Session>(cfg, server, false);
        s->greet();
        if (flags & 1) {
            (void)s->command("USER alice");
            (void)s->command("PASS secret");
        }
        size_t at = 0;
        while (at < text.size()) {
            size_t nl = text.find('\n', at);
            std::string_view line = text.substr(at, nl == std::string_view::npos ? std::string_view::npos : nl - at);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            at = nl == std::string_view::npos ? text.size() : nl + 1;
            Exact one(line);
            auto step = s->command(one.view());
            if (step != pd::Pop3Session::Step::more) {
                break;
            }
        }
        s->release();
        // the output as the client reads it: a status line, then a body
        // when the line announced one
        check(s->out.empty() || s->out.substr(s->out.size() - 2) == "\r\n");
        Reader r(s->out);
        std::string line;
        bool too_long = false;
        while (r.w->take_line(line, 1 << 20, too_long)) {
            bool status = line.rfind("+OK", 0) == 0 || line.rfind("-ERR", 0) == 0 || line == "+ ";
            check(status);
            auto ends = [&](std::string_view e) { return line.size() >= e.size() && std::string_view(line).substr(line.size() - e.size()) == e; };
            bool octets = line.rfind("+OK ", 0) == 0 && ends(" octets") && line.find_first_not_of("0123456789", 4) == line.size() - 7;
            bool body = ends("follows") || ends("follow") || octets;
            if (body && line.rfind("+OK", 0) == 0) {
                std::string text_out;
                check(r.w->take_multiline(text_out, too_long, size_t(1) << 30));
            }
        }
        check(r.w->buffered() == 0);
    }

    void reader(std::string_view bytes) {
        // the line readers on the input's own bytes (bytes: libFuzzer's data, its end the input's)
        std::string out;
        bool too_long = false;
        size_t at = 0;
        for (int i = 0; i < 64; ++i) {
            std::string_view rest = bytes.substr(at);
            size_t n = (i & 1) ? pd::pop3_take_multiline(rest, out, too_long, 1 << 16) : pd::pop3_take_line(rest, out, 1 << 16, too_long);
            check(n <= rest.size());
            at += n;
            if (n == 0 && (i & 1) == 0) {
                break;
            }
        }
        // and through the wire's buffer, as the client reads
        Reader r(bytes);
        while (r.w->take_line(out, 1 << 16, too_long)) {
        }
        check(r.w->at <= r.w->buf.size());
    }

    void roundtrip(std::string_view text) {
        std::string wire;
        pd::pop3_put_multiline(wire, text);
        Exact w(wire);
        std::string back;
        bool too_long = false;
        check(pd::pop3_take_multiline(w.view(), back, too_long, size_t(1) << 30) == wire.size());
        std::string want;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
                want += '\r';
            }
            want += text[i];
        }
        if (!want.empty() && (want.size() < 2 || want.substr(want.size() - 2) != "\r\n")) {
            want += "\r\n";
        }
        // a bare CR before the end is kept as it is, so compare where the text has none
        if (text.find('\r') == std::string_view::npos) {
            check(back == want);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    std::string_view text(reinterpret_cast<const char*>(data + 2), size - 2);
    switch (data[0] % 3) {
        case 0: session(text, data[1]); break;
        case 1: reader(text); break;
        case 2: roundtrip(text); break;
    }
    return 0;
}
