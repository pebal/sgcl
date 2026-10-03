//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bzip2.h"
#include "detail/stream.h"

namespace sgcl::compress {
    // bzip2 1.0 (.bz2 files, .tar.bz2): the data in blocks of up to 900 KB,
    // each sorted by the Burrows-Wheeler transform and coded with Huffman
    // codes. Decompression only, as Go has it: a compressor needs the
    // sorting of suffixes and is left for when someone asks. Streams one
    // after another (cat a.bz2 b.bz2, and what pbzip2 writes) are read as
    // one; data after the last that is not another stream is an error.
    // Randomised blocks, which bzip2 stopped writing in 0.9.5, are
    // errc::unsupported, as Go refuses them.
    class bzip2 {
    public:
        using error = compress::error;

        class reader;

        // Every stream, one after another, up to the limit
        static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept {
            return decompress(data, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            const uint8_t* p = detail::bytes(data);
            const uint8_t* end = p + data.size();
            auto d = std::make_unique<detail::Bzip2Decoder>();
            d->reset();
            // one byte past the limit: a block whose last symbols make
            // nothing more must not be taken for more than the limit
            uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
            size_t capacity = size_t(std::min<uint64_t>(std::max<uint64_t>(uint64_t(data.size()) * 6, 1 << 16), ceiling));
            std::unique_ptr<uint8_t[]> out(new uint8_t[capacity]);
            size_t total = 0;
            for (;;) {
                auto st = d->decode(p, end, true, out.get(), total, capacity);
                if (st == detail::Bzip2Status::done) {
                    break;
                }
                if (st == detail::Bzip2Status::failed) {
                    return unexpected<error>(error(d->error, d->offset(), string(d->error_text)));
                }
                if (capacity >= ceiling) {
                    return unexpected<error>(error(errc::too_large, d->offset(), "bzip2: decompressed data past the limit"));
                }
                size_t grown = size_t(std::min<uint64_t>({uint64_t(capacity) * 2, ceiling, uint64_t(SIZE_MAX)}));
                std::unique_ptr<uint8_t[]> bigger(new uint8_t[grown]);
                sgcl::detail::copy_bytes(bigger.get(), out.get(), total);
                out = std::move(bigger);
                capacity = grown;
            }
            if (total > l.max_size) {
                return unexpected<error>(error(errc::too_large, d->offset(), "bzip2: decompressed data past the limit"));
            }
            return detail::to_vector(out.get(), total);
        }
    };

