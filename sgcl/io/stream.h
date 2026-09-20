//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../async/coroutine.h"
#include "../containers/array.h"
#include "../containers/vector.h"
#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

namespace sgcl::io {
    // The streams of io: an interface of one primitive each (reader::read,
    // writer::write, seeker::seek, closer::close), pure virtual, and a
    // mixin over that primitive with everything else (m_reader,
    // m_writer, m_seeker: as m_enumerable is a mixin over begin() and
    // end()). A stream is a managed object held by a tracked_ptr; a
    // tracked_ptr<reader> holds any of them, so a buffered_reader, a
    // gzip reader or a TLS stream takes whatever reads and no template
    // parameter carries the type upward. Every operation exists twice:
    // read() takes the thread until the data comes, `co_await
    // async_read()` gives the worker back meanwhile. Errors are values:
    // result<T> (error.h), never exceptions; the end of a stream is not
    // an error, a read returns 0.
    //
    // A stream's destructor runs on the collector's thread, after the
    // sweep that finds the object dead, which may be long after the last
    // use: a file or a socket is closed then, but its descriptor is held
    // until then. A stream that is done is close()d, which releases what
    // it holds now and reports the error a deferred close cannot.
    //
    // An async operation is a coroutine of the stream: its frame holds
    // the stream by a plain `this`, so the caller keeps its tracked_ptr
    // for as long as it awaits, which a `co_await r->async_read(b)` in
    // a task does by itself.

    class reader;
    class writer;

    namespace detail {
        // The bytes of a string, as a slice that holds it; of a text
        // slice, the same owner; of a literal, none
        inline slice<const std::byte> bytes_of(const string& s) noexcept {
            return as_bytes(s.as_slice());
        }

        inline slice<const std::byte> bytes_of(const slice<const char>& s) noexcept {
            return as_bytes(s);
        }

        inline slice<const std::byte> bytes_of(const char* s) noexcept {
            return slice<const std::byte>(reinterpret_cast<const std::byte*>(s), std::char_traits<char>::length(s));
        }

        inline slice<const std::byte> bytes_of(std::string_view s) noexcept {
            return slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
        }

        // The bytes as characters: a std view (for the algorithms), a
        // slice of the same owner, a new string
        inline std::string_view chars_of(const slice<const std::byte>& b) noexcept {
            return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
        }

        inline slice<const char> text_slice_of(const slice<const std::byte>& b) noexcept {
            return slice<const char>(b.owner(), reinterpret_cast<const char*>(b.data()), b.size());
        }

        inline string text_of(const slice<const std::byte>& b) {
            return string(chars_of(b));
        }

        // The block copy() moves data through: one managed array, no header,
        // two to a page
        using CopyBlock = array<std::byte, config::IoCopyBufferSize>;
    }

    // The mixin over Derived::read(span) -> result<size_t>: the bytes
    // read, 0 at the end of the stream (a read of an empty span returns 0
    // without touching the stream), or the error. A read may return
    // fewer bytes than asked; read_full reads until the span is full or
    // the stream ends.
    template<class Derived>
    class m_reader {
    public:
        // Fills the whole span: the size, or an error; the stream ending
        // before the span is full is errc::unexpected_eof, unless it
        // ended before the first byte, which is 0.
        result<size_t> read_full(slice<std::byte> buffer) {
            size_t n = 0;
            while (n < buffer.size()) {
                auto r = _self().read(buffer.subspan(n));
                if (!r) {
                    return detail::fail(r);
                }
                if (*r == 0) {
                    return n ? detail::fail(error(errc::unexpected_eof, "read")) : result<size_t>(0);
                }
                n += *r;
            }
            return n;
        }

        // Everything to the end of the stream
        result<vector<std::byte>> read_all() {
            vector<std::byte> out;
            size_t n = 0;
            for (;;) {
                if (n == out.size()) {
                    out.resize(n ? n * 2 : config::IoBufferSize);
                }
                auto r = _self().read(out.as_slice(n));
                if (!r) {
                    return detail::fail(r);
                }
                if (*r == 0) {
                    break;
                }
                n += *r;
            }
            out.resize(n);
            return out;
        }

        result<string> read_all_text() {
            auto r = read_all();
            if (!r) {
                return detail::fail(r);
            }
            return detail::text_of(as_bytes(r->as_slice()));
        }

