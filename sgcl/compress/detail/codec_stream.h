//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "copy.h"
#include "stream.h"
#include "../error.h"
#include "../limits.h"
#include "../../async/scheduler.h"
#include "../../io/stream.h"

#include <algorithm>
#include <cstdint>
#include <memory>

// The streams of the formats written since the DEFLATE family — LZ4,
// Snappy, zstd, brotli, and bzip2's writer: one writer and one reader each,
// over an encoder and a decoder of the format, as xz's and bzip2's are.
//
// The encoder takes what is written and puts what it makes into the
// writer's managed output (block.h), which goes to the stream after every
// piece of 64 KB; it is made from the format's options, which it copies
// (a dictionary into plain memory: the encoder lives outside the managed
// heap), and says what is wrong with them, if anything, at the first
// write. Its interface:
//
//   static constexpr const char* name;              // "lz4": the stream's errors
//   explicit Encoder(const Options&) noexcept;
//   const char* setup_error() const noexcept;       // an option out of range, or nullptr
//   void start(ManagedOutput&) noexcept;            // the header
//   void write(const uint8_t*, size_t, ManagedOutput&) noexcept;
//   void flush(ManagedOutput&) noexcept;            // everything so far decodable
//   void finish(ManagedOutput&) noexcept;           // the end of the stream
//   void reset() noexcept;                          // a new stream, the same options
//
// The decoder is pushed: it takes every byte of input it is given (keeping
// what it cannot use yet) before it asks for more, and decodes into the
// caller's room. Its interface:
//
//   static constexpr const char* name;
//   Decoder(const Options&, const limits&) noexcept;
//   CodecStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept;
//   errc error; const char* error_text;             // the failure, when decode says failed
//   uint64_t offset() const noexcept;               // the byte of the input where it stands
//   void reset() noexcept;                          // a new stream
namespace sgcl::compress::detail {
    // The options of a format whose streams take none (Snappy)
    struct NoOptions {};

    enum class CodecStatus : uint8_t {
        done,         // the data ended, and nothing follows it
        need_input,   // every byte given was taken: more, please
        need_room,    // the room is full
        failed
    };

    // A whole decompress in memory through a pushed decoder (where the
    // format has no direct road of its own, or a case it does not take):
    // the result grows by doubling up to one byte past the limit
    template<class Decoder>
    expected<vector<byte>, error> decompress_with(Decoder& d, const uint8_t* p, size_t n, const limits& l, size_t guess) noexcept {
        const uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
        size_t capacity = size_t(std::min<uint64_t>(std::max<uint64_t>(guess, 1 << 16), ceiling));
        vector<byte> result;
        sgcl::detail::VectorOverwrite::resize(result, capacity);   // every byte written before it is returned: resize(pos) below
        const uint8_t* in = p;
        const uint8_t* end = p + n;
        size_t pos = 0;
        for (;;) {
            auto st = d.decode(in, end, true, reinterpret_cast<uint8_t*>(result.data()), pos, capacity);
            if (st == CodecStatus::done) {
                break;
            }
            if (st == CodecStatus::failed) {
                return unexpected<error>(error(d.error, d.offset(), d.error_text ? string(d.error_text) : string()));
            }
            if (st == CodecStatus::need_input) {   // final: the decoder fails rather than ask
                return unexpected<error>(error(errc::unexpected_end, n, string(std::string(Decoder::name) + ": unexpected end of data")));
            }
            if (capacity >= ceiling || pos > l.max_size) {
                return unexpected<error>(error(errc::too_large, d.offset(), string(std::string(Decoder::name) + ": decompressed data past the limit")));
            }
            capacity = size_t(std::min<uint64_t>(uint64_t(capacity) * 2, ceiling));
            vector<byte> grown;
            sgcl::detail::VectorOverwrite::resize(grown, capacity);
            copy_out(grown.data(), result.data(), pos);
            result = std::move(grown);
        }
        if (pos > l.max_size) {
            return unexpected<error>(error(errc::too_large, d.offset(), string(std::string(Decoder::name) + ": decompressed data past the limit")));
        }
        result.resize(pos);
        return result;
    }

