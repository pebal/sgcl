//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "functions.h"
#include "req.h"
#include "mixin/reader.h"
#include "mixin/seeker.h"
#include "mixin/writer.h"
#include "detail/bytes.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/detail/bytes.h"
#include "../core/detail/heap.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>

namespace sgcl::io {
    // The streams of io. A stream is whatever has the primitive (req.h):
    // read(slice<byte>) and write(slice<const byte>), which do
    // their work now, on the calling thread, and where a stream can wait
    // without holding a thread, async_read and async_write, which return a
    // task. No base class and nothing virtual on the side of the stream:
    // the functions of io (functions.h) take any of them, a lambda too, and
    // the handles below hold any of them where a stream must be kept (the
    // source of a buffered_reader, the output of an encoder). Errors are
    // values: expected<T, error> (error.h), never exceptions; the end of a stream
    // is not an error, a read returns 0.
    //
    // A stream's destructor runs on the collector's thread, after the
    // sweep that finds the object dead, which may be long after the last
    // use: a file or a socket is closed then, but its descriptor is held
    // until then. A stream that is done is close()d, which releases what
    // it holds now and reports the error a deferred close cannot.

    class reader;
    class writer;

    namespace detail {
        // The managed object a raw pointer lies in, or none: a stack
        // object, a global such as io::stdout, and an object a unique_ptr
        // owns, which no tracked_ptr may address (its unique_ptr keeps it,
        // as a scope keeps a stack object)
        inline tracked_ptr<const void> owner_of(const void* p) noexcept {
            if (p && Heap::page_of_checked(p) && Page::is_object(p) && !Page::is_unique(p)) {
                return tracked_ptr<const void>(p);
            }
            return tracked_ptr<const void>();
        }

        // A callable, or a stream given as a temporary, kept where a handle
        // can hold it: in a managed object of its own
        template<class T>
        struct Boxed {
            template<class U>
            explicit Boxed(U&& u)
            : value(std::forward<U>(u)) {
            }

            T value;
        };

        template<class T>
        inline constexpr bool IsTrackedPtr = false;

        template<class T>
        inline constexpr bool IsTrackedPtr<tracked_ptr<T>> = true;

        // Where a handle's object is and what keeps it: a tracked_ptr and
        // what it points at; a reference to an object with the methods, the
        // object itself (its managed object, if it lies in one, kept); a
        // callable or a temporary, a copy in a box of its own
        struct Bound {
            tracked_ptr<const void> owner;
            void* object;
        };

        template<class R>
        Bound bind(R&& r) {
            using S = std::remove_cvref_t<R>;
            using T = Target<R>;
            if constexpr (IsTrackedPtr<S>) {
                return Bound{tracked_ptr<const void>(r), const_cast<void*>(static_cast<const void*>(r.get()))};
            } else if constexpr (requires { typename S::element_type; requires std::is_rvalue_reference_v<R&&>; requires std::is_constructible_v<tracked_ptr<typename S::element_type>, S&&>; }) {
                tracked_ptr<typename S::element_type> p(std::move(r));   // a unique_ptr from make_tracked: held from now on
                return Bound{tracked_ptr<const void>(p), const_cast<void*>(static_cast<const void*>(p.get()))};
            } else if constexpr (PointerLike<R>) {
                void* p = const_cast<void*>(static_cast<const void*>(std::addressof(*r)));
                return Bound{owner_of(p), p};
            } else if constexpr (HasMembers<T> && std::is_lvalue_reference_v<R>) {
                void* p = const_cast<void*>(static_cast<const void*>(std::addressof(r)));
                return Bound{owner_of(p), p};
            } else {
                tracked_ptr<Boxed<S>> box = make_tracked<Boxed<S>>(std::forward<R>(r));
                void* p = std::addressof(box->value);
                return Bound{tracked_ptr<const void>(box), p};
            }
        }

        // The close of a stream that has none, or of an empty handle
        inline async::task<expected<void, error>> closed_now() {
            co_return expected<void, error>();
        }

        struct ReaderTable {
            expected<size_t, error> (*read)(const reader& self, void* object, const slice<byte>& buffer);   // by reference: a slice passed on by value is a tracked_ptr made at every step
            async::task<expected<size_t, error>> (*async_read)(const reader& self, void* object, slice<byte> buffer);
            expected<void, error> (*close)(const reader& self, void* object);
            async::task<expected<void, error>> (*async_close)(const reader& self, void* object);
            int (*fd)(void* object);
            bool has_read;
            bool has_async_read;
            bool has_close;
        };