        // Everything to the end of the stream, written to w: the bytes
        // copied, config::IoCopyBufferSize (32 KB) at a time through a
        // managed array
        result<size_t> copy_to(writer& w);

        // The same, the task giving its worker back while it waits
        task<result<size_t>> async_read_full(slice<std::byte> buffer) {
            size_t n = 0;
            while (n < buffer.size()) {
                auto r = co_await _self().async_read(buffer.subspan(n));
                if (!r) {
                    co_return detail::fail(r);
                }
                if (*r == 0) {
                    co_return n ? result<size_t>(detail::fail(error(errc::unexpected_eof, "read"))) : result<size_t>(0);
                }
                n += *r;
            }
            co_return n;
        }

        task<result<vector<std::byte>>> async_read_all() {
            vector<std::byte> out;
            size_t n = 0;
            for (;;) {
                if (n == out.size()) {
                    out.resize(n ? n * 2 : config::IoBufferSize);
                }
                auto r = co_await _self().async_read(out.as_slice(n));
                if (!r) {
                    co_return detail::fail(r);
                }
                if (*r == 0) {
                    break;
                }
                n += *r;
            }
            out.resize(n);
            co_return out;
        }

        task<result<string>> async_read_all_text() {
            auto r = co_await async_read_all();
            if (!r) {
                co_return detail::fail(r);
            }
            co_return detail::text_of(as_bytes(r->as_slice()));
        }

        task<result<size_t>> async_copy_to(writer& w);

    protected:
        m_reader() = default;
        ~m_reader() = default;

    private:
        Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };

    // The mixin over Derived::write(span<const byte>) -> result<size_t>:
    // writes the whole span, as Go's Write, and returns its size; fewer
    // only with an error, which says how far it got. write_text is the
    // same bytes (a name of its own, so that a class overriding write
    // does not hide it): a string, a text slice (a line of a
    // buffered_reader, a piece of a string), a literal or a
    // std::string_view (a std::string's), the last three written from
    // where they lie, no string made; the literal's overload is also what
    // keeps it from being ambiguous between the string and the slice.
    template<class Derived>
    class m_writer {
    public:
        result<size_t> write_text(const string& text) {
            return _self().write(detail::bytes_of(text));
        }

        result<size_t> write_text(const slice<const char>& text) {
            return _self().write(detail::bytes_of(text));
        }

        result<size_t> write_text(const char* text) {
            return _self().write(detail::bytes_of(text));
        }

        result<size_t> write_text(std::string_view text) {
            return _self().write(detail::bytes_of(text));
        }

        result<size_t> write_byte(std::byte b) {
            return _self().write(slice<const std::byte>(&b, 1));
        }

        // Everything from r to its end, written here: the bytes copied
        result<size_t> copy_from(reader& r);

        task<result<size_t>> async_write_text(const string& text) {
            co_return co_await _self().async_write(detail::bytes_of(text));
        }

        task<result<size_t>> async_write_text(const slice<const char>& text) {
            co_return co_await _self().async_write(detail::bytes_of(text));
        }

        task<result<size_t>> async_write_text(const char* text) {
            co_return co_await _self().async_write(detail::bytes_of(text));
        }

        task<result<size_t>> async_write_text(std::string_view text) {
            co_return co_await _self().async_write(detail::bytes_of(text));
        }

        task<result<size_t>> async_copy_from(reader& r);

    protected:
        m_writer() = default;
        ~m_writer() = default;

    private:
        Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };

    enum class seek_from { begin, current, end };

    // The mixin over Derived::seek(offset, from) -> result<uint64_t>: the
    // position after the seek
    template<class Derived>
    class m_seeker {
    public:
        result<uint64_t> tell() {
            return _self().seek(0, seek_from::current);
        }

        // The size, the position kept
        result<uint64_t> size() {
            auto here = _self().seek(0, seek_from::current);
            if (!here) {
                return here;
            }
            auto end = _self().seek(0, seek_from::end);
            if (!end) {
                return end;
            }
            auto back = _self().seek(static_cast<int64_t>(*here), seek_from::begin);
            if (!back) {
                return back;
            }
            return end;
        }

        result<void> rewind() {
            auto r = _self().seek(0, seek_from::begin);
            if (!r) {
                return detail::fail(r);
            }
            return {};
        }

    protected:
        m_seeker() = default;
        ~m_seeker() = default;

    private:
        Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };

