//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../../async/coroutine.h"
#include "../../../core/aliases.h"
#include "../../../core/array.h"
#include "../../../core/make_tracked.h"
#include "../../../core/string.h"
#include "../../../core/tracked_ptr.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

// What both sides of POP3 read and write by: a connection's bytes as
// lines, the multi-line responses of RFC 1939 §3 (byte-stuffed, ended by
// a line of one dot), the words of a command
namespace sgcl::net::pop3::detail {
    using namespace sgcl::detail;
    using net::detail::fail;

    using Pop3Block = array<std::byte, 16384>;

    SGCL_INLINE_HOT constexpr char pop3_upper(char c) noexcept {
        return char(c - (unsigned(c - 'a') < 26u) * 32);
    }

    inline bool pop3_iequal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (pop3_upper(a[i]) != pop3_upper(b[i])) {
                return false;
            }
        }
        return true;
    }

    // A line from the bytes without its CRLF (a bare LF taken too): the
    // bytes it took, 0 when no whole line waits; too_long when more than
    // max bytes wait with no end of line in them
    inline size_t pop3_take_line(std::string_view v, std::string& line, size_t max, bool& too_long) {
        too_long = false;
        size_t nl = v.find('\n');
        if (nl == std::string_view::npos) {
            too_long = v.size() > max;
            return 0;
        }
        size_t end = nl > 0 && v[nl - 1] == '\r' ? nl - 1 : nl;
        line.assign(v.data(), end);
        return nl + 1;
    }

    // The body of a multi-line response, up to its line of one dot, the
    // dots that stuffed lines taken off, lines joined by CRLF (each ended
    // by one): the bytes it took, 0 while it is not whole
    inline size_t pop3_take_multiline(std::string_view v, std::string& out, bool& too_long, size_t max) {
        too_long = false;
        size_t end;
        if (v.substr(0, 3) == ".\r\n") {
            end = 0;
        } else {
            end = v.find("\r\n.\r\n");
            if (end == std::string_view::npos) {
                too_long = v.size() > max;
                return 0;
            }
            end += 2;
        }
        out.clear();
        out.reserve(end);
        std::string_view body = v.substr(0, end);
        size_t p = 0;
        while (p < body.size()) {
            size_t eol = body.find("\r\n", p);
            if (eol == std::string_view::npos) {
                eol = body.size();
            }
            size_t from = body[p] == '.' ? p + 1 : p;
            out.append(body.data() + from, eol - from);
            out += "\r\n";
            p = eol + 2;
        }
        return end + 3;
    }

    // A connection's bytes as lines: what came is kept in buf, a line taken
    // from its front
    struct Pop3Wire {
        net::connection c;
        tracked_ptr<Pop3Block> block = make_tracked<Pop3Block>();
        std::string buf;
        size_t at = 0;

        explicit Pop3Wire(const net::connection& conn) noexcept
        : c(conn) {
        }

        size_t buffered() const noexcept {
            return buf.size() - at;
        }

        void discard() noexcept {
            buf.clear();
            at = 0;
        }

        void compact() {
            if (at == buf.size()) {
                buf.clear();
                at = 0;
            } else if (at > 65536 && at * 2 > buf.size()) {
                buf.erase(0, at);
                at = 0;
            }
        }

        // A read without a frame of its own (net's try_read): the bytes
        // there now appended (done), or the readiness to wait for, or slow
        // (the waiting read must serve). The caller loops:
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
            compact();
            auto& impl = net::detail::ConnectionAccess::impl(c);
            auto r = impl.try_read(slice<byte>(block->data(), block->size()), t.slow);
            if (t.slow) {
                return t;
            }
            if (!r) {
                t.result = net::detail::fail(r);
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

        // The waiting read, for a transport try_fill cannot serve
        async::task<expected<size_t, io::error>> fill() noexcept {
            compact();
            auto r = co_await c.async_read(slice<byte>(block->data(), block->size()));
            if (r && *r) {
                buf.append(reinterpret_cast<const char*>(block->data()), *r);
            }
            co_return r;
        }

        // A line from the buffer without its CRLF (pop3_take_line)
        bool take_line(std::string& line, size_t max, bool& too_long) {
            size_t n = pop3_take_line(std::string_view(buf.data() + at, buf.size() - at), line, max, too_long);
            at += n;
            return n != 0;
        }

        // The body of a multi-line response (pop3_take_multiline)
        bool take_multiline(std::string& out, bool& too_long, size_t max) {
            size_t n = pop3_take_multiline(std::string_view(buf.data() + at, buf.size() - at), out, too_long, max);
            at += n;
            return n != 0;
        }
    };

    // A message as the lines of a multi-line response: CRLF line breaks (a
    // bare LF made CRLF), a line that starts with a dot given another,
    // CRLF at its end, then the line of one dot. The lines are found by
    // memchr and copied whole: a message of CRLF lines without a leading
    // dot is one append a line
    inline void pop3_put_multiline(std::string& out, std::string_view text) {
        out.reserve(out.size() + text.size() + text.size() / 64 + 5);
        size_t at = 0;
        while (at < text.size()) {
            const void* found = std::memchr(text.data() + at, '\n', text.size() - at);
            size_t nl = found ? size_t(static_cast<const char*>(found) - text.data()) : text.size();
            if (text[at] == '.') {
                out += '.';
            }
            size_t end = nl;
            if (found && end > at && text[end - 1] == '\r') {
                --end;
            }
            out.append(text.data() + at, end - at);
            out += "\r\n";
            at = found ? nl + 1 : text.size();
        }
        out += ".\r\n";
    }

    // Writes the bytes of a std::string the caller's frame keeps (for the
    // paths where a frame does not count)
    inline async::task<expected<size_t, io::error>> pop3_write(net::connection c, const std::string& out) noexcept {
        if (out.empty()) {
            co_return size_t(0);
        }
        co_return co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
    }
}