        struct WriterTable {
            expected<size_t, error> (*write)(const writer& self, void* object, const slice<const byte>& data);
            async::task<expected<size_t, error>> (*async_write)(const writer& self, void* object, slice<const byte> data);
            expected<void, error> (*close)(const writer& self, void* object);
            async::task<expected<void, error>> (*async_close)(const writer& self, void* object);
            int (*fd)(void* object);
            bool has_write;
            bool has_async_write;
            bool has_close;
        };

        // The descriptor under a stream that has one (a file, a standard
        // stream, a socket), which a child process can take as it is
        template<class T>
        int fd_of(void* object) noexcept {
            if constexpr (requires(T& t) { { t.fd() } -> std::convertible_to<int>; }) {
                return static_cast<T*>(object)->fd();
            } else {
                (void)object;
                return -1;
            }
        }

        template<class T>
        const ReaderTable& reader_table() noexcept;

        template<class T>
        const WriterTable& writer_table() noexcept;
    }

    // Any reader, as a value: whatever meets req::reader or
    // req::async_reader, held as a tracked_ptr of the object that keeps it,
    // a pointer to it and a table of its methods (Go's interface value).
    // A stream given by reference is referenced, its managed object kept
    // if it lies in one (an object on a stack, a global or one a unique_ptr
    // owns is the caller's to keep alive), one given by tracked_ptr held by
    // it, a unique_ptr given as a temporary taken over; a callable or a temporary is copied into a
    // managed object of its own. The half the stream lacks is made from
    // the other, and only here: read of a stream that has only async_read
    // waits for its task on this thread, async_read of a stream that has
    // only read runs it on the blocking pool (async/blocking.h), which a
    // socket read waiting for data would hold; has_read() and
    // has_async_read() tell which halves are the stream's own. close and
    // async_close are the stream's when it has them, as a writer's are
    // (Go's ReadCloser): a file held as an io::reader is closed through it.
    class reader : public mixin::reader<reader> {
    public:
        reader() noexcept = default;

        template<class R>
        requires (!std::same_as<std::remove_cvref_t<R>, reader>) && (req::reader<R> || req::async_reader<R>)
        reader(R&& r)
        : reader(detail::bind(std::forward<R>(r)), &detail::reader_table<detail::Target<R>>()) {
        }

        // const, as a call through a pointer is: the handle is not what a
        // read changes
        expected<size_t, error> read(const slice<byte>& buffer) const {
            assert(_table && "a read of an empty io::reader");
            return _table->read(*this, _object, buffer);
        }

        async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const {
            assert(_table && "a read of an empty io::reader");
            return _table->async_read(*this, _object, buffer);
        }

        // The stream's close; of a stream that has none, nothing closed
        // and success. async_close of a stream that has only close runs it
        // on the blocking pool.
        expected<void, error> close() const {
            return _table ? _table->close(*this, _object) : expected<void, error>();
        }

        async::task<expected<void, error>> async_close() const {
            if (!_table) {
                return detail::closed_now();
            }
            return _table->async_close(*this, _object);
        }

        bool has_read() const noexcept {
            return _table && _table->has_read;
        }

        bool has_async_read() const noexcept {
            return _table && _table->has_async_read;
        }

        bool has_close() const noexcept {
            return _table && _table->has_close;
        }

        // The descriptor under the stream, -1 for a stream without one
        int fd() const noexcept {
            return _table ? _table->fd(_object) : -1;
        }

        explicit operator bool() const noexcept {
            return _table != nullptr;
        }

        // The same stream: the same object
        friend bool operator==(const reader& a, const reader& b) noexcept {
            return a._object == b._object;
        }

    private:
        reader(detail::Bound b, const detail::ReaderTable* table) noexcept
        : _owner(std::move(b.owner)), _object(b.object), _table(table) {
        }

        tracked_ptr<const void> _owner;
        void* _object = nullptr;
        const detail::ReaderTable* _table = nullptr;
    };

    // Any writer, as a value, as reader is; with the stream's close and
    // async_close when it has them (close of a stream that has none
    // closes nothing and succeeds, and so does its async_close, touching
    // no pool; async_close of a stream that has only close runs it on the
    // blocking pool)
    class writer : public mixin::writer<writer> {
    public:
        using mixin::writer<writer>::write;
        using mixin::writer<writer>::async_write;

        writer() noexcept = default;

