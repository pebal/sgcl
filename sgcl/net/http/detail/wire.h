//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "parser.h"
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
#include <string>

namespace sgcl::net::http::detail {
    using sgcl::io::detail::fail;

    // The block a head grows into past the connection's 8 KB: 32 KB, the
    // server's default limit in one object
    using HeadBlock = array<byte, config::io_copy_buffer_size>;

    // The bytes of a connection as HTTP reads them: a buffer in front of
    // it, 8 KB (io's block) to begin with, grown for a head that does not
    // fit (32 KB, then a vector for a limit past the size of an object).
    // A head is copied out of it into a string of exactly its size, which
    // the request or the response keeps and its fields are slices of; the
    // body is read through it (what is buffered first). As an io::reader
    // it is what is left of the connection after a head: what hijack hands
    // over.
    class Wire final : public io::mixin::reader<Wire> {
    public:
        explicit Wire(const net::connection& c)
        : _c(c) {
            tracked_ptr block = make_tracked<io::detail::IoBlock>();
            _data = block->data();
            _cap = block->size();
            _owner = tracked_ptr<const void>(block);
        }

        const net::connection& connection() const noexcept {
            return _c;
        }

        size_t buffered() const noexcept {
            return _end - _begin;
        }

        std::string_view view() const noexcept {
            return std::string_view(reinterpret_cast<const char*>(_data) + _begin, _end - _begin);
        }

        void consume(size_t n) noexcept {
            _begin += std::min(n, buffered());
            if (_begin == _end) {
                _begin = _end = 0;
            }
        }

        // More bytes from the connection into the buffer: how many, 0 at
        // the end of the stream
        async::task<expected<size_t, io::error>> fill() {
            if (_end == _cap) {
                if (_begin == 0) {
                    co_return size_t(0);   // full: the caller's limit decides
                }
                _compact();
            }
            slice<byte> room(_owner, _data + _end, _cap - _end);
            auto r = co_await _c.async_read(room);
            if (!r) {
                co_return fail(r);
            }
            _end += *r;
            co_return *r;
        }

        // A head: everything to the empty line that ends it, copied into a
        // string of its size, the empty lines before it skipped; nullopt
        // when the stream ended before a byte of it; header_too_large past
        // max bytes; io::errc::unexpected_eof when it ended inside
        async::task<expected<optional<string>, io::error>> read_head(size_t max) {
            size_t searched = 0;
            for (;;) {
                size_t skip = leading_empty_lines(reinterpret_cast<const char*>(_data) + _begin, buffered());
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
                auto r = co_await fill();
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

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            if (out.empty()) {
                co_return size_t(0);
            }
            if (buffered()) {
                size_t n = std::min(out.size(), buffered());
                std::memcpy(out.data(), _data + _begin, n);
                consume(n);
                co_return n;
            }
            co_return co_await _c.async_read(out);
        }

    private:
        void _compact() noexcept {
            size_t n = buffered();
            if (_begin && n) {
                std::memmove(_data, _data + _begin, n);
            }
            _begin = 0;
            _end = n;
        }

        // A buffer of at least `want` bytes, the buffered ones moved in
        void _grow(size_t want) {
            if (want <= _cap) {
                _compact();
                return;
            }
            size_t n = buffered();
            if (want <= config::io_copy_buffer_size) {
                tracked_ptr block = make_tracked<HeadBlock>();
                std::memcpy(block->data(), _data + _begin, n);
                _data = block->data();
                _cap = block->size();
                _owner = tracked_ptr<const void>(block);
            } else {
                // the old storage is held by _owner until the copy is made
                vector<byte> big(want);
                std::memcpy(big.data(), _data + _begin, n);
                auto owner = big.as_slice().owner();
                _big = std::move(big);
                _data = _big.data();
                _cap = _big.size();
                _owner = std::move(owner);
            }
            _begin = 0;
            _end = n;
        }

        net::connection _c;
        tracked_ptr<const void> _owner;   // the block _data is in
        vector<byte> _big;           // a head past 32 KB
        byte* _data = nullptr;
        size_t _cap = 0;
        size_t _begin = 0;
        size_t _end = 0;
        size_t _skipped = 0;
    };

    // A body as an io::reader: the framing over the wire, a limit, and
    // what is done at its end (the client gives the connection back to the
    // pool, the server goes on to the next request). A read past the limit
    // is net::errc::body_too_large; the framing broken is
    // malformed_response for a response and a 400 for a request (the
    // status kept in error_status for the server).
    class Body : public io::mixin::reader<Body> {
    public:
        Body(tracked_ptr<Wire> wire, BodyFraming framing, uint64_t limit, bool response)
        : _wire(std::move(wire)), _framing(framing), _limit(limit), _response(response) {
            _remaining = framing.length;
            if (framing.kind == Framing::none) {
                _done = true;
            }
        }

        expected<size_t, io::error> read(const slice<byte>& out) {
            return async_read(out).wait();
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
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

        // Everything to the end, bounded by the limit
        async::task<expected<vector<byte>, io::error>> read_everything() {
            vector<byte> all;
            for (;;) {
                size_t was = all.size();
                all.resize(was + 8192);
                auto r = co_await async_read(all.as_slice(was, 8192));
                if (!r) {
                    co_return fail(r);
                }
                all.resize(was + *r);
                if (*r == 0) {
                    co_return all;
                }
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
            return _chunked ? _chunked->trailers() : _no_trailers;
        }

        // Called once, before the first read: the server's 100 Continue
        void set_before_first_read(function<async::task<expected<void, io::error>>()> f) {
            _before = std::move(f);
        }

        // Called once at the end: clean when the body was read to its end
        void set_on_end(function<void(bool)> f) {
            _on_end = std::move(f);
            if (_done) {
                _finish(true);
            }
        }

        // Drops the rest of the body when it is at most `most` bytes and
        // can be read without waiting past the buffer (a response's close),
        // or reads it (a server between requests): true when the body came
        // to its end
        async::task<bool> discard(uint64_t most) {
            tracked_ptr block = make_tracked<io::detail::IoBlock>();
            uint64_t dropped = 0;
            while (!_done && !_failed) {
                slice<byte> room(block, block->data(), block->size());
                auto r = co_await async_read(room);
                if (!r || *r == 0) {
                    break;
                }
                dropped += *r;
                if (dropped > most) {
                    break;
                }
            }
            co_return _done && !_failed;
        }

        // What the buffer holds of the rest, read without waiting: whether
        // the body could be finished from it (a response's close())
        bool discard_buffered() {
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
            _finish(false);
        }

    private:
        async::task<expected<size_t, io::error>> _read(slice<byte> out) {
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

        struct Chunk {
            const byte* data = nullptr;
            size_t data_size = 0;
            int error = 0;
        };

        // One step of the decoder over the buffer; the data it hands back
        // stays in the buffer until the caller has copied it (the buffer is
        // consumed past it at once, which is safe: nothing refills it
        // before the copy)
        Chunk _chunked_step(size_t room) {
            if (!_chunked) {
                _chunked = make_tracked<ChunkedDecoder>();
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

        io::error _framing_error() {
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
        tracked_ptr<ChunkedDecoder> _chunked;
        headers _no_trailers;
        optional<io::error> _failed;
        function<async::task<expected<void, io::error>>()> _before;
        function<void(bool)> _on_end;
        int _error_status = 0;
        bool _done = false;
        bool _response;
    };
}
