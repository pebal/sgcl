//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once
#include "../core/detail/bytes.h"

#include "stream.h"
#include "../core/array.h"
#include "../core/config.h"
#include "../async/generator.h"
#include "../core/generator.h"

#include <cstring>

namespace sgcl::io {
    // Buffering over any reader or writer (bufio): the stream is read in
    // blocks of config::io_buffer_size (8 KB) into a managed array held by
    // a tracked_ptr, one object without a header, config::page_size /
    // config::io_buffer_size of them on a page; the buffered reader hands out lines
    // and prefixes as slices of that block (slice.h): nothing allocated
    // per line, and the slice holds the block, so a line kept past the
    // next read stays valid — on the old block, which the reader has let
    // go of by then, alive for as long as the slice is.

    namespace detail {
        using IoBlock = array<byte, config::io_buffer_size>;
    }

    // A reader with a buffer in front of r: read() takes from the buffer
    // and refills it from r when empty (a read larger than the buffer
    // goes to r directly). read_line and read_until return the token as
    // a slice of the block, holding it: valid as long as the slice is
    // kept, its characters those of the moment it was made (the reader
    // reuses the block for the next lines, so a slice kept across reads
    // is copied first: `string(line)`). A line longer than the block is
    // assembled in a vector the reader owns, and the slice is of that
    // vector's buffer, held likewise; set_max_line bounds it (none by
    // default: a file is trusted; a reader over a socket sets one), a
    // longer line being errc::line_too_long. The line comes without its
    // "\n" (and "\r\n"); read_until's token keeps its delimiter, as Go's
    // ReadString does, so that "a,b," is told from "a,b"; the last token
    // of a stream that does not end in one is a token too (without it);
    // nullopt is the end of the stream.
    class buffered_reader final : public mixin::reader<buffered_reader> {
    public:
        explicit buffered_reader(const io::reader& r)
        : _reader(r), _block(make_tracked<detail::IoBlock>()) {
        }

        // Not copied, as a std::ifstream is not: a copy would share the
        // block with a position of its own, and each would read over what
        // the other refills. Moved, and passed on by reference (a handle,
        // io::reader, refers to one)
        buffered_reader(const buffered_reader&) = delete;
        buffered_reader& operator=(const buffered_reader&) = delete;
        buffered_reader(buffered_reader&&) noexcept = default;
        buffered_reader& operator=(buffered_reader&&) noexcept = default;

        expected<size_t, error> read(const slice<byte>& out) {
            if (out.empty()) {
                return 0;
            }
            if (buffered() == 0) {
                if (out.size() >= _block->size()) {
                    return _reader.read(out);
                }
                auto r = _fill();
                if (!r) {
                    return r;
                }
                if (*r == 0) {
                    return 0;
                }
            }
            return _take(out);
        }

        async::task<expected<size_t, error>> async_read(slice<byte> out) {
            if (out.empty()) {
                co_return 0;
            }
            if (buffered() == 0) {
                if (out.size() >= _block->size()) {
                    co_return co_await _reader.async_read(out);
                }
                auto r = co_await _async_fill();
                if (!r) {
                    co_return r;
                }
                if (*r == 0) {
                    co_return 0;
                }
            }
            co_return _take(out);
        }

        // `read_until(...)` on this thread, `co_await async_read_until(...)` in a task
        expected<optional<slice<const char>>, error> read_until(char delimiter) {
            return _block_read_until(delimiter, true);
        }

        async::task<expected<optional<slice<const char>>, error>> async_read_until(char delimiter) {
            return _co_read_until(delimiter, true);
        }

        // `read_line(...)` on this thread, `co_await async_read_line(...)` in a task
        expected<optional<slice<const char>>, error> read_line() {
            return _block_read_line();
        }

        async::task<expected<optional<slice<const char>>, error>> async_read_line() {
            return _co_read_line();
        }

        // The next n bytes without consuming them (fewer at the end of
        // the stream, at most the buffer's size): a slice of the block
        expected<slice<const byte>, error> peek(size_t n) {
            n = std::min(n, _block->size());
            while (buffered() < n) {
                auto r = _fill_tail();
                if (!r) {
                    return detail::fail(r);
                }
                if (*r == 0) {
                    break;
                }
            }
            return _block_slice(_begin, std::min(n, buffered()));
        }