        template<class W>
        requires (!std::same_as<std::remove_cvref_t<W>, writer>) && (req::writer<W> || req::async_writer<W>)
        writer(W&& w)
        : writer(detail::bind(std::forward<W>(w)), &detail::writer_table<detail::Target<W>>()) {
        }

        expected<size_t, error> write(const slice<const byte>& data) const {
            assert(_table && "a write to an empty io::writer");
            return _table->write(*this, _object, data);
        }

        async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const {
            assert(_table && "a write to an empty io::writer");
            return _table->async_write(*this, _object, data);
        }

        expected<void, error> close() const {
            return _table ? _table->close(*this, _object) : expected<void, error>();
        }

        async::task<expected<void, error>> async_close() const {
            if (!_table) {
                return detail::closed_now();
            }
            return _table->async_close(*this, _object);
        }

        bool has_write() const noexcept {
            return _table && _table->has_write;
        }

        bool has_async_write() const noexcept {
            return _table && _table->has_async_write;
        }

        bool has_close() const noexcept {
            return _table && _table->has_close;
        }

        // The descriptor under the stream, -1 for a stream without one
        int fd() const noexcept {
            return _table ? _table->fd(_object) : -1;
        }

        explicit operator bool() const noexcept {
            return _table != nullptr;
        }

        // The same stream: the same object
        friend bool operator==(const writer& a, const writer& b) noexcept {
            return a._object == b._object;
        }

    private:
        writer(detail::Bound b, const detail::WriterTable* table) noexcept
        : _owner(std::move(b.owner)), _object(b.object), _table(table) {
        }

        tracked_ptr<const void> _owner;
        void* _object = nullptr;
        const detail::WriterTable* _table = nullptr;
    };

    namespace detail {
        // The halves a stream lacks, made from the other: a task waited for
        // on this thread, or the blocking call on the pool (the handle
        // copied into the job, so the stream lives while it runs)
        inline async::task<expected<size_t, error>> read_on_pool(reader self, slice<byte> buffer) {
            co_return co_await async::spawn_blocking([self, buffer]() mutable { return self.read(buffer); });
        }

        inline async::task<expected<size_t, error>> write_on_pool(writer self, slice<const byte> data) {
            co_return co_await async::spawn_blocking([self, data]() mutable { return self.write(data); });
        }

        inline async::task<expected<void, error>> close_on_pool(writer self) {
            co_return co_await async::spawn_blocking([self]() mutable { return self.close(); });
        }

        inline async::task<expected<void, error>> close_on_pool(reader self) {
            co_return co_await async::spawn_blocking([self]() mutable { return self.close(); });
        }

        template<class T>
        const ReaderTable& reader_table() noexcept {
            static constexpr ReaderTable table = {
                [](const reader& self, void* object, const slice<byte>& buffer) -> expected<size_t, error> {
                    if constexpr (MemberRead<T> || CalledRead<T>) {
                        (void)self;
                        return call_read(*static_cast<T*>(object), buffer);
                    } else {
                        return async::spawn(self.async_read(buffer)).wait();
                    }
                },
                [](const reader& self, void* object, slice<byte> buffer) -> async::task<expected<size_t, error>> {
                    if constexpr (MemberAsyncRead<T> || CalledAsyncRead<T>) {
                        (void)self;
                        return call_async_read(*static_cast<T*>(object), std::move(buffer));
                    } else {
                        (void)object;
                        return read_on_pool(self, std::move(buffer));
                    }
                },
                [](const reader& self, void* object) -> expected<void, error> {
                    (void)self;
                    if constexpr (req::closer<T&>) {
                        return static_cast<T*>(object)->close();
                    } else if constexpr (req::async_closer<T&>) {
                        return async::spawn(static_cast<T*>(object)->async_close()).wait();
                    } else {
                        (void)object;
                        return expected<void, error>();
                    }
                },
                [](const reader& self, void* object) -> async::task<expected<void, error>> {
                    if constexpr (req::async_closer<T&>) {
                        (void)self;
                        return static_cast<T*>(object)->async_close();
                    } else if constexpr (req::closer<T&>) {
                        (void)object;
                        return close_on_pool(self);
                    } else {
                        (void)self;
                        (void)object;
                        return closed_now();
                    }
                },
                &fd_of<T>,
                MemberRead<T> || CalledRead<T>,
                MemberAsyncRead<T> || CalledAsyncRead<T>,
                req::closer<T&> || req::async_closer<T&>
            };
            return table;
        }

