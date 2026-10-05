//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../envelope.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../../async/coroutine.h"
#include "../../../core/aliases.h"
#include "../../../core/array.h"
#include "../../../core/clock.h"
#include "../../../core/detail/bytes.h"
#include "../../../core/make_tracked.h"
#include "../../../core/string.h"
#include "../../../core/tracked_ptr.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// What both sides of SMTP read and write by: a connection's bytes as
// lines, a reply's lines as a reply, a command's words, the encodings of
// RFC 3461 (xtext) and the dot-stuffing of RFC 5321 §4.5.2
namespace sgcl::net::smtp::detail {
    using namespace sgcl::detail;
    using net::detail::fail;
    using net::detail::net_error;
    using net::detail::no_deadline_at_max;

    using SmtpBlock = array<std::byte, 8192>;

    SGCL_INLINE_HOT constexpr char smtp_upper(char c) noexcept {
        return c >= 'a' && c <= 'z' ? char(c - 32) : c;
    }

    inline bool smtp_iequal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (smtp_upper(a[i]) != smtp_upper(b[i])) {
                return false;
            }
        }
        return true;
    }

    inline bool smtp_istarts(std::string_view s, std::string_view prefix) noexcept {
        return s.size() >= prefix.size() && smtp_iequal(s.substr(0, prefix.size()), prefix);
    }

    inline std::string smtp_uppered(std::string_view s) {
        std::string out(s);
        for (char& c : out) {
            c = smtp_upper(c);
        }
        return out;
    }

    // A connection read as lines: a block of 8 KB read at a time, the bytes
    // past the line kept for the next (a pipelined command, the data of
    // BDAT); buffered() says how many wait, which is what the server's
    // STARTTLS checks (CVE-2011-0411: what came in clear text before the
    // handshake is not a command of the encrypted session)
    struct SmtpWire {
        net::connection c;
        tracked_ptr<SmtpBlock> block = make_tracked<SmtpBlock>();
        std::string buf;
        size_t at = 0;

        explicit SmtpWire(const net::connection& conn) noexcept
        : c(conn) {
        }

        SGCL_INLINE_HOT size_t buffered() const noexcept {
            return buf.size() - at;
        }

        void discard() noexcept {
            buf.clear();
            at = 0;
        }

        void _compact() {
            if (at == buf.size()) {
                buf.clear();
                at = 0;
            } else if (at > 65536 && at * 2 > buf.size()) {
                buf.erase(0, at);
                at = 0;
            }
        }

        // More bytes from the connection; 0 at its end
        async::task<expected<size_t, io::error>> fill() noexcept {
            _compact();
            auto r = co_await c.async_read(slice<byte>(block->data(), block->size()));
            if (r && *r) {
                buf.append(reinterpret_cast<const char*>(block->data()), *r);
            }
            co_return r;
        }

        // A read without a frame of its own (net's try_read, as http's
        // Wire::try_fill): the bytes there now appended (done), or the
        // readiness to wait for, or slow (the waiting read must serve).
        // The caller loops:
        //   for (;;) { auto t = w.try_fill(); if (t.done) { r = t.result; break; }
        //              if (t.slow) { r = co_await w.fill(); break; }
        //              if (auto ready = co_await t.ready; !ready) { r = fail(ready); break; } }
        struct FillTry {
            expected<size_t, io::error> result = size_t(0);
            bool done = false;
            bool slow = false;
            net::detail::readiness ready;
        };

        FillTry try_fill() {
            FillTry t;
            _compact();
            auto& impl = net::detail::ConnectionAccess::impl(c);
            auto r = impl.try_read(slice<byte>(block->data(), block->size()), t.slow);
            if (t.slow) {
                return t;
            }
            if (!r) {
                t.result = fail(r);
                t.done = true;
                return t;
            }
            if (*r) {
                buf.append(reinterpret_cast<const char*>(block->data()), **r);
                t.result = **r;
                t.done = true;
                return t;
            }
            t.ready = impl.raw_readable();
            return t;
        }

        // A line from the buffer without its CRLF (a bare LF taken too);
        // false when no whole line waits. A line past max is too_long: the
        // buffer holds max bytes with no end of line in them.
        bool take_line(std::string& line, size_t max, bool& too_long) {
            too_long = false;
            std::string_view v(buf.data() + at, buf.size() - at);
            size_t nl = v.find('\n');
            if (nl == std::string_view::npos) {
                too_long = v.size() > max;
                return false;
            }
            size_t end = nl;
            if (end > 0 && v[end - 1] == '\r') {
                --end;
            }
            if (end > max) {
                too_long = true;
                return false;
            }
            line.assign(v.data(), end);
            at += nl + 1;
            return true;
        }

        // A line, read from the connection as it comes; nullopt at the
        // connection's end before a whole line
        async::task<expected<optional<std::string>, io::error>> read_line(size_t max, const char* what) noexcept {
            std::string line;
            for (;;) {
                bool too_long = false;
                if (take_line(line, max, too_long)) {
                    co_return optional<std::string>(std::move(line));
                }
                if (too_long) {
                    co_return fail(net_error(errc::malformed_smtp_reply, what, string("line too long")));
                }
                auto r = co_await fill();
                if (!r) {
                    co_return fail(r);
                }
                if (*r == 0) {
                    co_return optional<std::string>();
                }
            }
        }

        // n bytes exactly into out (the data of BDAT); false at the end
        // before them
        async::task<expected<bool, io::error>> read_exact(uint64_t n, std::string& out, uint64_t keep_max) noexcept {
            while (n) {
                if (buffered() == 0) {
                    auto r = co_await fill();
                    if (!r) {
                        co_return fail(r);
                    }
                    if (*r == 0) {
                        co_return false;
                    }
                }
                size_t k = size_t(std::min<uint64_t>(n, buffered()));
                if (out.size() < keep_max) {
                    out.append(buf.data() + at, size_t(std::min<uint64_t>(k, keep_max - out.size())));
                }
                at += k;
                n -= k;
            }
            co_return true;
        }
    };

    // A reply read line by line (RFC 5321 §4.2): "250-first", "250 last",
    // every line of one code; the enhanced code of RFC 2034 at the start
    // of a line's text taken out when its class is the code's
    struct ReplyParser {
        smtp::reply r;
        std::string text;
        size_t lines = 0;
        bool done = false;
        string error;

        static constexpr size_t MaxLines = 1000;

        static bool is_enhanced(std::string_view s, int klass) noexcept {
            // class "." subject "." detail: 1 digit, 1-3 digits, 1-3 digits
            if (s.size() < 5 || s[0] - '0' != klass || s[1] != '.') {
                return false;
            }
            size_t i = 2;
            size_t n = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                ++i;
                ++n;
            }
            if (n < 1 || n > 3 || i >= s.size() || s[i] != '.') {
                return false;
            }
            ++i;
            n = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                ++i;
                ++n;
            }
            return n >= 1 && n <= 3 && i == s.size();
        }

        // false: the line breaks the grammar (error set)
        bool feed(std::string_view line) {
            if (line.size() < 3 || line[0] < '2' || line[0] > '5' || line[1] < '0' || line[1] > '9' || line[2] < '0' || line[2] > '9') {
                error = "not a reply line";
                return false;
            }
            int code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
            if (lines && code != r.code) {
                error = "lines of different codes";
                return false;
            }
            r.code = code;
            bool last = true;
            std::string_view rest;
            if (line.size() > 3) {
                if (line[3] == '-') {
                    last = false;
                } else if (line[3] != ' ') {
                    error = "no space after the code";
                    return false;
                }
                rest = line.substr(4);
            }
            if (++lines > MaxLines) {
                error = "too many lines";
                return false;
            }
            size_t sp = rest.find(' ');
            std::string_view first = rest.substr(0, sp);
            if (is_enhanced(first, code / 100)) {
                if (r.enhanced.empty()) {
                    r.enhanced = string(first);
                }
                rest = sp == std::string_view::npos ? std::string_view() : rest.substr(sp + 1);
            }
            if (lines > 1) {
                text += '\n';
            }
            text += rest;
            if (last) {
                r.text = string(text);
                done = true;
            }
            return true;
        }
    };

    // The reply as the path of an error: "550 5.1.1 No such user"
    inline io::error reply_error(errc e, const string& op, const smtp::reply& r) noexcept {
        return net_error(e, op, r.to_string());
    }

    // An address as a path in a command: "<a@b>", the characters that
    // would end the command (CR, LF, NUL) or the path ('<', '>') refused
    inline bool smtp_path_ok(std::string_view a) noexcept {
        for (char c : a) {
            if (c == '\r' || c == '\n' || c == '\0' || c == '<' || c == '>') {
                return false;
            }
        }
        return a.size() <= 1024;
    }

    // xtext of RFC 3461 §4: '+', '=' and what is not printable ASCII as +XX
    inline std::string xtext_encode(std::string_view s) {
        static constexpr char Hex[] = "0123456789ABCDEF";
        std::string out;
        for (char c : s) {
            uint8_t u = uint8_t(c);
            if (u < 33 || u > 126 || c == '+' || c == '=') {
                out += '+';
                out += Hex[u >> 4];
                out += Hex[u & 15];
            } else {
                out += c;
            }
        }
        return out;
    }

    inline std::string xtext_decode(std::string_view s) {
        auto hex = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '+' && i + 2 < s.size() && hex(s[i + 1]) >= 0 && hex(s[i + 2]) >= 0) {
                out += char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
                i += 2;
            } else {
                out += s[i];
            }
        }
        return out;
    }

    // The message as DATA sends it (RFC 5321 §4.5.2): line breaks CRLF
    // (a bare LF or CR made one), a '.' doubled at the start of a line,
    // the end "CRLF.CRLF"; as BDAT sends it, the same without the dots
    // and the end
    inline void smtp_stuffed(std::string_view m, std::string& out, bool dots) {
        out.reserve(out.size() + m.size() + m.size() / 64 + 8);
        bool line_start = true;
        size_t i = 0;
        const size_t n = m.size();
        while (i < n) {
            if (line_start && dots && m[i] == '.') {
                out += '.';
            }
            // the rest of the line in one piece
            const char* p = m.data() + i;
            const void* lf = std::memchr(p, '\n', n - i);
            size_t end = lf ? size_t(static_cast<const char*>(lf) - m.data()) : n;
            const void* cr = std::memchr(p, '\r', end - i);
            if (cr) {
                size_t k = size_t(static_cast<const char*>(cr) - m.data());
                out.append(p, k - i);
                out += "\r\n";
                i = (k + 1 < n && m[k + 1] == '\n') ? k + 2 : k + 1;
                line_start = true;
                continue;
            }
            out.append(p, end - i);
            if (end < n) {
                out += "\r\n";
                i = end + 1;
                line_start = true;
            } else {
                i = n;
                line_start = false;
            }
        }
        if (!line_start) {
            out += "\r\n";
        }
        if (dots) {
            out += ".\r\n";
        }
    }

    inline bool smtp_ascii(std::string_view s) noexcept {
        for (char c : s) {
            if (uint8_t(c) >= 0x80) {
                return false;
            }
        }
        return true;
    }
}