        expected<optional<byte>, error> read_byte() {
            if (buffered() == 0) {
                auto r = _fill();
                if (!r) {
                    return detail::fail(r);
                }
                if (*r == 0) {
                    return nullopt;
                }
            }
            return (*_block)[_begin++];
        }

        // Skips n bytes: the bytes skipped
        expected<size_t, error> discard(size_t n) {
            size_t skipped = 0;
            while (skipped < n) {
                if (buffered() == 0) {
                    auto r = _fill();
                    if (!r) {
                        return r;
                    }
                    if (*r == 0) {
                        break;
                    }
                }
                size_t k = std::min(n - skipped, buffered());
                _begin += k;
                skipped += k;
            }
            return skipped;
        }

        // The bytes in the buffer, readable without touching r
        size_t buffered() const noexcept {
            return _end - _begin;
        }

        // The longest line read_line, read_until and lines accept, in
        // bytes; 0 for no bound
        void set_max_line(size_t n) noexcept {
            _max_line = n;
        }

        size_t max_line() const noexcept {
            return _max_line;
        }

        // The lines of the stream as a range: `for (auto line : r.lines())`;
        // the generator ends at the end of the stream or on an error, which
        // last_error() holds afterwards (Scanner.Err())
        generator<slice<const char>> lines() {
            _error = nullopt;
            for (;;) {
                auto r = _block_read_line();
                if (!r) {
                    _error = r.error();
                    co_return;
                }
                if (!*r) {
                    co_return;
                }
                co_yield **r;
            }
        }

        // The same for a task: `while (auto line = co_await g.next())`
        async::generator<slice<const char>> async_lines() {
            _error = nullopt;
            for (;;) {
                auto r = co_await _co_read_line();
                if (!r) {
                    _error = r.error();
                    co_return;
                }
                if (!*r) {
                    co_return;
                }
                co_yield **r;
            }
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        io::reader underlying() const noexcept {
            return _reader;
        }

        // Drops what is buffered and closes r (the reader underneath, when
        // it has a close: a file, a connection), as buffered_writer's close
        // closes its writer; a read after it is r's, which a closed file
        // answers with errc::closed
        expected<void, error> close() {
            _drop();
            return _reader.close();
        }

        async::task<expected<void, error>> async_close() {
            _drop();
            return _reader.async_close();
        }

    private:
        void _drop() noexcept {
            _begin = _end = 0;
            _long.clear();
        }

        // A read of the block from the front, the buffer being empty
        expected<size_t, error> _fill() {
            _begin = _end = 0;
            auto r = _reader.read(_block_room(0));
            if (r) {
                _end = *r;
            }
            return r;
        }

        async::task<expected<size_t, error>> _async_fill() {
            _begin = _end = 0;
            auto r = co_await _reader.async_read(_block_room(0));
            if (r) {
                _end = *r;
            }
            co_return r;
        }

        // A read into the room behind the unread bytes, which are moved
        // to the front first; a full block is spilled into _long
        void _make_room() {
            if (_end == _block->size()) {
                if (_begin == 0) {
                    _long.insert(_long.end(), _block->data(), _block->data() + _end);
                    _begin = _end = 0;
                } else {
                    sgcl::detail::move_bytes(_block->data(), _block->data() + _begin, _end - _begin);
                    _end -= _begin;
                    _begin = 0;
                }
            }
        }

        expected<size_t, error> _fill_tail() {
            _make_room();
            auto r = _reader.read(_block_room(_end));
            if (r) {
                _end += *r;
            }
            return r;
        }

        async::task<expected<size_t, error>> _async_fill_tail() {
            _make_room();
            auto r = co_await _reader.async_read(_block_room(_end));
            if (r) {
                _end += *r;
            }
            co_return r;
        }

        size_t _take(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), buffered());
            sgcl::detail::copy_bytes(out.data(), _block->data() + _begin, n);
            _begin += n;
            return n;
        }

        // The block's bytes [from, from + n) as a writable slice holding
        // the block, and as characters
        slice<byte> _block_room(size_t from) noexcept {
            return slice<byte>(_block, _block->data() + from, _block->size() - from);
        }