        template<class T>
        const WriterTable& writer_table() noexcept {
            static constexpr WriterTable table = {
                [](const writer& self, void* object, const slice<const byte>& data) -> expected<size_t, error> {
                    if constexpr (MemberWrite<T> || CalledWrite<T> || CalledVoidWrite<T>) {
                        (void)self;
                        return call_write(*static_cast<T*>(object), data);
                    } else {
                        return async::spawn(self.async_write(data)).wait();
                    }
                },
                [](const writer& self, void* object, slice<const byte> data) -> async::task<expected<size_t, error>> {
                    if constexpr (MemberAsyncWrite<T> || CalledAsyncWrite<T>) {
                        (void)self;
                        return call_async_write(*static_cast<T*>(object), std::move(data));
                    } else {
                        (void)object;
                        return write_on_pool(self, std::move(data));
                    }
                },
                [](const writer& self, void* object) -> expected<void, error> {
                    (void)self;
                    if constexpr (req::closer<T&>) {
                        return static_cast<T*>(object)->close();
                    } else if constexpr (req::async_closer<T&>) {
                        return async::spawn(static_cast<T*>(object)->async_close()).wait();
                    } else {
                        (void)object;
                        return expected<void, error>();
                    }
                },
                [](const writer& self, void* object) -> async::task<expected<void, error>> {
                    if constexpr (req::async_closer<T&>) {
                        (void)self;
                        return static_cast<T*>(object)->async_close();
                    } else if constexpr (req::closer<T&>) {
                        (void)object;
                        return close_on_pool(self);
                    } else {
                        (void)self;
                        (void)object;
                        return closed_now();
                    }
                },
                &fd_of<T>,
                MemberWrite<T> || CalledWrite<T> || CalledVoidWrite<T>,
                MemberAsyncWrite<T> || CalledAsyncWrite<T>,
                req::closer<T&> || req::async_closer<T&>
            };
            return table;
        }
    }

    // A reader of the first n bytes of r, then the end
    class limit_reader final : public mixin::reader<limit_reader> {
    public:
        limit_reader(const io::reader& r, uint64_t n) noexcept
        : _reader(r), _remaining(n) {
        }

        expected<size_t, error> read(const slice<byte>& buffer) {
            if (_remaining == 0 || buffer.empty()) {
                return 0;
            }
            auto r = _reader.read(buffer.first(static_cast<size_t>(std::min<uint64_t>(buffer.size(), _remaining))));
            if (r) {
                _remaining -= *r;
            }
            return r;
        }

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) {
            if (_remaining == 0 || buffer.empty()) {
                co_return 0;
            }
            auto r = co_await _reader.async_read(buffer.first(static_cast<size_t>(std::min<uint64_t>(buffer.size(), _remaining))));
            if (r) {
                _remaining -= *r;
            }
            co_return r;
        }

        uint64_t remaining() const noexcept {
            return _remaining;
        }

