//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/bytes.h"
#include "../core/detail/handle_word.h"

#include "stream.h"
#include "../core/array.h"
#include "../core/config.h"
#include "../async/generator.h"
#include "../core/generator.h"

#include <cstring>
#include <memory>

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
        // A buffered reader's state with its block in unmanaged memory (below)
        struct UnmanagedBlock {};

        class BufferedReaderState;
        class BufferedWriterState;
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
    //
    // The state of a buffered reader, the object its handles share
    // (io::buffered_reader below); the library's own readers (a
    // connection's read_line) hold it directly.
    namespace detail {
    class BufferedReaderState final {
    public:
        explicit BufferedReaderState(const io::reader& r)
        : _reader(r), _block(make_tracked<detail::IoBlock>()), _data(_block->data()) {
        }

        // A reader whose block is unmanaged memory it owns, for a caller
        // that copies each token out before the next read and hands no
        // slice on (a connection's read_line, net/connection.h): the slices
        // it returns hold nothing, and are the caller's to drop in time
        BufferedReaderState(const io::reader& r, detail::UnmanagedBlock)
        : _reader(r), _unmanaged(std::make_unique_for_overwrite<byte[]>(config::io_buffer_size)), _data(_unmanaged.get()) {
        }

        // Not copied: a copy would share the block with a position of its
        // own, and each would read over what the other refills; the
        // handles share one
        BufferedReaderState(const BufferedReaderState&) = delete;
        BufferedReaderState& operator=(const BufferedReaderState&) = delete;

        expected<size_t, error> read(const slice<byte>& out) {
            if (out.empty()) {
                return 0;
            }
            if (buffered() == 0) {
                if (out.size() >= config::io_buffer_size) {
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
                if (out.size() >= config::io_buffer_size) {
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
            n = std::min(n, config::io_buffer_size);
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
            return _data[_begin++];
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
            if (_end == config::io_buffer_size) {
                if (_begin == 0) {
                    _long.insert(_long.end(), _data, _data + _end);
                    _begin = _end = 0;
                } else {
                    sgcl::detail::move_bytes(_data, _data + _begin, _end - _begin);
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
            sgcl::detail::copy_bytes(out.data(), _data + _begin, n);
            _begin += n;
            return n;
        }

        // The block's bytes [from, from + n) as a writable slice holding
        // the block, and as characters
        slice<byte> _block_room(size_t from) noexcept {
            return slice<byte>(_block, _data + from, config::io_buffer_size - from);
        }

        slice<const byte> _block_slice(size_t from, size_t n) const noexcept {
            return slice<const byte>(_block, _data + from, n);
        }

        slice<const char> _block_text(size_t from, size_t n) const noexcept {
            return slice<const char>(_block, reinterpret_cast<const char*>(_data) + from, n);
        }

        // The assembled long line as characters: a slice of _long's buffer
        slice<const char> _long_text() const noexcept {
            return detail::text_slice_of(_long.as_slice());
        }

        // The delimiter searched for among the unread bytes: the token
        // consumed (with what _long holds before it; with the delimiter
        // when `keep`), or nullopt to read more, or line_too_long
        expected<optional<slice<const char>>, error> _find(char delimiter, bool keep) {
            auto first = _data + _begin;
            auto last = _data + _end;
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
            _long.insert(_long.end(), _data + _begin, _data + _end);
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
        tracked_ptr<detail::IoBlock> _block;   // the block, managed: the slices hold it
        std::unique_ptr<byte[]> _unmanaged;     // or unmanaged (UnmanagedBlock)
        byte* _data;                            // the block's bytes, either way
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

    }

    // A buffered reader as a handle: one tracked word to the state above,
    // copied and passed by value, the copies sharing one reader (one
    // block, one position). A handle is a tracked word: on a stack, in a
    // task, in a managed object; in a global or a std container, a
    // root_ptr to it, as to any managed object. `io::buffered_reader
    // lines(f)` makes the state over the stream; a default-constructed one
    // holds none (`!r`), and an operation on it is a contract violation.
    class buffered_reader final : public mixin::reader<buffered_reader> {
    public:
        buffered_reader() noexcept = default;

        explicit buffered_reader(const io::reader& r)
        : _state(make_tracked<detail::BufferedReaderState>(r)) {
        }

        expected<size_t, error> read(const slice<byte>& out) const {
            return _get().read(out);
        }

        async::task<expected<size_t, error>> async_read(const slice<byte>& out) const {
            return _get().async_read(out);
        }

        // `read_until(...)` on this thread, `co_await async_read_until(...)` in a task
        expected<optional<slice<const char>>, error> read_until(char delimiter) const {
            return _get().read_until(delimiter);
        }

        async::task<expected<optional<slice<const char>>, error>> async_read_until(char delimiter) const {
            return _get().async_read_until(delimiter);
        }

        // `read_line(...)` on this thread, `co_await async_read_line(...)` in a task
        expected<optional<slice<const char>>, error> read_line() const {
            return _get().read_line();
        }

        async::task<expected<optional<slice<const char>>, error>> async_read_line() const {
            return _get().async_read_line();
        }

        // The next n bytes without consuming them (fewer at the end of
        // the stream, at most the buffer's size): a slice of the block
        expected<slice<const byte>, error> peek(size_t n) const {
            return _get().peek(n);
        }

        expected<optional<byte>, error> read_byte() const {
            return _get().read_byte();
        }

        // Skips n bytes: the bytes skipped
        expected<size_t, error> discard(size_t n) const {
            return _get().discard(n);
        }

        // The bytes in the buffer, readable without touching the stream
        size_t buffered() const noexcept {
            return _get().buffered();
        }

        // The longest line read_line, read_until and lines accept, in
        // bytes; 0 for no bound
        void set_max_line(size_t n) const noexcept {
            _get().set_max_line(n);
        }

        size_t max_line() const noexcept {
            return _get().max_line();
        }

        // The lines of the stream as a range: `for (auto line : r.lines())`;
        // the generator ends at the end of the stream or on an error, which
        // last_error() holds afterwards (Scanner.Err())
        generator<slice<const char>> lines() const {
            return _get().lines();
        }

        // The same for a task: `while (auto line = co_await g.next())`
        async::generator<slice<const char>> async_lines() const {
            return _get().async_lines();
        }

        const optional<error>& last_error() const noexcept {
            return _get().last_error();
        }

        io::reader underlying() const noexcept {
            return _get().underlying();
        }

        // Drops what is buffered and closes the stream underneath (when
        // it has a close: a file, a connection)
        expected<void, error> close() const {
            return _get().close();
        }

        async::task<expected<void, error>> async_close() const {
            return _get().async_close();
        }

        // Whether this handle holds a reader
        explicit operator bool() const noexcept {
            return (bool)_state;
        }

        // The same reader: the same state
        friend bool operator==(const buffered_reader& a, const buffered_reader& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct detail::HandleAccess;

        detail::BufferedReaderState& _get() const noexcept {
            assert(_state && "an empty io::buffered_reader");
            return *_state;
        }

        const tracked_ptr<detail::BufferedReaderState>& _stream_state() const noexcept {
            return _state;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        buffered_reader(sgcl::detail::FromWord, const tracked_ptr<detail::BufferedReaderState>& w) noexcept
        : _state(w) {
        }

        tracked_ptr<detail::BufferedReaderState>& _handle_word() noexcept {
            return _state;
        }

        const tracked_ptr<detail::BufferedReaderState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::BufferedReaderState> _state;
    };

    namespace detail {
        template<>
        inline constexpr bool IsStreamHandle<buffered_reader> = true;
    }

    // A writer with a buffer in front of w: write() fills the buffer and
    // writes it to w when full (a write larger than the buffer goes to w
    // directly, after the buffer). flush() writes what is buffered; the
    // destructor does not, it runs on the collector's thread and could
    // report nothing (as bufio.Writer: what is not flushed is lost), so a
    // buffered writer is flushed or closed when done. close() flushes and
    // closes w when w is a closer. Every error it gives is kept as its
    // first (a failure of w, a write after close): every write and flush
    // after it gives that error at once and writes nothing, and close()
    // gives it too (it still closes w), so a writer may be written freely
    // and checked once, at the close; last_error() holds it. The block (8 KB) is unmanaged memory
    // the writer owns while it is written from this thread: nothing hands
    // out a slice of it, and a write of it is over when the call returns.
    // The first async operation moves it into a managed block, once for
    // the writer's life: a task's write may run on the pool and outlive
    // the frame of a task let go of, so the slice it is given holds the
    // block.
    //
    // The state of a buffered writer, the object its handles share
    // (io::buffered_writer below).
    namespace detail {
    class BufferedWriterState final {
    public:
        explicit BufferedWriterState(const io::writer& w)
        : _writer(w), _plain(std::make_unique_for_overwrite<byte[]>(config::io_buffer_size)), _data(_plain.get()) {
        }

        // Not copied, as the reader's: a copy would share the block with a
        // fill of its own, and the bytes not yet flushed would go out
        // twice, or be written over; the handles share one
        BufferedWriterState(const BufferedWriterState&) = delete;
        BufferedWriterState& operator=(const BufferedWriterState&) = delete;

        expected<size_t, error> write(const slice<const byte>& data) {
            if (auto e = _check()) {
                return detail::fail(*e);
            }
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= config::io_buffer_size) {
                    auto r = _writer.write(data.subspan(written));
                    if (!r) {
                        return detail::fail(_failed(r.error()));
                    }
                    return data.size();
                }
                size_t n = std::min(rest, available());
                sgcl::detail::copy_bytes(_data + _size, data.data() + written, n);
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
            if (auto e = _check()) {
                co_return detail::fail(*e);
            }
            _to_managed();
            size_t written = 0;
            while (written < data.size()) {
                size_t rest = data.size() - written;
                if (_size == 0 && rest >= config::io_buffer_size) {
                    auto r = co_await _writer.async_write(data.subspan(written));
                    if (!r) {
                        co_return detail::fail(_failed(r.error()));
                    }
                    co_return data.size();
                }
                size_t n = std::min(rest, available());
                sgcl::detail::copy_bytes(_data + _size, data.data() + written, n);
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
        // close: a file, a connection), after an error too; the kept error
        // is the result, of this close and of every one after it
        expected<void, error> close() {
            if (_closed) {
                return _kept();
            }
            _block_flush();   // a failure kept, the result below
            _closed = true;
            if (auto cc = _writer.close(); !cc) {
                _failed(cc.error());
            }
            return _kept();
        }

        async::task<expected<void, error>> async_close() {
            if (_closed) {
                co_return _kept();
            }
            co_await _co_flush();
            _closed = true;
            if (auto cc = co_await _writer.async_close(); !cc) {
                _failed(cc.error());
            }
            co_return _kept();
        }

        bool is_closed() const noexcept {
            return _closed;
        }

        // The bytes in the buffer not yet written; the room left
        size_t buffered() const noexcept {
            return _size;
        }

        size_t available() const noexcept {
            return config::io_buffer_size - _size;
        }

        io::writer underlying() const noexcept {
            return _writer;
        }

        // The first error given (a failure of w, a write after close), kept
        // (as buffered_reader's last_error and compress's writers): what
        // every operation after it gives
        const optional<error>& last_error() const noexcept {
            return _error;
        }

    private:
        io::writer _writer;
        std::unique_ptr<byte[]> _plain;            // the block, unmanaged, until the first async operation
        tracked_ptr<detail::IoBlock> _managed;     // the block from then on
        byte* _data;                               // the block's bytes, either way
        size_t _size = 0;
        bool _closed = false;
        optional<error> _error;                    // the first error given, kept

        // the kept error, or a write after close kept as one: what a write gives at once
        optional<error> _check() {
            if (_error) {
                return _error;
            }
            if (_closed) {
                return _failed(error(errc::closed, "write"));
            }
            return nullopt;
        }

        // e kept, unless a failure is kept already; the one kept
        const error& _failed(const error& e) {
            if (!_error) {
                _error = e;
            }
            return *_error;
        }

        expected<void, error> _kept() const {
            if (_error) {
                return detail::fail(*_error);
            }
            return {};
        }

        // The block moved into managed memory, the bytes in it kept: a
        // task's write is given a slice that holds it
        void _to_managed() {
            if (!_managed) {
                _managed = make_tracked<detail::IoBlock>();
                sgcl::detail::copy_bytes(_managed->data(), _data, _size);
                _data = _managed->data();
                _plain.reset();
            }
        }

        // the two halves of the operations above: a thread's and a task's
        expected<void, error> _block_flush()  {
            if (_error) {
                return detail::fail(*_error);
            }
            if (_size) {
                auto r = _writer.write(slice<const byte>(_data, _size));
                if (!r) {
                    return detail::fail(_failed(r.error()));
                }
                _size = 0;
            }
            return {};
        }

        async::task<expected<void, error>> _co_flush()  {
            if (_error) {
                co_return detail::fail(*_error);
            }
            _to_managed();
            if (_size) {
                auto r = co_await _writer.async_write(slice<const byte>(_managed, _data, _size));
                if (!r) {
                    co_return detail::fail(_failed(r.error()));
                }
                _size = 0;
            }
            co_return expected<void, error>();
        }
    };
    }

    // A buffered writer as a handle: one tracked word to the state above,
    // copied and passed by value, the copies sharing one writer (one
    // block, one kept error). A handle is a tracked word: on a stack, in a
    // task, in a managed object; in a global or a std container, a
    // root_ptr to it. `io::buffered_writer out(f)` makes the state over the
    // stream; a default-constructed one holds none (`!w`), and an
    // operation on it is a contract violation.
    class buffered_writer final : public mixin::writer<buffered_writer> {
    public:
        using mixin::writer<buffered_writer>::write;
        using mixin::writer<buffered_writer>::async_write;

        buffered_writer() noexcept = default;

        explicit buffered_writer(const io::writer& w)
        : _state(make_tracked<detail::BufferedWriterState>(w)) {
        }

        expected<size_t, error> write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const {
            return _get().async_write(data);
        }

        // `flush(...)` on this thread, `co_await async_flush(...)` in a task
        expected<void, error> flush() const {
            return _get().flush();
        }

        async::task<expected<void, error>> async_flush() const {
            return _get().async_flush();
        }

        // Flushes, then closes the writer underneath (when it has a close:
        // a file, a connection), after an error too; the kept error is the
        // result, of this close and of every one after it
        expected<void, error> close() const {
            return _get().close();
        }

        async::task<expected<void, error>> async_close() const {
            return _get().async_close();
        }

        bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The bytes in the buffer not yet written; the room left
        size_t buffered() const noexcept {
            return _get().buffered();
        }

        size_t available() const noexcept {
            return _get().available();
        }

        io::writer underlying() const noexcept {
            return _get().underlying();
        }

        // The first error given (a failure of the writer underneath, a
        // write after close), kept: what every operation after it gives
        const optional<error>& last_error() const noexcept {
            return _get().last_error();
        }

        // Whether this handle holds a writer
        explicit operator bool() const noexcept {
            return (bool)_state;
        }

        // The same writer: the same state
        friend bool operator==(const buffered_writer& a, const buffered_writer& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct detail::HandleAccess;

        detail::BufferedWriterState& _get() const noexcept {
            assert(_state && "an empty io::buffered_writer");
            return *_state;
        }

        const tracked_ptr<detail::BufferedWriterState>& _stream_state() const noexcept {
            return _state;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        buffered_writer(sgcl::detail::FromWord, const tracked_ptr<detail::BufferedWriterState>& w) noexcept
        : _state(w) {
        }

        tracked_ptr<detail::BufferedWriterState>& _handle_word() noexcept {
            return _state;
        }

        const tracked_ptr<detail::BufferedWriterState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::BufferedWriterState> _state;
    };

    namespace detail {
        template<>
        inline constexpr bool IsStreamHandle<buffered_writer> = true;
    }
}