    // What the data read from in decompresses to, every stream of it. It
    // reads its input 64 KB at a time, so it may read past the end of the
    // bzip2 data. A block comes out only when the whole of it has been
    // read (the transform needs all of it): a stream's first bytes wait
    // for up to 900 KB of the data. A read decodes into the caller's
    // buffer directly; the task's form does it in portions of 64 KB with
    // a yield between them. A failure is the error of the read that
    // reaches it and of every read after, last_error() the whole of it.
    class bzip2::reader final
    : public io::mixin::reader<bzip2::reader> {
    public:
    private:
        static constexpr size_t InputBytes = size_t(64) << 10;
        static constexpr size_t ManagedInputBytes = size_t(32) << 10;   // a task's: the largest block of whole pages
        static constexpr size_t Portion = size_t(64) << 10;

    public:

        explicit reader(const io::reader& in) noexcept
        : _in(in)
        , _decoder(std::make_unique<detail::Bzip2Decoder>())
        , _input(InputBytes) {
            _decoder->reset();
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;

        // The other left without a stream, its decoder and its input gone
        // with the move: its reads give io::errc::closed, its close closes
        // nothing, and a reset gives it a new stream
        reader(reader&& o) noexcept
        : _in(std::move(o._in))
        , _decoder(std::move(o._decoder))
        , _input(std::move(o._input))
        , _in_begin(o._in_begin)
        , _in_end(o._in_end)
        , _source_ended(o._source_ended)
        , _ended(o._ended)
        , _error(std::move(o._error)) {
            o._in = io::reader();
            o._input = detail::InputBuffer<ManagedInputBytes>(InputBytes);
            o._in_begin = o._in_end = 0;
            o._ended = true;
            o._error = detail::moved_from_error("bzip2");
        }

        reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t pos = 0;
            for (;;) {
                if (_error) {
                    if (pos) {
                        return pos;
                    }
                    return io::detail::fail(detail::to_io_error(*_error, "bzip2"));
                }
                if (_ended || pos == out.size()) {
                    return pos;
                }
                if (_step(out, pos, out.size())) {
                    if (pos) {
                        return pos;
                    }
                    if (auto e = _fill()) {
                        return io::detail::fail(*e);
                    }
                }
            }
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            size_t pos = 0;
            for (;;) {
                if (_error) {
                    if (pos) {
                        co_return pos;
                    }
                    co_return io::detail::fail(detail::to_io_error(*_error, "bzip2"));
                }
                if (_ended || pos == out.size()) {
                    co_return pos;
                }
                size_t before = pos;
                if (_step(out, pos, std::min(out.size(), pos + Portion))) {
                    if (pos) {
                        co_return pos;
                    }
                    if (auto e = co_await _async_fill()) {
                        co_return io::detail::fail(*e);
                    }
                } else if (pos != before && pos != out.size()) {
                    // a portion made: the worker's other tasks before the next
                    co_await async::yield();
                }
            }
        }

        // Closes in, as buffered_reader's close does
        expected<void, io::error> close() {
            return _in.close();
        }

        async::task<expected<void, io::error>> async_close() noexcept {
            return _in.async_close();
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // A new stream from in: the decoder's memory kept
        void reset(const io::reader& in) noexcept {
            _in = in;
            if (!_decoder) {
                _decoder = std::make_unique<detail::Bzip2Decoder>();   // moved from: the decoder went with the move
            }
            _decoder->reset();
            _in_begin = _in_end = 0;
            _source_ended = false;
            _ended = false;
            _error = nullopt;
        }

    private:
        // Decodes what the input holds into out[pos, cap); true when it
        // needs more input
        bool _step(const slice<byte>& out, size_t& pos, size_t cap) noexcept {
            const uint8_t* p = _input.data() + _in_begin;
            auto st = _decoder->decode(p, _input.data() + _in_end, _source_ended, reinterpret_cast<uint8_t*>(out.data()), pos, cap);
            _in_begin = size_t(p - _input.data());
            switch (st) {
                case detail::Bzip2Status::done:
                    _ended = true;
                    return false;
                case detail::Bzip2Status::failed:
                    _error = error(_decoder->error, _decoder->offset(), string(_decoder->error_text));
                    return false;
                case detail::Bzip2Status::need_room:
                    return false;
                case detail::Bzip2Status::need_input:
                    break;
            }
            return true;
        }

        // The decoder takes every byte it is given, so the buffer is
        // empty when it asks for more
        optional<io::error> _fill() {
            _in_begin = _in_end = 0;
            auto r = _in.read(_input.room(0, _input.size()));
            return _took(r);
        }

        async::task<optional<io::error>> _async_fill() noexcept {
            _in_begin = _in_end = 0;
            _input.to_managed(0);   // the read may run on the pool: into a managed block of 32 KB, which the slice holds
            auto r = co_await _in.async_read(_input.room(0, _input.size()));
            co_return _took(r);
        }

        optional<io::error> _took(const expected<size_t, io::error>& r) noexcept {
            if (!r) {
                _error = error(r.error(), _decoder->offset());
                return r.error();
            }
            if (*r == 0) {
                _source_ended = true;
            }
            _in_end = *r;
            return nullopt;
        }

        io::reader _in;
        std::unique_ptr<detail::Bzip2Decoder> _decoder;
        detail::InputBuffer<ManagedInputBytes> _input;   // plain until a task's first read, managed from then on (detail/block.h)
        size_t _in_begin = 0;     // the input not yet taken: [_in_begin, _in_end)
        size_t _in_end = 0;
        bool _source_ended = false;
        bool _ended = false;
        optional<error> _error;
    };
}
