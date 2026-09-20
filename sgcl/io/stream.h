//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "mixin/reader.h"
#include "mixin/seeker.h"
#include "mixin/writer.h"
#include "detail/bytes.h"
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
    // mixin over that primitive with everything else (mixin::reader,
    // mixin::writer, mixin::seeker: as mixin::enumerable is a mixin over begin() and
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

    // The mixins over the primitives: mixin/reader.h, mixin/writer.h,
    // mixin/seeker.h; the definitions that need reader and writer whole
    // (copy_to, copy_from) are below the classes.

    class reader
    : public mixin::reader<reader> {
    public:
        virtual ~reader() = default;
        virtual result<size_t> read(slice<std::byte> buffer) = 0;
        virtual task<result<size_t>> async_read(slice<std::byte> buffer) = 0;
    };

    class writer
    : public mixin::writer<writer> {
    public:
        virtual ~writer() = default;
        virtual result<size_t> write(slice<const std::byte> data) = 0;
        virtual task<result<size_t>> async_write(slice<const std::byte> data) = 0;
    };

    class seeker
    : public mixin::seeker<seeker> {
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
    class stream
    : public reader
    , public writer
    , public closer {
    };

    template<class Derived>
    result<size_t> mixin::reader<Derived>::copy_to(io::writer& w) {
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
    task<result<size_t>> mixin::reader<Derived>::async_copy_to(io::writer& w) {
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
    result<size_t> mixin::writer<Derived>::copy_from(io::reader& r) {
        return r.copy_to(_self());
    }

    template<class Derived>
    task<result<size_t>> mixin::writer<Derived>::async_copy_from(io::reader& r) {
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