    class reader : public m_reader<reader> {
    public:
        virtual ~reader() = default;
        virtual result<size_t> read(slice<std::byte> buffer) = 0;
        virtual task<result<size_t>> async_read(slice<std::byte> buffer) = 0;
    };

    class writer : public m_writer<writer> {
    public:
        virtual ~writer() = default;
        virtual result<size_t> write(slice<const std::byte> data) = 0;
        virtual task<result<size_t>> async_write(slice<const std::byte> data) = 0;
    };

    class seeker : public m_seeker<seeker> {
    public:
        virtual ~seeker() = default;
        virtual result<uint64_t> seek(int64_t offset, seek_from from = seek_from::begin) = 0;
    };

    class closer {
    public:
        virtual ~closer() = default;
        // Releases what the stream holds; a second close does nothing and
        // succeeds; a read or write after it is errc::closed
        virtual result<void> close() = 0;
        virtual bool is_closed() const noexcept = 0;
    };

    // A stream that is read, written and closed: a file, a socket, a pipe
    class stream : public reader, public writer, public closer {};

    template<class Derived>
    result<size_t> m_reader<Derived>::copy_to(writer& w) {
        tracked_ptr<detail::CopyBlock> block = make_tracked<detail::CopyBlock>();
        size_t total = 0;
        for (;;) {
            slice<std::byte> room(block, block->data(), block->size());
            auto r = _self().read(room);
            if (!r) {
                return detail::fail(r);
            }
            if (*r == 0) {
                return total;
            }
            auto ww = w.write(room.first(*r));
            if (!ww) {
                return detail::fail(ww);
            }
            total += *r;
        }
    }

    template<class Derived>
    task<result<size_t>> m_reader<Derived>::async_copy_to(writer& w) {
        tracked_ptr<detail::CopyBlock> block = make_tracked<detail::CopyBlock>();
        size_t total = 0;
        for (;;) {
            slice<std::byte> room(block, block->data(), block->size());
            auto r = co_await _self().async_read(room);
            if (!r) {
                co_return detail::fail(r);
            }
            if (*r == 0) {
                co_return total;
            }
            auto ww = co_await w.async_write(room.first(*r));
            if (!ww) {
                co_return detail::fail(ww);
            }
            total += *r;
        }
    }

    template<class Derived>
    result<size_t> m_writer<Derived>::copy_from(reader& r) {
        return r.copy_to(_self());
    }

    template<class Derived>
    task<result<size_t>> m_writer<Derived>::async_copy_from(reader& r) {
        co_return co_await r.async_copy_to(_self());
    }

    // Copies r to its end into w: the bytes copied (r.copy_to(w))
    inline result<size_t> copy(writer& w, reader& r) {
        return r.copy_to(w);
    }

    inline task<result<size_t>> async_copy(writer& w, reader& r) {
        co_return co_await r.async_copy_to(w);
    }

    // A reader of the first n bytes of r, then the end
    class limit_reader final : public reader {
    public:
        limit_reader(tracked_ptr<reader> r, uint64_t n) noexcept
        : _reader(std::move(r)), _remaining(n) {
        }

        result<size_t> read(slice<std::byte> buffer) override {
            if (_remaining == 0 || buffer.empty()) {
                return 0;
            }
            auto r = _reader->read(buffer.first(static_cast<size_t>(std::min<uint64_t>(buffer.size(), _remaining))));
            if (r) {
                _remaining -= *r;
            }
            return r;
        }

        task<result<size_t>> async_read(slice<std::byte> buffer) override {
            if (_remaining == 0 || buffer.empty()) {
                co_return 0;
            }
            auto r = co_await _reader->async_read(buffer.first(static_cast<size_t>(std::min<uint64_t>(buffer.size(), _remaining))));
            if (r) {
                _remaining -= *r;
            }
            co_return r;
        }

        uint64_t remaining() const noexcept {
            return _remaining;
        }

    private:
        tracked_ptr<reader> _reader;
        uint64_t _remaining;
    };

    // A reader that writes what it reads to w as well (io.TeeReader); an
    // error of the write is the read's error
    class tee_reader final : public reader {
    public:
        tee_reader(tracked_ptr<reader> r, tracked_ptr<writer> w) noexcept
        : _reader(std::move(r)), _writer(std::move(w)) {
        }

        result<size_t> read(slice<std::byte> buffer) override {
            auto r = _reader->read(buffer);
            if (r && *r) {
                auto w = _writer->write(buffer.first(*r));
                if (!w) {
                    return detail::fail(w);
                }
            }
            return r;
        }