        slice<const byte> _block_slice(size_t from, size_t n) const noexcept {
            return slice<const byte>(_block, _block->data() + from, n);
        }

        slice<const char> _block_text(size_t from, size_t n) const noexcept {
            return slice<const char>(_block, reinterpret_cast<const char*>(_block->data()) + from, n);
        }

        // The assembled long line as characters: a slice of _long's buffer
        slice<const char> _long_text() const noexcept {
            return detail::text_slice_of(_long.as_slice());
        }

        // The delimiter searched for among the unread bytes: the token
        // consumed (with what _long holds before it; with the delimiter
        // when `keep`), or nullopt to read more, or line_too_long
        expected<optional<slice<const char>>, error> _find(char delimiter, bool keep) {
            auto first = _block->data() + _begin;
            auto last = _block->data() + _end;
            auto p = static_cast<const byte*>(std::memchr(first, delimiter, last - first));
            if (p) {
                size_t n = p - first;
                _begin += n + 1;
                if (_long.empty()) {
                    if (_max_line && n > _max_line) {
                        return detail::fail(error(errc::line_too_long, "read_line"));
                    }
                    return optional<slice<const char>>(_block_text(_begin - n - 1, n + keep));
                }
                _long.insert(_long.end(), first, first + n + keep);
                if (_max_line && _long.size() - keep > _max_line) {
                    return detail::fail(error(errc::line_too_long, "read_line"));
                }
                return optional<slice<const char>>(_long_text());
            }
            if (_max_line && _long.size() + buffered() > _max_line) {
                return detail::fail(error(errc::line_too_long, "read_line"));
            }
            return optional<slice<const char>>();
        }

        // The end of the stream: what remains is the last token, or nothing
        optional<slice<const char>> _rest() {
            if (_long.empty()) {
                if (buffered() == 0) {
                    return nullopt;
                }
                auto v = _block_text(_begin, buffered());
                _begin = _end;
                return v;
            }
            _long.insert(_long.end(), _block->data() + _begin, _block->data() + _end);
            _begin = _end;
            return _long_text();
        }

        // A token up to '\n', the '\n' left out, as a line: without its '\r'
        static expected<optional<slice<const char>>, error> _line(expected<optional<slice<const char>>, error> r) {
            if (r && *r && !(*r)->empty() && (*r)->back() == '\r') {
                (*r)->remove_suffix(1);
            }
            return r;
        }

        io::reader _reader;
        tracked_ptr<detail::IoBlock> _block;
        size_t _begin = 0, _end = 0;   // the unread bytes: [_begin, _end)
        vector<byte> _long;       // a line that did not fit the block
        size_t _max_line = 0;
        optional<error> _error;

        // the two halves of the operations above: a thread's and a task's
        expected<optional<slice<const char>>, error> _block_read_until(char delimiter, bool keep)  {
            _long.clear();
            for (;;) {
                auto found = _find(delimiter, keep);
                if (!found) {
                    return detail::fail(found);
                }
                if (*found) {
                    return **found;
                }
                auto r = _fill_tail();
                if (!r) {
                    return detail::fail(r);
                }
                if (*r == 0) {
                    return _rest();
                }
            }
        }

        async::task<expected<optional<slice<const char>>, error>> _co_read_until(char delimiter, bool keep)  {
            _long.clear();
            for (;;) {
                auto found = _find(delimiter, keep);
                if (!found) {
                    co_return detail::fail(found);
                }
                if (*found) {
                    co_return **found;
                }
                auto r = co_await _async_fill_tail();
                if (!r) {
                    co_return detail::fail(r);
                }
                if (*r == 0) {
                    co_return _rest();
                }
            }
        }

        expected<optional<slice<const char>>, error> _block_read_line()  {
            return _line(_block_read_until('\n', false));
        }

