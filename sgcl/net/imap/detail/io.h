//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "syntax.h"
#include "../error.h"
#include "../../connection.h"
#include "../../../async/coroutine.h"
#include "../../../core/detail/bytes.h"

#include <cstring>
#include <memory>
#include <string>
#include <string_view>

// The bytes of an IMAP connection both sides read through: a buffer of
// their own in front of the connection (never the connection's read_line,
// whose buffer a STARTTLS would have to drain into the TLS layer), lines
// to CRLF and literals of a size given. discard() drops what is buffered:
// after STARTTLS, bytes sent in the clear behind the command are never
// read as the first of the protected stream (RFC 9051 §6.2.1, the
// injection of CVE-2011-0411).
namespace sgcl::net::imap::detail {
    enum class LineEnd : uint8_t { ok, eof, too_long };

    class Reader {
    public:
        // 16 KB for a server's connection (a command is mostly a line); a
        // client takes 64 KB, so that a message of FETCH is read whole
        explicit Reader(size_t capacity = size_t(16) << 10) noexcept
        : _data(std::make_unique_for_overwrite<char[]>(capacity))
        , _cap(capacity) {
        }

        SGCL_INLINE_HOT void reset(const net::connection& c) noexcept {
            _c = c;
        }

        SGCL_INLINE_HOT const net::connection& connection() const noexcept {
            return _c;
        }

        SGCL_INLINE_HOT void discard() noexcept {
            _begin = _end = 0;
        }

        // What is buffered and not yet read
        SGCL_INLINE_HOT std::string_view pending() const noexcept {
            return std::string_view(_data.get() + _begin, _end - _begin);
        }

        SGCL_INLINE_HOT size_t buffered() const noexcept {
            return _end - _begin;
        }

        // Whether a line (to its LF) is already buffered whole
        SGCL_INLINE_HOT bool has_line() const noexcept {
            return std::memchr(_data.get() + _begin, '\n', _end - _begin) != nullptr;
        }

        // The size of the response the buffer holds whole: its lines, each
        // ended by CRLF, and the literals they announce (none past max);
        // 0 when it is not all there, or a line ends with a bare LF before
        // a literal (the slow road reads it)
        size_t complete_response(uint64_t max) const noexcept {
            const char* p = _data.get() + _begin;
            const size_t n = _end - _begin;
            size_t at = 0;
            for (;;) {
                const char* lf = static_cast<const char*>(std::memchr(p + at, '\n', n - at));
                if (!lf) {
                    return 0;
                }
                const size_t end = size_t(lf - p);
                if (end == at || p[end - 1] != '\r') {
                    return 0;
                }
                LiteralHead h = literal_at_end(std::string_view(p + at, end - 1 - at));
                at = end + 1;
                if (!h.found) {
                    return at;
                }
                if (h.overflow || h.size > max || n - at < h.size) {
                    return 0;
                }
                at += size_t(h.size);
            }
        }

        // The response complete_response found, into out without its last
        // CRLF (the inner ones before literals kept, as the lexer reads them)
        void take_response(std::string& out, size_t size) {
            out.assign(_data.get() + _begin, size - 2);
            _begin += size;
            if (_begin == _end) {
                _begin = _end = 0;
            }
        }

        // One line appended to out without its CRLF (a bare LF taken as
        // the end too); eof when the stream ends before a line, too_long
        // past max bytes of out (the line's rest left unread)
        async::task<expected<LineEnd, io::error>> read_line(std::string& out, size_t max) noexcept {
            for (;;) {
                const char* p = _data.get() + _begin;
                const size_t n = _end - _begin;
                const char* lf = static_cast<const char*>(std::memchr(p, '\n', n));
                if (lf) {
                    size_t len = size_t(lf - p);
                    const size_t take = len + 1;
                    if (len && p[len - 1] == '\r') {
                        --len;
                    }
                    if (out.size() + len > max) {
                        _begin += take;
                        co_return LineEnd::too_long;
                    }
                    out.append(p, len);
                    _begin += take;
                    co_return LineEnd::ok;
                }
                if (out.size() + n > max + 1) {
                    _begin = _end = 0;
                    co_return LineEnd::too_long;
                }
                out.append(p, n);
                _begin = _end = 0;
                auto r = co_await _fill();
                if (!r) {
                    co_return unexpected(r.error());
                }
                if (*r == 0) {
                    co_return LineEnd::eof;
                }
                // a CR at the end of what was appended belongs with the LF
                // that may come first in the next read
                if (!out.empty() && out.back() == '\r' && _end > _begin && _data[_begin] == '\n') {
                    out.pop_back();
                    _begin += 1;
                    co_return LineEnd::ok;
                }
            }
        }

        // Exactly n bytes appended to out; false when the stream ends first
        async::task<expected<bool, io::error>> read_exact(std::string& out, size_t n) noexcept {
            const size_t have = std::min(n, _end - _begin);
            out.append(_data.get() + _begin, have);
            _begin += have;
            if (_begin == _end) {
                _begin = _end = 0;
            }
            size_t left = n - have;
            if (left == 0) {
                co_return true;
            }
            const size_t at = out.size();
            out.resize(at + left);
            size_t got = 0;
            while (got < left) {
                // straight into out: the frame holds out's owner while the read runs
                auto r = co_await _c.async_read(slice<byte>(reinterpret_cast<byte*>(out.data() + at + got), left - got));
                if (!r) {
                    out.resize(at + got);
                    co_return unexpected(r.error());
                }
                if (*r == 0) {
                    out.resize(at + got);
                    co_return false;
                }
                got += *r;
            }
            co_return true;
        }

        // n bytes skipped (a literal refused); false when the stream ends first
        async::task<expected<bool, io::error>> skip(uint64_t n) noexcept {
            while (n) {
                if (_begin == _end) {
                    auto r = co_await _fill();
                    if (!r) {
                        co_return unexpected(r.error());
                    }
                    if (*r == 0) {
                        co_return false;
                    }
                }
                const size_t take = size_t(std::min<uint64_t>(n, _end - _begin));
                _begin += take;
                n -= take;
            }
            if (_begin == _end) {
                _begin = _end = 0;
            }
            co_return true;
        }

    private:
        async::task<expected<size_t, io::error>> _fill() noexcept {
            if (_begin == _end) {
                _begin = _end = 0;
            } else if (_end == _cap) {
                std::memmove(_data.get(), _data.get() + _begin, _end - _begin);
                _end -= _begin;
                _begin = 0;
            }
            auto r = co_await _c.async_read(slice<byte>(reinterpret_cast<byte*>(_data.get() + _end), _cap - _end));
            if (r) {
                _end += *r;
            }
            co_return r;
        }

        net::connection _c;
        std::unique_ptr<char[]> _data;
        size_t _cap;
        size_t _begin = 0;
        size_t _end = 0;
    };

    // Writes the bytes of a std::string the caller's frame keeps
    inline async::task<expected<size_t, io::error>> write_all(net::connection c, const std::string& out) noexcept {
        if (out.empty()) {
            co_return size_t(0);
        }
        co_return co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
    }
}