    // What is written, compressed into out. The first failure — of out, of
    // an option out of range, a write or a flush after close — is kept:
    // every write, flush and close after it gives it at once and writes
    // nothing, so a stream may be written freely and checked once, at the
    // close; close() ends the stream and leaves out open.
    template<class Encoder, class Options>
    class CodecWriter
    : public io::mixin::writer<CodecWriter<Encoder, Options>> {
    public:
        using io::mixin::writer<CodecWriter<Encoder, Options>>::write;
        using io::mixin::writer<CodecWriter<Encoder, Options>>::async_write;

    private:
        static constexpr size_t Piece = size_t(64) << 10;

    public:
        SGCL_INLINE_HOT CodecWriter(const io::writer& out, const Options& o) noexcept
        : _out(out)
        , _options(o)
        , _encoder(std::make_unique<Encoder>(o)) {
        }

        CodecWriter(const CodecWriter&) = delete;
        CodecWriter& operator=(const CodecWriter&) = delete;

        // The other left closed, its stream and its encoder gone with the
        // move (its options kept): its writes give io::errc::closed, its
        // close does nothing, and a reset gives it a new stream. The
        // derived class assigns through move_into.
        CodecWriter(CodecWriter&& o) noexcept
        : _out(std::move(o._out))
        , _options(o._options)
        , _encoder(std::move(o._encoder))
        , _pending(std::move(o._pending))
        , _error(std::move(o._error))
        , _started(o._started)
        , _closed(o._closed) {
            o._out = io::writer();
            o._pending = ManagedOutput();
            o._error = nullopt;
            o._started = true;
            o._closed = true;
        }

        CodecWriter& operator=(CodecWriter&&) = delete;

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto e = _check("write")) {
                return io::detail::fail(*e);
            }
            const uint8_t* p = bytes(data);
            size_t n = data.size();
            while (n) {
                const size_t k = std::min(n, Piece);
                _encoder->write(p, k, _pending);
                p += k;
                n -= k;
                if (auto e = _drain()) {
                    return io::detail::fail(*e);
                }
            }
            return data.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
            if (auto e = _check("write")) {
                co_return io::detail::fail(*e);
            }
            const uint8_t* p = bytes(data);
            size_t n = data.size();
            while (n) {
                const size_t k = std::min(n, Piece);
                _encoder->write(p, k, _pending);
                p += k;
                n -= k;
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                if (n) {
                    co_await async::yield();
                }
            }
            co_return data.size();
        }

        // Everything written so far, compressed and handed to out, so that
        // a reader can decode it now
        expected<void, io::error> flush() {
            if (auto e = _check("flush")) {
                return io::detail::fail(*e);
            }
            _encoder->flush(_pending);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_flush() noexcept {
            if (auto e = _check("flush")) {
                co_return io::detail::fail(*e);
            }
            _encoder->flush(_pending);
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        // The end of the stream; out stays open. A second close does nothing.
        expected<void, io::error> close() {
            if (_closed && !_error) {
                return {};
            }
            if (auto e = _check("close")) {
                return io::detail::fail(*e);
            }
            _closed = true;
            _encoder->finish(_pending);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_close() noexcept {
            if (_closed && !_error) {
                co_return expected<void, io::error>();
            }
            if (auto e = _check("close")) {
                co_return io::detail::fail(*e);
            }
            _closed = true;
            _encoder->finish(_pending);
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _closed;
        }

        SGCL_INLINE_HOT const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same options: the encoder's
        // memory kept, nothing of the old stream carried over
        void reset(const io::writer& out) noexcept {
            _out = out;
            if (_encoder) {
                _encoder->reset();
            } else {
                _encoder = std::make_unique<Encoder>(_options);   // moved from: its encoder went with the move
            }
            _pending.clear();
            _started = false;
            _closed = false;
            _error = nullopt;
        }

    private:
        // the header before the first bytes, and the errors that stop everything
        optional<io::error> _check(const char* op) noexcept {
            if (_error) {
                return _error;
            }
            if (_closed) {
                _error = io::error(io::errc::closed, op, Encoder::name);
                return _error;
            }
            if (!_started) {
                _started = true;
                if (const char* wrong = _encoder->setup_error()) {
                    // "write zstd: a level of -131072..22: invalid argument"
                    std::string text = std::string(Encoder::name) + ": " + wrong;
                    _error = io::error(make_error_code(errc::invalid_argument), op, string(text));
                    return _error;
                }
                _encoder->start(_pending);
            }
            return nullopt;
        }

        optional<io::error> _drain() {
            if (_pending.empty()) {
                return nullopt;
            }
            auto w = _out.write(_pending.bytes());
            _pending.clear();
            if (!w) {
                _error = w.error();
                return _error;
            }
            return nullopt;
        }

        async::task<optional<io::error>> _async_drain() noexcept {
            if (_pending.empty()) {
                co_return nullopt;
            }
            auto w = co_await _out.async_write(_pending.bytes());   // the block's own managed bytes: the write may run on the pool
            _pending.clear();
            if (!w) {
                _error = w.error();
                co_return _error;
            }
            co_return nullopt;
        }

        io::writer _out;
        Options _options;
        std::unique_ptr<Encoder> _encoder;
        ManagedOutput _pending;
        optional<io::error> _error;
        bool _started = false;
        bool _closed = false;
    };

    // What the data read from in decompresses to. It reads its input 64 KB
    // at a time, so it may read past the end of the data; a read decodes
    // into the caller's room directly; the task's form does it in portions
    // of 64 KB with a yield between them. A failure is the error of the
    // read that reaches it and of every read after, last_error() the whole
    // of it.
    template<class Decoder, class Options>
    class CodecReader
    : public io::mixin::reader<CodecReader<Decoder, Options>> {
    private:
        static constexpr size_t InputBytes = size_t(64) << 10;
        static constexpr size_t ManagedInputBytes = size_t(32) << 10;   // a task's: the largest block of whole pages
        static constexpr size_t Portion = size_t(64) << 10;

    public:
        SGCL_INLINE_HOT CodecReader(const io::reader& in, const Options& o, const limits& l) noexcept
        : _in(in)
        , _options(o)
        , _limits(l)
        , _decoder(std::make_unique<Decoder>(o, l))
        , _input(InputBytes) {
        }

        CodecReader(const CodecReader&) = delete;
        CodecReader& operator=(const CodecReader&) = delete;

        // The other left without a stream, its decoder and its input gone
        // with the move: its reads give io::errc::closed, its close closes
        // nothing, and a reset gives it a new stream
        CodecReader(CodecReader&& o) noexcept
        : _in(std::move(o._in))
        , _options(o._options)
        , _limits(o._limits)
        , _decoder(std::move(o._decoder))
        , _input(std::move(o._input))
        , _in_begin(o._in_begin)
        , _in_end(o._in_end)
        , _source_ended(o._source_ended)
        , _ended(o._ended)
        , _error(std::move(o._error)) {
            o._in = io::reader();
            o._input = InputBuffer<ManagedInputBytes>(InputBytes);
            o._in_begin = o._in_end = 0;
            o._ended = true;
            o._error = moved_from_error(Decoder::name);
        }

        CodecReader& operator=(CodecReader&&) = delete;

        expected<size_t, io::error> read(const slice<byte>& out) {
            size_t pos = 0;
            for (;;) {
                if (_error) {
                    if (pos) {
                        return pos;
                    }
                    return io::detail::fail(to_io_error(*_error, Decoder::name));
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
                    co_return io::detail::fail(to_io_error(*_error, Decoder::name));
                }
                if (_ended || pos == out.size()) {
                    co_return pos;
                }
                const size_t before = pos;
                if (_step(out, pos, std::min(out.size(), pos + Portion))) {
                    if (pos) {
                        co_return pos;
                    }
                    if (auto e = co_await _async_fill()) {
                        co_return io::detail::fail(*e);
                    }
                } else if (pos != before && pos != out.size()) {
                    co_await async::yield();   // a portion made: the worker's other tasks before the next
                }
            }
        }

        // Closes in, as buffered_reader's close does
        SGCL_INLINE_HOT expected<void, io::error> close() {
            return _in.close();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close() noexcept {
            return _in.async_close();
        }

        SGCL_INLINE_HOT const optional<error>& last_error() const noexcept {
            return _error;
        }

        // A new stream from in: the decoder's memory kept
        void reset(const io::reader& in) noexcept {
            _in = in;
            if (_decoder) {
                _decoder->reset();
            } else {
                _decoder = std::make_unique<Decoder>(_options, _limits);   // moved from: the decoder went with the move
            }
            _in_begin = _in_end = 0;
            _source_ended = false;
            _ended = false;
            _error = nullopt;
        }

    protected:
        SGCL_INLINE_HOT const Decoder* _decoder_ptr() const noexcept {
            return _decoder.get();
        }

    private:
        // Decodes what the input holds into out[pos, cap); true when it
        // needs more input
        bool _step(const slice<byte>& out, size_t& pos, size_t cap) noexcept {
            const uint8_t* p = _input.data() + _in_begin;
            auto st = _decoder->decode(p, _input.data() + _in_end, _source_ended, reinterpret_cast<uint8_t*>(out.data()), pos, cap);
            _in_begin = size_t(p - _input.data());
            switch (st) {
                case CodecStatus::done:
                    _ended = true;
                    return false;
                case CodecStatus::failed:
                    _error = error(_decoder->error, _decoder->offset(), _decoder->error_text ? string(_decoder->error_text) : string());
                    return false;
                case CodecStatus::need_room:
                    return false;
                case CodecStatus::need_input:
                    break;
            }
            return true;
        }

        // The decoder takes every byte it is given, so the buffer is empty
        // when it asks for more
        SGCL_INLINE_HOT optional<io::error> _fill() {
            _in_begin = _in_end = 0;
            auto r = _in.read(_input.room(0, _input.size()));
            return _took(r);
        }

        async::task<optional<io::error>> _async_fill() noexcept {
            _in_begin = _in_end = 0;
            _input.to_managed(0);   // the read may run on the pool: into a managed block the slice holds
            auto r = co_await _in.async_read(_input.room(0, _input.size()));
            co_return _took(r);
        }

        SGCL_INLINE_HOT optional<io::error> _took(const expected<size_t, io::error>& r) noexcept {
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
        Options _options;
        limits _limits;
        std::unique_ptr<Decoder> _decoder;
        InputBuffer<ManagedInputBytes> _input;   // plain until a task's first read, managed from then on (block.h)
        size_t _in_begin = 0;   // the input not yet taken: [_in_begin, _in_end)
        size_t _in_end = 0;
        bool _source_ended = false;
        bool _ended = false;
        optional<error> _error;
    };
}
