//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "parser.h"
#include "h2/stream.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../../async/coroutine.h"
#include "../../../core/array.h"
#include "../../../core/config.h"
#include "../../../core/function.h"
#include "../../../core/make_tracked.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"
#include "../../../io/buffered.h"
#include "../../../io/stream.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace sgcl::net::http::detail {
    using sgcl::io::detail::fail;

    // The bytes of a connection as HTTP reads them: a buffer in front of
    // it, 8 KB (io's block) to begin with, grown for a head that does not
    // fit (32 KB, the server's default limit, then the limit itself). A
    // head is copied out of it into a string of exactly its size, which
    // the request or the response keeps and its fields are slices of; the
    // body is read through it (what is buffered first, copied out). Nothing
    // hands out a slice of the buffer, so it is unmanaged memory the Wire
    // owns, freed with it. As an io::reader it is what is left of the
    // connection after a head: what hijack hands over.
    class Wire final : public io::mixin::reader<Wire> {
    public:
        explicit Wire(const net::connection& c) noexcept
        : _c(c), _data(std::make_unique_for_overwrite<byte[]>(config::io_buffer_size)), _cap(config::io_buffer_size) {
        }

        const net::connection& connection() const noexcept {
            return _c;
        }

        // The bytes of the response going out (a head with what follows it
        // in one send), kept from one exchange to the next with its
        // capacity: a response's head costs the system's allocator nothing
        // after the connection's first (WriterImpl: the one exchange that
        // writes at a time, its send awaited before the next begins)
        std::string out;

        // At the point the connection waits for its next request: past
        // OutKeep of room given back (a response written through `out`
        // whole, chunked or flushed, grew it to its size, and the room
        // stayed for the connection's life); the head's room stays
        static constexpr size_t OutKeep = size_t(64) << 10;

        void trim_out() noexcept {
            if (out.capacity() > OutKeep) {
                std::string().swap(out);
            }
        }

        size_t buffered() const noexcept {
            return _end - _begin;
        }

        // Room for at least n bytes buffered (HTTP/2: a frame whole)
        void reserve(size_t n) noexcept {
            if (n > _cap) {
                _grow(n);
            }
        }

        std::string_view view() const noexcept {
            return std::string_view(reinterpret_cast<const char*>(_data.get()) + _begin, _end - _begin);
        }

        void consume(size_t n) noexcept {
            _begin += std::min(n, buffered());
            if (_begin == _end) {
                _begin = _end = 0;
            }
        }

        // More bytes from the connection into the buffer: how many, 0 at
        // the end of the stream
        async::task<expected<size_t, io::error>> fill() noexcept {
            if (_end == _cap) {
                if (_begin == 0) {
                    co_return size_t(0);   // full: the caller's limit decides
                }
                _compact();
            }
            slice<byte> room(_data.get() + _end, _cap - _end);   // the frame holds this Wire, and so the buffer, until the read is done
            auto r = co_await _c.async_read(room);
            if (!r) {
                co_return fail(r);
            }
            _end += *r;
            co_return *r;
        }

        // One step of fill() without a frame of its own, for a coroutine
        // that loops over it: the connection tried (ConnImpl::try_read);
        // done with the result (bytes read into the buffer, 0 at the end of
        // the stream or with the buffer full, or the error); or `ready`,
        // the connection's readiness, for the caller to await in its own
        // frame and try again; or `slow`: the connection cannot be read
        // so, and the caller awaits fill(). A server's wait for the next
        // request and its head cost the frame of the coroutine that waits,
        // where the waiting way was three more (fill's, the connection's,
        // the transport's).
        //
        //     for (;;) {
        //         auto step = wire->try_fill();
        //         if (step.done) { r = step.result; break; }
        //         if (step.slow) { r = co_await wire->fill(); break; }
        //         if (auto ready = co_await step.ready; !ready) { r = fail(ready); break; }
        //     }
        struct FillStep {
            expected<size_t, io::error> result = size_t(0);
            bool done = false;
            bool slow = false;
            net::detail::readiness ready;
        };

        FillStep try_fill() {
            FillStep step;
            if (_end == _cap) {
                if (_begin == 0) {
                    step.done = true;   // full: the caller's limit decides
                    return step;
                }
                _compact();
            }
            slice<byte> room(_data.get() + _end, _cap - _end);
            auto& impl = net::detail::ConnectionAccess::impl(_c);
            auto r = impl.try_read(room, step.slow);
            if (step.slow) {
                return step;
            }
            if (!r) {
                step.result = fail(r);
                step.done = true;
                return step;
            }
            if (*r) {
                _end += **r;
                step.result = **r;
                step.done = true;
                return step;
            }
            step.ready = impl.raw_readable();
            return step;
        }

        // A head: everything to the empty line that ends it, copied into a
        // string of its size, the empty lines before it skipped; nullopt
        // when the stream ended before a byte of it; header_too_large past
        // max bytes; io::errc::unexpected_eof when it ended inside
        async::task<expected<optional<string>, io::error>> read_head(size_t max) noexcept {
            size_t searched = 0;
            for (;;) {
                size_t skip = leading_empty_lines(reinterpret_cast<const char*>(_data.get()) + _begin, buffered());
                if (skip) {
                    // at most a head's worth of them
                    _skipped += skip;
                    consume(skip);
                    searched = 0;
                    if (_skipped > max) {
                        co_return fail(net::detail::net_error(net::errc::header_too_large, "read", "head"));
                    }
                }
                auto v = view();
                size_t end = find_head_end(v.data(), std::min(v.size(), max), searched);
                if (end) {
                    string head(v.substr(0, end));
                    consume(end);
                    _skipped = 0;
                    co_return optional<string>(std::move(head));
                }
                if (buffered() >= max) {
                    co_return fail(net::detail::net_error(net::errc::header_too_large, "read", "head"));
                }
                searched = head_search_resume(buffered());
                if (_end == _cap) {
                    _grow(std::min(max, _cap * 4 > max ? max : _cap * 4));
                }
                bool was_empty = buffered() == 0;
                expected<size_t, io::error> r = size_t(0);
                for (;;) {   // fill() in this frame (try_fill)
                    auto step = try_fill();
                    if (step.done) {
                        r = std::move(step.result);
                        break;
                    }
                    if (step.slow) {
                        r = co_await fill();
                        break;
                    }
                    if (auto ready = co_await step.ready; !ready) {
                        r = fail(ready);
                        break;
                    }
                }
                if (!r) {
                    co_return fail(r);
                }
                if (*r == 0) {
                    if (was_empty && _skipped == 0) {
                        co_return optional<string>();
                    }
                    co_return fail(io::error(io::errc::unexpected_eof, "read", "head"));
                }
            }
        }

        // io::reader: the buffered bytes first, then the connection
        expected<size_t, io::error> read(const slice<byte>& out) {
            return async_read(out).wait();
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            if (out.empty()) {
                co_return size_t(0);
            }
            if (buffered()) {
                size_t n = std::min(out.size(), buffered());
                std::memcpy(out.data(), _data.get() + _begin, n);
                consume(n);
                co_return n;
            }
            co_return co_await _c.async_read(out);
        }

    private:
        void _compact() noexcept {
            size_t n = buffered();
            if (_begin && n) {
                std::memmove(_data.get(), _data.get() + _begin, n);
            }
            _begin = 0;
            _end = n;
        }

        // A buffer of at least `want` bytes, the buffered ones moved in
        void _grow(size_t want) noexcept {
            if (want <= _cap) {
                _compact();
                return;
            }
            size_t n = buffered();
            auto bigger = std::make_unique_for_overwrite<byte[]>(want);
            std::memcpy(bigger.get(), _data.get() + _begin, n);
            _data = std::move(bigger);
            _cap = want;
            _begin = 0;
            _end = n;
        }

        net::connection _c;
        std::unique_ptr<byte[]> _data;
        size_t _cap;
        size_t _begin = 0;
        size_t _end = 0;
        size_t _skipped = 0;
    };

    // The body of a request that has none, as a stream: at its end at
    // once. One for the program, with no state and nothing managed in it,
    // handed out by reference (an io::reader of it holds no owner), so
    // that a request without a body makes no object for its body().
    struct NoBody {
        expected<size_t, io::error> read(const slice<byte>&) const noexcept {
            return size_t(0);
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte>) const noexcept {
            co_return size_t(0);
        }
    };

    inline NoBody no_body;

    // A body as an io::reader: its source, a limit, and what is done at its
    // end (the client gives the connection or the stream back, the server
    // goes on to the next request or ends the handler's turn). The source
    // is one of two, chosen when the body is made: the wire of HTTP/1.1
    // with its framing, or an HTTP/2 stream (h2::StreamState: its DATA as
    // it came, its trailers, its end or its reset), on either side. A read
    // past the limit is net::errc::body_too_large; the framing broken is
    // malformed_response for a response and a 400 for a request (the
    // status kept in error_status for the server); a stream reset is
    // connection_reset. Given up before its end (discard past its bound,
    // abandon), an HTTP/2 body is reset with CANCEL, as Go does: HTTP/2 has
    // no connection to drain.
    class Body : public io::mixin::reader<Body> {
    public:
        Body(tracked_ptr<Wire> wire, BodyFraming framing, uint64_t limit, bool response) noexcept
        : _wire(std::move(wire)), _framing(framing), _limit(limit), _response(response) {
            _remaining = framing.length;
            if (framing.kind == Framing::none) {
                _done = true;
            }
        }

        // The body of an HTTP/2 stream; its content-length, when the head
        // gave one, lets read_everything read it straight into a vector
        // of its size (as a body of HTTP/1.1 with a length)
        Body(tracked_ptr<h2::StreamState> stream, uint64_t limit, bool response, optional<uint64_t> length = nullopt) noexcept
        : _framing{length ? Framing::length : Framing::until_close, length ? *length : 0}, _limit(limit), _response(response), _h2(std::move(stream)) {
            _remaining = _framing.length;
        }

        expected<size_t, io::error> read(const slice<byte>& out) {
            return async_read(out).wait();
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            if (_failed) {
                co_return fail(*_failed);
            }
            if (_done || out.empty()) {
                co_return size_t(0);
            }
            if (_before) {
                auto hook = std::move(_before);
                _before = {};
                auto r = co_await hook();
                if (!r) {
                    co_return _fail(r.error());
                }
            }
            auto r = co_await _read(out);
            if (!r) {
                co_return _fail(r.error());
            }
            _read_total += *r;
            if (_limit && _read_total > _limit) {
                _error_status = 413;
                co_return _fail(net::detail::net_error(net::errc::body_too_large, "read", "body"));
            }
            if (_done) {
                _finish(true);
            }
            co_return *r;
        }

        // Everything to the end, bounded by the limit. A length declared
        // (up to ExactUpTo, since a peer's number is not to be trusted with
        // more before its bytes come) is read straight into a vector of
        // that size; any other body is gathered in unmanaged memory and
        // copied once into a vector of its size. The reads are the wire's,
        // a copy out of its buffer or a read of the connection in this
        // task (the reactor, never the pool), so the gathered bytes are
        // written by nothing that outlives the frame.
        // The same as text (request::text, response::text): a length
        // declared read straight into the string's object, of its size;
        // any other body gathered and copied once into a string of its
        // size (a vector first and a string of it was two of each)
        async::task<expected<string, io::error>> read_text() noexcept {
            static constexpr uint64_t ExactUpTo = uint64_t(1) << 20;
            if (_framing.kind == Framing::length && _read_total == 0 && !_done && _framing.length && _framing.length <= ExactUpTo) {
                const size_t n = static_cast<size_t>(_framing.length);
                auto room = sgcl::detail::StringAccess::unfilled<string>(n);
                size_t got = 0;
                while (got < n) {
                    auto r = co_await async_read(slice<byte>(reinterpret_cast<byte*>(room.chars) + got, n - got));
                    if (!r) {
                        co_return fail(r);
                    }
                    if (*r == 0) {
                        break;
                    }
                    got += *r;
                }
                if (_h2 && !_done && !_failed) {
                    auto end = co_await _h2->wait_end();
                    if (!end) {
                        co_return fail(_fail(end.error()));
                    }
                    _done = true;
                    _finish(true);
                }
                co_return sgcl::detail::StringAccess::finish<string>(std::move(room), got);
            }
            io::detail::Gathered all;
            for (;;) {
                auto r = co_await async_read(all.room());
                if (!r) {
                    co_return fail(r);
                }
                if (*r == 0) {
                    co_return all.take_text();
                }
                all.added(*r);
            }
        }

        async::task<expected<vector<byte>, io::error>> read_everything() noexcept {
            static constexpr uint64_t ExactUpTo = uint64_t(1) << 20;
            if (_framing.kind == Framing::length && _read_total == 0 && !_done && _framing.length && _framing.length <= ExactUpTo) {
                vector<byte> all(static_cast<size_t>(_framing.length));
                size_t got = 0;
                while (got < all.size()) {
                    auto r = co_await async_read(all.as_slice(got, all.size() - got));
                    if (!r) {
                        co_return fail(r);
                    }
                    if (*r == 0) {
                        break;
                    }
                    got += *r;
                }
                all.resize(got);
                // HTTP/2: the length is met, but the stream may end later,
                // with trailers (HEADERS and END_STREAM after the DATA):
                // everything is its end, the trailers readable after it
                if (_h2 && !_done && !_failed) {
                    auto end = co_await _h2->wait_end();
                    if (!end) {
                        co_return fail(_fail(end.error()));
                    }
                    _done = true;
                    _finish(true);
                }
                co_return all;
            }
            io::detail::Gathered all;
            for (;;) {
                auto r = co_await async_read(all.room());
                if (!r) {
                    co_return fail(r);
                }
                if (*r == 0) {
                    co_return all.take();
                }
                all.added(*r);
            }
        }

        bool done() const noexcept {
            return _done;
        }

        bool failed() const noexcept {
            return (bool)_failed;
        }

        int error_status() const noexcept {
            return _error_status;
        }

        uint64_t read_total() const noexcept {
            return _read_total;
        }

        const headers& trailers() const noexcept {
            if (_h2) {
                return _done ? _h2->trailers_ref() : _no_trailers;
            }
            return _chunked ? _chunked->trailers() : _no_trailers;
        }

        bool http2() const noexcept {
            return (bool)_h2;
        }

        // Called once, before the first read: the server's 100 Continue
        void set_before_first_read(function<async::task<expected<void, io::error>>()> f) noexcept {
            _before = std::move(f);
        }

        // Called once at the end: clean when the body was read to its end
        void set_on_end(function<void(bool)> f) {
            _on_end = std::move(f);
            if (_done) {
                _finish(true);
            }
        }

        // Reads the rest of the body and drops it, up to about `most` bytes
        // (a server between requests): true when the body came to its end.
        // The bytes are dropped where they lie, in the wire's buffer, which
        // is refilled when they are gone: no block of its own, nothing
        // copied; counted against the limit as a read is.
        async::task<bool> discard(uint64_t most) noexcept {
            if (_h2) {
                co_return _h2_discard();
            }
            uint64_t dropped = 0;
            while (!_done && !_failed) {
                if (_before) {
                    auto hook = std::move(_before);
                    _before = {};
                    auto r = co_await hook();
                    if (!r) {
                        (void)_fail(r.error());
                        break;
                    }
                }
                if (!_wire->buffered()) {
                    auto r = co_await _wire->fill();
                    if (!r) {
                        (void)_fail(r.error());
                        break;
                    }
                    if (*r == 0) {
                        if (_framing.kind != Framing::until_close) {
                            (void)_fail(io::error(io::errc::unexpected_eof, "read", "body"));
                            break;
                        }
                        _done = true;
                    }
                }
                auto n = _drop_buffered();
                if (!n) {
                    (void)_fail(n.error());
                    break;
                }
                _read_total += *n;
                if (_limit && _read_total > _limit) {
                    _error_status = 413;
                    (void)_fail(net::detail::net_error(net::errc::body_too_large, "read", "body"));
                    break;
                }
                if (_done) {
                    _finish(true);
                    break;
                }
                dropped += *n;
                if (dropped > most) {
                    break;
                }
            }
            co_return _done && !_failed;
        }

        // What the buffer holds of the rest, read without waiting: whether
        // the body could be finished from it (a response's close())
        bool discard_buffered() {
            if (_h2) {
                return _h2_discard();
            }
            while (!_done && !_failed && _wire->buffered()) {
                if (_framing.kind == Framing::chunked) {
                    auto s = _chunked_step(size_t(-1));
                    if (s.error) {
                        return false;
                    }
                } else if (_framing.kind == Framing::length) {
                    size_t n = size_t(std::min<uint64_t>(_remaining, _wire->buffered()));
                    _wire->consume(n);
                    _remaining -= n;
                    if (_remaining == 0) {
                        _done = true;
                    }
                } else {
                    return false;
                }
            }
            if (_done) {
                _finish(true);
            }
            return _done;
        }

        void abandon() {
            if (_h2 && !_done && !_failed) {
                _h2->owner->reset(_h2->id, h2::ErrorCode::cancel);
            }
            _finish(false);
        }

    private:
        // HTTP/2: what came is dropped; before the end the stream is reset
        // (CANCEL): true when the body had ended
        bool _h2_discard() {
            if (_done) {
                return !_failed;
            }
            if (_h2->drop_buffered()) {
                _done = true;
                _finish(true);
                return true;
            }
            _h2->owner->reset(_h2->id, h2::ErrorCode::cancel);
            _finish(false);
            return false;
        }

        async::task<expected<size_t, io::error>> _read(slice<byte> out) noexcept {
            if (_h2) {
                auto r = co_await _h2->read(out);
                if (r && (*r == 0 || _h2->ended())) {
                    _done = true;   // the last bytes read: the end known without another read
                }
                co_return r;
            }
            switch (_framing.kind) {
                case Framing::none:
                    _done = true;
                    co_return size_t(0);
                case Framing::length: {
                    size_t want = size_t(std::min<uint64_t>(_remaining, out.size()));
                    auto r = co_await _wire->async_read(out.first(want));
                    if (!r) {
                        co_return fail(r);
                    }
                    if (*r == 0) {
                        co_return fail(io::error(io::errc::unexpected_eof, "read", "body"));
                    }
                    _remaining -= *r;
                    if (_remaining == 0) {
                        _done = true;
                    }
                    co_return *r;
                }
                case Framing::until_close: {
                    auto r = co_await _wire->async_read(out);
                    if (r && *r == 0) {
                        _done = true;
                    }
                    co_return r;
                }
                case Framing::chunked: {
                    for (;;) {
                        if (_wire->buffered()) {
                            auto s = _chunked_step(out.size());
                            if (s.error) {
                                co_return fail(_framing_error());
                            }
                            if (s.data_size) {
                                std::memcpy(out.data(), s.data, s.data_size);
                                co_return s.data_size;
                            }
                            if (_done) {
                                co_return size_t(0);
                            }
                            if (_wire->buffered()) {
                                continue;
                            }
                        }
                        auto r = co_await _wire->fill();
                        if (!r) {
                            co_return fail(r);
                        }
                        if (*r == 0) {
                            co_return fail(io::error(io::errc::unexpected_eof, "read", "body"));
                        }
                    }
                }
            }
            co_return size_t(0);
        }

        // The body's bytes in the wire's buffer dropped, as far as they go
        // (to its end at most): how many of the body's own there were, the
        // framing's left out; the framing broken is an error
        expected<size_t, io::error> _drop_buffered() noexcept {
            size_t n = 0;
            while (!_done && _wire->buffered()) {
                if (_framing.kind == Framing::chunked) {
                    size_t was = _wire->buffered();
                    auto s = _chunked_step(size_t(-1));
                    if (s.error) {
                        return fail(_framing_error());
                    }
                    n += s.data_size;
                    if (_wire->buffered() == was) {
                        break;   // a step that takes nothing waits for more bytes
                    }
                } else {
                    size_t k = _wire->buffered();
                    if (_framing.kind == Framing::length) {
                        k = size_t(std::min<uint64_t>(_remaining, k));
                        _remaining -= k;
                        if (_remaining == 0) {
                            _done = true;
                        }
                    }
                    _wire->consume(k);
                    n += k;
                }
            }
            return n;
        }

        struct Chunk {
            const byte* data = nullptr;
            size_t data_size = 0;
            int error = 0;
        };

        // One step of the decoder over the buffer; the data it hands back
        // stays in the buffer until the caller has copied it (the buffer is
        // consumed past it at once, which is safe: nothing refills it
        // before the copy)
        Chunk _chunked_step(size_t room) noexcept {
            if (!_chunked) {
                _chunked.emplace();
            }
            auto v = _wire->view();
            auto s = _chunked->step(v, room);
            Chunk c;
            if (s.error) {
                _error_status = 400;
                c.error = s.error;
                return c;
            }
            c.data = reinterpret_cast<const byte*>(v.data() + s.data_at);
            c.data_size = s.data_size;
            _wire->consume(s.consumed);
            if (s.done) {
                _done = true;
            }
            return c;
        }

        io::error _framing_error() noexcept {
            if (_response) {
                return net::detail::net_error(net::errc::malformed_response, "read", "body");
            }
            return io::error(error_code(EPROTO, std::system_category()), "read", "body");
        }

        expected<size_t, io::error> _fail(const io::error& e) {
            _failed = e;
            if (!_error_status) {
                _error_status = 400;
            }
            _finish(false);
            return fail(e);
        }

        void _finish(bool clean) {
            if (_on_end) {
                auto f = std::move(_on_end);
                _on_end = {};
                f(clean);
            }
        }

        tracked_ptr<Wire> _wire;
        BodyFraming _framing;
        uint64_t _limit;
        uint64_t _remaining = 0;
        uint64_t _read_total = 0;
        optional<ChunkedDecoder> _chunked;   // a chunked body's, made by its first step
        headers _no_trailers;
        optional<io::error> _failed;
        function<async::task<expected<void, io::error>>()> _before;
        function<void(bool)> _on_end;
        int _error_status = 0;
        bool _done = false;
        bool _response;
        tracked_ptr<h2::StreamState> _h2;
    };
}