    private:
        io::reader _reader;
        uint64_t _remaining;
    };

    // A reader that writes what it reads to w as well (io.TeeReader); an
    // error of the write is the read's error
    class tee_reader final : public mixin::reader<tee_reader> {
    public:
        tee_reader(const io::reader& r, const io::writer& w) noexcept
        : _reader(r), _writer(w) {
        }

        expected<size_t, error> read(const slice<byte>& buffer) {
            auto r = _reader.read(buffer);
            if (r && *r) {
                auto w = _writer.write(buffer.first(*r));
                if (!w) {
                    return detail::fail(w);
                }
            }
            return r;
        }

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) {
            auto r = co_await _reader.async_read(buffer);
            if (r && *r) {
                auto w = co_await _writer.async_write(buffer.first(*r));
                if (!w) {
                    co_return detail::fail(w);
                }
            }
            co_return r;
        }

    private:
        io::reader _reader;
        io::writer _writer;
    };

    // The readers one after another, as one stream
    class multi_reader final : public mixin::reader<multi_reader> {
    public:
        explicit multi_reader(vector<io::reader> readers) noexcept   // by value: a vector's copy copies the elements, its move does not
        : _readers(std::move(readers)) {
        }

        expected<size_t, error> read(const slice<byte>& buffer) {
            while (_current < _readers.size()) {
                auto r = _readers[_current].read(buffer);
                if (!r || *r || buffer.empty()) {
                    return r;
                }
                ++_current;
            }
            return 0;
        }

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) {
            while (_current < _readers.size()) {
                auto r = co_await _readers[_current].async_read(buffer);
                if (!r || *r || buffer.empty()) {
                    co_return r;
                }
                ++_current;
            }
            co_return 0;
        }

    private:
        vector<io::reader> _readers;
        size_t _current = 0;
    };

    // A writer that writes to every one of the writers; the first error
    // stops it
    class multi_writer final : public mixin::writer<multi_writer> {
    public:
        using mixin::writer<multi_writer>::write;
        using mixin::writer<multi_writer>::async_write;

        explicit multi_writer(vector<io::writer> writers) noexcept
        : _writers(std::move(writers)) {
        }

        expected<size_t, error> write(const slice<const byte>& data) {
            for (auto& w : _writers) {
                auto r = w.write(data);
                if (!r) {
                    return r;
                }
            }
            return data.size();
        }

        async::task<expected<size_t, error>> async_write(slice<const byte> data) {
            for (auto& w : _writers) {
                auto r = co_await w.async_write(data);
                if (!r) {
                    co_return r;
                }
            }
            co_return data.size();
        }

    private:
        vector<io::writer> _writers;
    };

    // A reader that hands on what r reads after f has changed it in place
    // (f(slice<byte>) with the bytes just read): the most common
    // wrapper, in both forms, each over the source's own
    template<class F>
    class transform_reader final : public mixin::reader<transform_reader<F>> {
    public:
        transform_reader(const io::reader& r, F f)
        : _reader(r), _f(std::move(f)) {
        }

        expected<size_t, error> read(const slice<byte>& buffer) {
            auto r = _reader.read(buffer);
            if (r && *r) {
                _f(buffer.first(*r));
            }
            return r;
        }

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) {
            auto r = co_await _reader.async_read(buffer);
            if (r && *r) {
                _f(buffer.first(*r));
            }
            co_return r;
        }

    private:
        io::reader _reader;
        F _f;
    };

    template<class F>
    transform_reader(reader, F) -> transform_reader<F>;

    // The writer that drops everything (io.Discard): an object, as io::stdout
    // is, written to by reference: io::copy(io::discard, r)
    class discard_writer final : public mixin::writer<discard_writer> {
    public:
        using mixin::writer<discard_writer>::write;
        using mixin::writer<discard_writer>::async_write;

        expected<size_t, error> write(const slice<const byte>& data) noexcept {
            return data.size();
        }

        async::task<expected<size_t, error>> async_write(slice<const byte> data) {
            co_return data.size();
        }
    };

    inline discard_writer discard;

    // A growing block of bytes in memory that is read from the front and
    // written at the back (bytes.Buffer), a vector inside the managed
    // object. A read consumes; data() is what remains, a view valid until
    // the next write, text() the same as a string.
    class buffer final : public mixin::reader<buffer>, public mixin::writer<buffer> {
    public:
        using mixin::writer<buffer>::write;
        using mixin::writer<buffer>::async_write;

        buffer() = default;

        explicit buffer(const slice<const byte>& initial)
        : _data(initial.begin(), initial.end()) {
        }

        explicit buffer(const string& initial)
        : buffer(detail::bytes_of(initial)) {
        }

        expected<size_t, error> read(const slice<byte>& out) {
            if (out.empty()) {
                return 0;
            }
            size_t n = std::min(out.size(), size());
            if (n) {
                sgcl::detail::copy_bytes(out.data(), _data.data() + _read, n);
                _read += n;
                if (_read == _data.size()) {
                    clear();
                }
            }
            return n;
        }

        async::task<expected<size_t, error>> async_read(slice<byte> out) {
            co_return read(out);
        }

        expected<size_t, error> write(const slice<const byte>& in) {
            _data.insert(_data.end(), in.begin(), in.end());
            return in.size();
        }

        async::task<expected<size_t, error>> async_write(slice<const byte> in) {
            co_return write(in);
        }

        // What io::copy calls with a buffer as the source: what it holds,
        // in one write
        template<class W>
        expected<size_t, error> write_to(W& w) {
            auto r = detail::call_write(w, data());
            if (r) {
                clear();
            }
            return r;
        }

        // The same in a task, what io::async_copy calls: the writer is the
        // caller's to keep alive across the wait (async_copy's frame holds it)
        template<class W>
        async::task<expected<size_t, error>> async_write_to(W& w) {
            auto r = co_await detail::call_async_write(w, data());
            if (r) {
                clear();
            }
            co_return r;
        }

        slice<const byte> data() const noexcept {
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
        vector<byte> release() {
            vector<byte> out(_data.begin() + _read, _data.end());
            clear();
            return out;
        }

    private:
        vector<byte> _data;
        size_t _read = 0;   // the front: consumed up to here
    };
}
