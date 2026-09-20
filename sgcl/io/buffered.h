//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "stream.h"
#include "../containers/array.h"
#include "../core/config.h"

#include <cstring>

namespace sgcl::io {
    // Buffering over any reader or writer (bufio): the stream is read in
    // blocks of config::IoBufferSize (8 KB) into a managed array held by
    // a tracked_ptr, one object without a header, config::PageSize /
    // IoBufferSize of them on a page; the buffered reader hands out lines
    // and prefixes as slices of that block (slice.h): nothing allocated
    // per line, and the slice holds the block, so a line kept past the
    // next read stays valid — on the old block, which the reader has let
    // go of by then, alive for as long as the slice is.

    namespace detail {
        using IoBlock = array<std::byte, config::IoBufferSize>;
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
    // "\n" (and "\r\n"); the last line of a stream that does not end in
    // one is a line too; nullopt is the end of the stream.
    class buffered_reader final : public reader {
    public:
        explicit buffered_reader(tracked_ptr<reader> r)
        : _reader(std::move(r)), _block(make_tracked<detail::IoBlock>()) {
        }

        result<size_t> read(slice<std::byte> out) override {
            if (out.empty()) {
                return 0;
            }
            if (buffered() == 0) {
                if (out.size() >= _block->size()) {
                    return _reader->read(out);
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

        task<result<size_t>> async_read(slice<std::byte> out) override {
            if (out.empty()) {
                co_return 0;
            }
            if (buffered() == 0) {
                if (out.size() >= _block->size()) {
                    co_return co_await _reader->async_read(out);
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

        result<optional<slice<const char>>> read_until(char delimiter) {
            _long.clear();
            for (;;) {
                auto found = _find(delimiter);
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

        task<result<optional<slice<const char>>>> async_read_until(char delimiter) {
            _long.clear();
            for (;;) {
                auto found = _find(delimiter);
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

        result<optional<slice<const char>>> read_line() {
            return _line(read_until('\n'));
        }

        task<result<optional<slice<const char>>>> async_read_line() {
            co_return _line(co_await async_read_until('\n'));
        }

        // The next n bytes without consuming them (fewer at the end of
        // the stream, at most the buffer's size): a slice of the block
        result<slice<const std::byte>> peek(size_t n) {
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

        result<optional<std::byte>> read_byte() {
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
        result<size_t> discard(size_t n) {
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
                auto r = read_line();
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

        tracked_ptr<reader> underlying() const noexcept {
            return _reader;
        }

    private:
        // A read of the block from the front, the buffer being empty
        result<size_t> _fill() {
            _begin = _end = 0;
            auto r = _reader->read(_block_room(0));
            if (r) {
                _end = *r;
            }
            return r;
        }

        task<result<size_t>> _async_fill() {
            _begin = _end = 0;
            auto r = co_await _reader->async_read(_block_room(0));
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
                    std::memmove(_block->data(), _block->data() + _begin, _end - _begin);
                    _end -= _begin;
                    _begin = 0;
                }
            }
        }

        result<size_t> _fill_tail() {
            _make_room();
            auto r = _reader->read(_block_room(_end));
            if (r) {
                _end += *r;
            }
            return r;
        }

        task<result<size_t>> _async_fill_tail() {
            _make_room();
            auto r = co_await _reader->async_read(_block_room(_end));
            if (r) {
                _end += *r;
            }
            co_return r;
        }

        size_t _take(const slice<std::byte>& out) noexcept {
            size_t n = std::min(out.size(), buffered());
            std::memcpy(out.data(), _block->data() + _begin, n);
            _begin += n;
            return n;
        }

        // The block's bytes [from, from + n) as a writable slice holding
        // the block, and as characters
        slice<std::byte> _block_room(size_t from) noexcept {
            return slice<std::byte>(_block, _block->data() + from, _block->size() - from);
        }

        slice<const std::byte> _block_slice(size_t from, size_t n) const noexcept {
            return slice<const std::byte>(_block, _block->data() + from, n);
        }

        slice<const char> _block_text(size_t from, size_t n) const noexcept {
            return slice<const char>(_block, reinterpret_cast<const char*>(_block->data()) + from, n);
        }

        // The assembled long line as characters: a slice of _long's buffer
        slice<const char> _long_text() const noexcept {
            return detail::text_slice_of(_long.as_slice());
        }

        // The delimiter searched for among the unread bytes: the token
        // consumed (with what _long holds before it), or nullopt to read
        // more, or line_too_long
        result<optional<slice<const char>>> _find(char delimiter) {
            auto first = _block->data() + _begin;
            auto last = _block->data() + _end;
            auto p = static_cast<const std::byte*>(std::memchr(first, delimiter, last - first));
            if (p) {
                size_t n = p - first;
                _begin += n + 1;
                if (_long.empty()) {
                    if (_max_line && n > _max_line) {
                        return detail::fail(error(errc::line_too_long, "read_line"));
                    }
                    return optional<slice<const char>>(_block_text(_begin - n - 1, n));
                }
                _long.insert(_long.end(), first, first + n);
                if (_max_line && _long.size() > _max_line) {
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

        // A token of read_until('\n') as a line: without its '\r'
        static result<optional<slice<const char>>> _line(result<optional<slice<const char>>> r) {
            if (r && *r && !(*r)->empty() && (*r)->back() == '\r') {
                (*r)->remove_suffix(1);
            }
            return r;
        }

        tracked_ptr<reader> _reader;
        tracked_ptr<detail::IoBlock> _block;
        size_t _begin = 0, _end = 0;   // the unread bytes: [_begin, _end)
        vector<std::byte> _long;       // a line that did not fit the block
        size_t _max_line = 0;
        optional<error> _error;
    };

    // A writer with a buffer in front of w: write() fills the buffer and
    // writes it to w when full (a write larger than the buffer goes to w
    // directly, after the buffer). flush() writes what is buffered; the
    // destructor does not, it runs on the collector's thread and could
    // report nothing (as bufio.Writer: what is not flushed is lost), so a
    // buffered writer is flushed or closed when done. close() flushes and
    // closes w when w is a closer.
    class buffered_writer final : public writer, public closer {
    public:
        explicit buffered_writer(tracked_ptr<writer> w)
        : _writer(std::move(w)), _block(make_tracked<detail::IoBlock>()) {
        }

        result<size_t> write(slice<const std::byte> data) override {
            if (_closed) {
                return detail::fail(error(errc::closed, "write"));
            }
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= _block->size()) {
                    auto r = _writer->write(data.subspan(written));
                    if (!r) {
                        return detail::fail(r);
                    }
                    return data.size();
                }
                size_t n = std::min(rest, available());
                std::memcpy(_block->data() + _size, data.data() + written, n);
                _size += n;
                written += n;
                if (available() == 0) {
                    auto r = flush();
                    if (!r) {
                        return detail::fail(r);
                    }
                }
            }
            return data.size();
        }

        task<result<size_t>> async_write(slice<const std::byte> data) override {
            if (_closed) {
                co_return detail::fail(error(errc::closed, "write"));
            }
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= _block->size()) {
                    auto r = co_await _writer->async_write(data.subspan(written));
                    if (!r) {
                        co_return detail::fail(r);
                    }
                    co_return data.size();
                }
                size_t n = std::min(rest, available());
                std::memcpy(_block->data() + _size, data.data() + written, n);
                _size += n;
                written += n;
                if (available() == 0) {
                    auto r = co_await async_flush();
                    if (!r) {
                        co_return detail::fail(r);
                    }
                }
            }
            co_return data.size();
        }

        result<void> flush() {
            if (_size) {
                auto r = _writer->write(slice<const std::byte>(_block->data(), _size));
                if (!r) {
                    return detail::fail(r);
                }
                _size = 0;
            }
            return {};
        }

        task<result<void>> async_flush() {
            if (_size) {
                auto r = co_await _writer->async_write(slice<const std::byte>(_block->data(), _size));
                if (!r) {
                    co_return detail::fail(r);
                }
                _size = 0;
            }
            co_return result<void>();
        }

        result<void> close() override {
            if (_closed) {
                return {};
            }
            auto r = flush();
            _closed = true;
            if (auto c = dynamic_cast<closer*>(_writer.get())) {
                auto cc = c->close();
                if (r && !cc) {
                    return cc;
                }
            }
            return r;
        }

        bool is_closed() const noexcept override {
            return _closed;
        }

        // The bytes in the buffer not yet written; the room left
        size_t buffered() const noexcept {
            return _size;
        }

        size_t available() const noexcept {
            return _block->size() - _size;
        }

        tracked_ptr<writer> underlying() const noexcept {
            return _writer;
        }

    private:
        tracked_ptr<writer> _writer;
        tracked_ptr<detail::IoBlock> _block;
        size_t _size = 0;
        bool _closed = false;
    };
}