        task<result<size_t>> async_read(slice<std::byte> buffer) override {
            auto r = co_await _reader->async_read(buffer);
            if (r && *r) {
                auto w = co_await _writer->async_write(buffer.first(*r));
                if (!w) {
                    co_return detail::fail(w);
                }
            }
            co_return r;
        }

    private:
        tracked_ptr<reader> _reader;
        tracked_ptr<writer> _writer;
    };

    // The readers one after another, as one stream
    class multi_reader final : public reader {
    public:
        explicit multi_reader(vector<tracked_ptr<reader>> readers) noexcept
        : _readers(std::move(readers)) {
        }

        result<size_t> read(slice<std::byte> buffer) override {
            while (_current < _readers.size()) {
                auto r = _readers[_current]->read(buffer);
                if (!r || *r || buffer.empty()) {
                    return r;
                }
                ++_current;
            }
            return 0;
        }

        task<result<size_t>> async_read(slice<std::byte> buffer) override {
            while (_current < _readers.size()) {
                auto r = co_await _readers[_current]->async_read(buffer);
                if (!r || *r || buffer.empty()) {
                    co_return r;
                }
                ++_current;
            }
            co_return 0;
        }

    private:
        vector<tracked_ptr<reader>> _readers;
        size_t _current = 0;
    };

    // A writer that writes to every one of the writers; the first error
    // stops it
    class multi_writer final : public writer {
    public:
        explicit multi_writer(vector<tracked_ptr<writer>> writers) noexcept
        : _writers(std::move(writers)) {
        }

        result<size_t> write(slice<const std::byte> data) override {
            for (auto& w : _writers) {
                auto r = w->write(data);
                if (!r) {
                    return r;
                }
            }
            return data.size();
        }

        task<result<size_t>> async_write(slice<const std::byte> data) override {
            for (auto& w : _writers) {
                auto r = co_await w->async_write(data);
                if (!r) {
                    co_return r;
                }
            }
            co_return data.size();
        }

    private:
        vector<tracked_ptr<writer>> _writers;
    };

    // The writer that drops everything (io.Discard)
    class discard_writer final : public writer {
    public:
        result<size_t> write(slice<const std::byte> data) override {
            return data.size();
        }

        task<result<size_t>> async_write(slice<const std::byte> data) override {
            co_return data.size();
        }
    };

    inline tracked_ptr<writer> discard() {
        return make_tracked<discard_writer>();
    }

    // A growing block of bytes in memory that is read from the front and
    // written at the back (bytes.Buffer), a vector inside the managed
    // object. A read consumes; data() is what remains, a view valid until
    // the next write, text() the same as a string.
    class buffer final : public reader, public writer {
    public:
        buffer() = default;

        explicit buffer(const slice<const std::byte>& initial)
        : _data(initial.begin(), initial.end()) {
        }

        explicit buffer(const string& initial)
        : buffer(detail::bytes_of(initial)) {
        }

        result<size_t> read(slice<std::byte> out) override {
            if (out.empty()) {
                return 0;
            }
            size_t n = std::min(out.size(), size());
            if (n) {
                std::memcpy(out.data(), _data.data() + _read, n);
                _read += n;
                if (_read == _data.size()) {
                    clear();
                }
            }
            return n;
        }

        task<result<size_t>> async_read(slice<std::byte> out) override {
            co_return read(out);
        }

        result<size_t> write(slice<const std::byte> in) override {
            _data.insert(_data.end(), in.begin(), in.end());
            return in.size();
        }

        task<result<size_t>> async_write(slice<const std::byte> in) override {
            co_return write(in);
        }

        slice<const std::byte> data() const noexcept {
            return _data.as_slice(_read);
        }

        // What remains, as a string (a copy)
        string text() const {
            return detail::text_of(data());
        }

        size_t size() const noexcept {
            return _data.size() - _read;
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        void clear() noexcept {
            _data.clear();
            _read = 0;
        }

        void reserve(size_t n) {
            _data.reserve(_read + n);
        }

        // Takes the bytes out, leaving the buffer empty
        vector<std::byte> release() {
            vector<std::byte> out(_data.begin() + _read, _data.end());
            clear();
            return out;
        }

    private:
        vector<std::byte> _data;
        size_t _read = 0;   // the front: consumed up to here
    };
}