        async::task<expected<optional<slice<const char>>, error>> _co_read_line()  {
            co_return _line(co_await _co_read_until('\n', false));
        }
    };

    // A writer with a buffer in front of w: write() fills the buffer and
    // writes it to w when full (a write larger than the buffer goes to w
    // directly, after the buffer). flush() writes what is buffered; the
    // destructor does not, it runs on the collector's thread and could
    // report nothing (as bufio.Writer: what is not flushed is lost), so a
    // buffered writer is flushed or closed when done. close() flushes and
    // closes w when w is a closer.
    class buffered_writer final : public mixin::writer<buffered_writer> {
    public:
        using mixin::writer<buffered_writer>::write;
        using mixin::writer<buffered_writer>::async_write;

        explicit buffered_writer(const io::writer& w)
        : _writer(w), _block(make_tracked<detail::IoBlock>()) {
        }

        // Moved, not copied, as buffered_reader: a copy would share the
        // block with a fill of its own, and the bytes not yet flushed would
        // go out twice, or be written over
        buffered_writer(const buffered_writer&) = delete;
        buffered_writer& operator=(const buffered_writer&) = delete;
        buffered_writer(buffered_writer&&) noexcept = default;
        buffered_writer& operator=(buffered_writer&&) noexcept = default;

        expected<size_t, error> write(const slice<const byte>& data) {
            if (_closed) {
                return detail::fail(error(errc::closed, "write"));
            }
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= _block->size()) {
                    auto r = _writer.write(data.subspan(written));
                    if (!r) {
                        return detail::fail(r);
                    }
                    return data.size();
                }
                size_t n = std::min(rest, available());
                sgcl::detail::copy_bytes(_block->data() + _size, data.data() + written, n);
                _size += n;
                written += n;
                if (available() == 0) {
                    auto r = _block_flush();
                    if (!r) {
                        return detail::fail(r);
                    }
                }
            }
            return data.size();
        }

        async::task<expected<size_t, error>> async_write(slice<const byte> data) {
            if (_closed) {
                co_return detail::fail(error(errc::closed, "write"));
            }
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= _block->size()) {
                    auto r = co_await _writer.async_write(data.subspan(written));
                    if (!r) {
                        co_return detail::fail(r);
                    }
                    co_return data.size();
                }
                size_t n = std::min(rest, available());
                sgcl::detail::copy_bytes(_block->data() + _size, data.data() + written, n);
                _size += n;
                written += n;
                if (available() == 0) {
                    auto r = co_await _co_flush();
                    if (!r) {
                        co_return detail::fail(r);
                    }
                }
            }
            co_return data.size();
        }

        // `flush(...)` on this thread, `co_await async_flush(...)` in a task
        expected<void, error> flush() {
            return _block_flush();
        }

        async::task<expected<void, error>> async_flush() {
            return _co_flush();
        }

        // Flushes, then closes w (the writer underneath, when it has a
        // close: a file, a connection); a second close does nothing
        expected<void, error> close() {
            if (_closed) {
                return {};
            }
            auto r = _block_flush();
            _closed = true;
            auto cc = _writer.close();
            if (r && !cc) {
                return cc;
            }
            return r;
        }

        async::task<expected<void, error>> async_close() {
            if (_closed) {
                co_return expected<void, error>();
            }
            auto r = co_await _co_flush();
            _closed = true;
            auto cc = co_await _writer.async_close();
            if (r && !cc) {
                co_return cc;
            }
            co_return r;
        }

        bool is_closed() const noexcept {
            return _closed;
        }

        // The bytes in the buffer not yet written; the room left
        size_t buffered() const noexcept {
            return _size;
        }

        size_t available() const noexcept {
            return _block->size() - _size;
        }

        io::writer underlying() const noexcept {
            return _writer;
        }

    private:
        io::writer _writer;
        tracked_ptr<detail::IoBlock> _block;
        size_t _size = 0;
        bool _closed = false;

        // the two halves of the operations above: a thread's and a task's
        expected<void, error> _block_flush()  {
            if (_size) {
                auto r = _writer.write(slice<const byte>(_block->data(), _size));
                if (!r) {
                    return detail::fail(r);
                }
                _size = 0;
            }
            return {};
        }

        async::task<expected<void, error>> _co_flush()  {
            if (_size) {
                auto r = co_await _writer.async_write(slice<const byte>(_block->data(), _size));
                if (!r) {
                    co_return detail::fail(r);
                }
                _size = 0;
            }
            co_return expected<void, error>();
        }
    };
}
