//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/stream.h"

#include <stdexcept>

namespace sgcl::compress::detail {
    // LZW as GIF, TIFF and PDF use it, and Go's compress/lzw. The codes
    // below 2^w are the literals of w bits (the literal width, 2..8), 2^w
    // is clear (the table starts over), 2^w + 1 the end of the data, and
    // each code after the first since a clear makes a new one: the string
    // of the code before it and the first byte of its own. A code takes
    // w + 1 bits at the start and one more each time the next new code
    // reaches the next power of two, up to 12 bits (4096 codes). The codes
    // are packed least significant bit first (GIF) or most significant
    // first (TIFF, PDF).
    //
    // The width grows when the code just made is the first that needs the
    // wider width, not one code earlier: TIFF's "early change", which
    // libtiff writes, is not read here (Go does not either), and is left
    // for when a TIFF codec asks for it.
    inline constexpr uint32_t LzwMaxWidth = 12;
    inline constexpr uint32_t LzwCodes = 1u << LzwMaxWidth;

    SGCL_INLINE_HOT void lzw_check_width(int literal_width) {
        if (literal_width < 2 || literal_width > 8) {
            throw std::invalid_argument("compress::lzw: literal width outside 2..8");
        }
    }

    // The bits of the codes, into bytes
    struct LzwBitWriter {
        bool msb = false;
        uint32_t bits = 0;
        uint32_t count = 0;

        void put(uint32_t code, uint32_t width, std::vector<uint8_t>& out) noexcept {
            if (msb) {
                bits = bits << width | code;
                count += width;
                while (count >= 8) {
                    count -= 8;
                    out.push_back(uint8_t(bits >> count));
                }
            } else {
                bits |= code << count;
                count += width;
                while (count >= 8) {
                    out.push_back(uint8_t(bits));
                    bits >>= 8;
                    count -= 8;
                }
            }
        }

        SGCL_INLINE_HOT void flush(std::vector<uint8_t>& out) noexcept {
            if (count) {
                out.push_back(msb ? uint8_t(bits << (8 - count)) : uint8_t(bits));
            }
            bits = 0;
            count = 0;
        }
    };

    // The compressor: the longest string the table has, its code out, and
    // a new code for that string and the byte after it. It starts with a
    // clear code, as Go's does and GIF readers expect, and when the next
    // new code would be the last one (4095) it sends a clear code in its
    // place and starts over — Go's choice, so that the two make the same
    // bytes. A decoder makes each code one code later than the compressor
    // (it needs the next code to know the byte), so the width the
    // compressor sets after making a code is the one the decoder reads the
    // next code with.
    class LzwEncoder {
    public:
        SGCL_INLINE_HOT LzwEncoder(bool msb, int literal_width) noexcept
        : _clear(1u << literal_width)
        , _literal_width(uint32_t(literal_width))
        , _slots(std::make_unique<uint32_t[]>(Slots)) {
            _out.msb = msb;
            _start_over();
        }

        // The bytes compressed into out; false at a byte the literal width
        // cannot hold (the bytes before it are compressed)
        bool write(const uint8_t* p, size_t n, std::vector<uint8_t>& out) noexcept {
            if (!_started) {
                _started = true;
                _out.put(_clear, _width, out);
            }
            uint32_t limit = _clear;
            for (size_t i = 0; i < n; ++i) {
                uint32_t b = p[i];
                if (b >= limit) {
                    return false;
                }
                if (_current == None) {
                    _current = b;
                    continue;
                }
                uint32_t key = _current << 8 | b;
                uint32_t slot = _find(key);
                uint32_t found = _slots[slot];
                if (found) {
                    _current = found & (LzwCodes - 1);
                    continue;
                }
                _out.put(_current, _width, out);
                if (_make(out)) {
                    _slots[slot] = key << LzwMaxWidth | (_next - 1);
                }
                _current = b;
            }
            return true;
        }

        // A new stream with the same settings: the table cleared, nothing
        // of the old stream's string or bits kept
        SGCL_INLINE_HOT void reset() noexcept {
            _current = None;
            _started = false;
            _out.bits = 0;
            _out.count = 0;
            _start_over();
        }

        // The last string, the end code, and the last bits to a byte
        void finish(std::vector<uint8_t>& out) noexcept {
            if (!_started) {
                _started = true;
                _out.put(_clear, _width, out);
            }
            if (_current != None) {
                _out.put(_current, _width, out);
                _make(out);
                _current = None;
            }
            _out.put(_clear + 1, _width, out);
            _out.flush(out);
        }

    private:
        static constexpr uint32_t None = UINT32_MAX;
        static constexpr uint32_t Slots = 8192;   // twice the codes: short probes

        // A new code, after a code went out: false when the table was full
        // and a clear code went out in its place
        bool _make(std::vector<uint8_t>& out) noexcept {
            uint32_t code = _next++;
            if (code == (1u << _width)) {
                ++_width;
            }
            if (code == LzwCodes - 1) {
                _out.put(_clear, _width, out);
                _start_over();
                return false;
            }
            return true;
        }

        SGCL_INLINE_HOT void _start_over() noexcept {
            _width = _literal_width + 1;
            _next = _clear + 2;
            std::memset(_slots.get(), 0, Slots * sizeof(uint32_t));
        }

        // The slot of key: where it is, or the empty one where it goes. A
        // slot holds the key above the code; a code is never 0 there, as
        // the first code made is 2^w + 2.
        uint32_t _find(uint32_t key) const noexcept {
            uint32_t s = (key * 2654435761u) >> (32 - 13);
            for (;;) {
                uint32_t v = _slots[s];
                if (!v || (v >> LzwMaxWidth) == key) {
                    return s;
                }
                s = (s + 1) & (Slots - 1);
            }
        }

        uint32_t _clear;
        uint32_t _literal_width;
        uint32_t _width = 0;
        uint32_t _next = 0;
        uint32_t _current = None;   // the code of the string read so far
        bool _started = false;
        LzwBitWriter _out;
        std::unique_ptr<uint32_t[]> _slots;
    };

    enum class LzwStatus : uint8_t {
        need_input,   // every byte given was taken; more is needed
        need_room,    // the next string does not fit the output
        done,         // the end code was read
        failed
    };

    // The decompressor, resumable at any byte of its input and at any
    // string of its output. A string is kept as the code of the string one
    // byte shorter and its last byte, with its length and its first byte,
    // so that it is written from its end, into its place, at once. The
    // table ends at 4096 codes and stays full until a clear comes, adding
    // nothing (the "deferred clear" GIF encoders use; Go reads it the same
    // way).
    class LzwDecoder {
    public:
        errc error = errc::corrupt;
        const char* error_text = nullptr;

        SGCL_INLINE_HOT LzwDecoder(bool msb, int literal_width) noexcept
        : _msb(msb)
        , _clear(1u << literal_width)
        , _literal_width(uint32_t(literal_width)) {
            reset();
        }

        void reset() noexcept {
            for (uint32_t c = 0; c < _clear; ++c) {
                _prefix[c] = 0;
                _suffix[c] = uint8_t(c);
                _first[c] = uint8_t(c);
                _length[c] = 1;
            }
            _start_over();
            _bits = 0;
            _count = 0;
            _pulled = 0;
            _done = false;
            _failed = false;
            error = errc::corrupt;
            error_text = nullptr;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _pulled - _count / 8;
        }

        // Decodes from [in, end) into out[pos, cap); final says that no
        // input follows end
        LzwStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept {
            if (_failed) {
                return LzwStatus::failed;
            }
            if (_done) {
                return LzwStatus::done;
            }
            for (;;) {
                while (_count <= 56 && in < end) {
                    uint64_t b = *in++;
                    _bits = _msb ? _bits << 8 | b : _bits | b << _count;
                    _count += 8;
                    ++_pulled;
                }
                if (_count < _width) {
                    if (final) {
                        return _fail(errc::unexpected_end, "lzw: unexpected end of the data (no end code)");
                    }
                    return LzwStatus::need_input;
                }
                uint32_t mask = (1u << _width) - 1;
                uint32_t code = _msb ? uint32_t(_bits >> (_count - _width)) & mask : uint32_t(_bits) & mask;
                uint32_t length;
                if (code < _clear) {
                    length = 1;
                } else if (code == _clear) {
                    _take();
                    _start_over();
                    continue;
                } else if (code == _clear + 1) {
                    _take();
                    _done = true;
                    return LzwStatus::done;
                } else if (code < _next) {
                    length = _length[code];
                } else if (code == _next && _previous != None) {
                    length = _length[_previous] + 1u;   // the string of the code before and its own first byte
                } else {
                    return _fail(errc::corrupt, "lzw: a code past the table");
                }
                if (cap - pos < length) {
                    return LzwStatus::need_room;
                }
                _take();
                if (_previous != None && _next < LzwCodes) {
                    uint32_t n = _next++;
                    _prefix[n] = uint16_t(_previous);
                    _first[n] = _first[_previous];
                    _suffix[n] = code == n ? _first[_previous] : _first[code];
                    _length[n] = uint16_t(_length[_previous] + 1);
                    if (_next == (1u << _width) && _width < LzwMaxWidth) {
                        ++_width;
                    }
                }
                uint8_t* at = out + pos + length;
                for (uint32_t c = code; at != out + pos;) {
                    *--at = _suffix[c];
                    c = _prefix[c];
                }
                pos += length;
                _previous = code;
            }
        }

    private:
        static constexpr uint32_t None = UINT32_MAX;

        SGCL_INLINE_HOT void _take() noexcept {
            _count -= _width;
            if (!_msb) {
                _bits >>= _width;
            }
        }

        SGCL_INLINE_HOT void _start_over() noexcept {
            _width = _literal_width + 1;
            _next = _clear + 2;
            _previous = None;
        }

        LzwStatus _fail(errc code, const char* text) noexcept {
            error = code;
            error_text = text;
            _failed = true;
            return LzwStatus::failed;
        }

        bool _msb;
        uint32_t _clear;
        uint32_t _literal_width;
        uint32_t _width = 0;
        uint32_t _next = 0;
        uint32_t _previous = None;
        uint64_t _bits = 0;
        uint32_t _count = 0;
        uint64_t _pulled = 0;
        bool _done = false;
        bool _failed = false;
        uint16_t _prefix[LzwCodes];
        uint8_t _suffix[LzwCodes];
        uint8_t _first[LzwCodes];
        uint16_t _length[LzwCodes];
    };
}

namespace sgcl::compress {
    // LZW (Go's compress/lzw): GIF's codes least significant bit first,
    // TIFF's and PDF's most significant first, literals of 2 to 8 bits
    // (GIF's image data is 2..8, TIFF's and PDF's 8). A literal width
    // outside 2..8 is a mistake of the program, std::invalid_argument; a
    // byte the width cannot hold (5 with a width of 2) is invalid_argument
    // too, thrown by compress and the error of the writer's write.
    // Decompression stops at the end code; what follows it is not read.
    class lzw {
    public:
        using error = compress::error;

        // The order of the bits in the bytes: a value of the data's format
        enum class order : uint8_t {
            lsb,   // GIF
            msb    // TIFF, PDF
        };

        class writer;
        class reader;

        static vector<byte> compress(const slice<const byte>& data, order o, int literal_width) {
            detail::lzw_check_width(literal_width);
            detail::LzwEncoder e(o == order::msb, literal_width);
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(data.size() / 2 + 16);
            if (!e.write(detail::bytes(data), data.size(), out)) {
                throw std::invalid_argument("compress::lzw: a byte past the literal width");
            }
            e.finish(out);
            return detail::to_vector(out.data(), out.size());
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data, order o, int literal_width) {
            return decompress(data, o, literal_width, limits{});
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data, order o, int literal_width, const limits& l) {
            detail::lzw_check_width(literal_width);
            auto d = std::make_unique<detail::LzwDecoder>(o == order::msb, literal_width);
            const uint8_t* p = detail::bytes(data);
            const uint8_t* end = p + data.size();
            size_t capacity = size_t(std::min<uint64_t>(std::max<uint64_t>(uint64_t(data.size()) * 3, 1 << 12), l.max_size));
            std::unique_ptr<uint8_t[]> out(new uint8_t[std::max<size_t>(capacity, 1)]);
            size_t total = 0;
            for (;;) {
                auto st = d->decode(p, end, true, out.get(), total, capacity);
                if (st == detail::LzwStatus::done) {
                    break;
                }
                if (st == detail::LzwStatus::failed) {
                    return unexpected<error>(error(d->error, d->offset(), string(d->error_text)));
                }
                // need_room: the next string passes the output
                if (capacity >= l.max_size) {
                    return unexpected<error>(error(errc::too_large, d->offset(), "lzw: decompressed data past the limit"));
                }
                size_t grown = size_t(std::min<uint64_t>({uint64_t(capacity) * 2 + detail::LzwCodes, l.max_size, uint64_t(SIZE_MAX)}));
                std::unique_ptr<uint8_t[]> bigger(new uint8_t[grown]);
                sgcl::detail::copy_bytes(bigger.get(), out.get(), total);
                out = std::move(bigger);
                capacity = grown;
            }
            return detail::to_vector(out.get(), total);
        }
    };

    // What is written, compressed into out, 64 KB of input at a time (the
    // task's form yields between them); close() writes the end code and
    // leaves out open, as Go's does. LZW has no point to flush at short of
    // the end. A failure of out, or a byte the literal width cannot hold,
    // is kept for good (last_error()) until a reset.
    class lzw::writer final
    : public io::mixin::writer<lzw::writer> {
    public:
        using io::mixin::writer<lzw::writer>::write;
        using io::mixin::writer<lzw::writer>::async_write;
    private:
        static constexpr size_t Piece = size_t(64) << 10;

    public:

        SGCL_INLINE_HOT writer(const io::writer& out, order o, int literal_width)
        : _out(out)
        , _encoder((detail::lzw_check_width(literal_width), std::make_unique<detail::LzwEncoder>(o == order::msb, literal_width)))
        , _order(o)
        , _literal_width(literal_width) {
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;

        // The other left closed, its stream and its encoder gone with the
        // move (its order and width kept): its writes give
        // io::errc::closed, its close does nothing, and a reset gives it a
        // new stream
        SGCL_INLINE_HOT writer(writer&& o) noexcept
        : _out(std::move(o._out))
        , _encoder(std::move(o._encoder))
        , _pending(std::move(o._pending))
        , _stage(std::move(o._stage))
        , _error(std::move(o._error))
        , _closed(o._closed)
        , _order(o._order)
        , _literal_width(o._literal_width) {
            o._out = io::writer();
            o._pending = std::vector<uint8_t>();
            o._stage = detail::OutputStage();
            o._error = nullopt;
            o._closed = true;
        }

        SGCL_INLINE_HOT writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto e = _check("write")) {
                return io::detail::fail(*e);
            }
            auto p = detail::bytes(data);
            size_t n = data.size();
            while (n) {
                size_t k = std::min(n, Piece);
                bool ok = _encoder->write(p, k, _pending);
                if (auto e = _drain()) {
                    return io::detail::fail(*e);
                }
                if (!ok) {
                    return io::detail::fail(*_invalid("write"));
                }
                p += k;
                n -= k;
            }
            return data.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
            if (auto e = _check("write")) {
                co_return io::detail::fail(*e);
            }
            auto p = detail::bytes(data);
            size_t n = data.size();
            while (n) {
                size_t k = std::min(n, Piece);
                bool ok = _encoder->write(p, k, _pending);
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                if (!ok) {
                    co_return io::detail::fail(*_invalid("write"));
                }
                p += k;
                n -= k;
                if (n) {
                    co_await async::yield();
                }
            }
            co_return data.size();
        }

        // The end code; out stays open. A second close does nothing.
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

        // The first failure (of out, or a byte past the literal width), kept
        SGCL_INLINE_HOT const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same order and width: the
        // encoder's memory (the table) kept, nothing of the old stream
        // carried over
        void reset(const io::writer& out) noexcept {
            _out = out;
            if (_encoder) {
                _encoder->reset();
            } else {
                _encoder = std::make_unique<detail::LzwEncoder>(_order == order::msb, _literal_width);   // moved from: its encoder went with the move
            }
            _pending.clear();
            _closed = false;
            _error = nullopt;
        }

    private:
        SGCL_INLINE_HOT optional<io::error> _check(const char* op) noexcept {
            if (_error) {
                return _error;
            }
            if (_closed) {
                _error = io::error(io::errc::closed, op, "lzw");   // kept as every error
                return _error;
            }
            return nullopt;
        }

        optional<io::error> _invalid(const char* op) noexcept {
            _error = io::error(make_error_code(errc::invalid_argument), op, "lzw");
            return _error;
        }

        optional<io::error> _drain() {
            if (_pending.empty()) {
                return nullopt;
            }
            auto w = _out.write(detail::view(_pending));
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
            auto w = co_await _out.async_write(_stage.stage(_pending));   // a task's write may run on the pool: the bytes it is given are managed
            _pending.clear();
            if (!w) {
                _error = w.error();
                co_return _error;
            }
            co_return nullopt;
        }

        io::writer _out;
        std::unique_ptr<detail::LzwEncoder> _encoder;
        std::vector<uint8_t> _pending;
        detail::OutputStage _stage;   // the pending bytes for a task's write (detail/block.h)
        optional<io::error> _error;
        bool _closed = false;
        order _order;
        int _literal_width;
    };

    // What the data read from in decompresses to, up to the end code. It
    // reads its input 16 KB at a time, so it may read past the end code;
    // a read decodes up to 64 KB (the buffer it hands out from). A failure
    // is the error of the read that reaches it and of every read after,
    // last_error() the whole of it.
    class lzw::reader final
    : public io::mixin::reader<lzw::reader> {
    public:
    private:
        static constexpr size_t InputBytes = size_t(16) << 10;
        static constexpr size_t OutputBytes = size_t(64) << 10;

    public:

        SGCL_INLINE_HOT reader(const io::reader& in, order o, int literal_width)
        : _in(in)
        , _decoder((detail::lzw_check_width(literal_width), std::make_unique<detail::LzwDecoder>(o == order::msb, literal_width)))
        , _input(InputBytes)
        , _output(OutputBytes)
        , _order(o)
        , _literal_width(literal_width) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;

        // The other left without a stream, its decoder and its buffers gone
        // with the move (its order and width kept): its reads give
        // io::errc::closed, its close closes nothing, and a reset gives it
        // a new stream
        reader(reader&& o) noexcept
        : _in(std::move(o._in))
        , _decoder(std::move(o._decoder))
        , _input(std::move(o._input))
        , _output(std::move(o._output))
        , _pos(o._pos)
        , _from(o._from)
        , _in_begin(o._in_begin)
        , _in_end(o._in_end)
        , _source_ended(o._source_ended)
        , _ended(o._ended)
        , _error(std::move(o._error))
        , _since_yield(o._since_yield)
        , _order(o._order)
        , _literal_width(o._literal_width) {
            o._in = io::reader();
            o._input = detail::InputBuffer<InputBytes>(InputBytes);
            o._output = std::vector<uint8_t>();
            o._pos = o._from = o._in_begin = o._in_end = 0;
            o._ended = true;
            o._error = detail::moved_from_error("lzw");
        }

        SGCL_INLINE_HOT reader& operator=(reader&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        expected<size_t, io::error> read(const slice<byte>& out) {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    return n;
                }
                if (_error) {
                    return io::detail::fail(detail::to_io_error(*_error, "lzw"));
                }
                if (_ended || out.empty()) {
                    return 0;
                }
                if (_step()) {
                    if (auto e = _fill()) {
                        return io::detail::fail(*e);
                    }
                }
            }
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    // decoding is work, not a wait: every 64 KB handed out
                    // the task lets the worker go, so that a read of it all
                    // does not starve the other tasks
                    _since_yield += n;
                    if (_since_yield >= YieldEvery) {
                        _since_yield = 0;
                        co_await async::yield();
                    }
                    co_return n;
                }
                if (_error) {
                    co_return io::detail::fail(detail::to_io_error(*_error, "lzw"));
                }
                if (_ended || out.empty()) {
                    co_return 0;
                }
                if (_step()) {
                    if (auto e = co_await _async_fill()) {
                        co_return io::detail::fail(*e);
                    }
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

        // A new stream from in, with the same order and width: the
        // decoder's memory kept
        void reset(const io::reader& in) noexcept {
            _in = in;
            if (_decoder) {
                _decoder->reset();
            } else {
                // moved from: the decoder and the output went with the move
                _decoder = std::make_unique<detail::LzwDecoder>(_order == order::msb, _literal_width);
                _output.assign(OutputBytes, 0);
            }
            _pos = _from = 0;
            _in_begin = _in_end = 0;
            _source_ended = false;
            _ended = false;
            _error = nullopt;
        }

    private:
        SGCL_INLINE_HOT size_t _hand_out(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), _pos - _from);
            if (n) {
                sgcl::detail::copy_bytes(out.data(), _output.data() + _from, n);
                _from += n;
                if (_from == _pos) {
                    _from = _pos = 0;
                }
            }
            return n;
        }

        // Decodes into the empty output buffer; true when it needs more input
        bool _step() noexcept {
            const uint8_t* p = _input.data() + _in_begin;
            auto st = _decoder->decode(p, _input.data() + _in_end, _source_ended, _output.data(), _pos, _output.size());
            _in_begin = size_t(p - _input.data());
            switch (st) {
                case detail::LzwStatus::done:
                    _ended = true;
                    return false;
                case detail::LzwStatus::failed:
                    _error = error(_decoder->error, _decoder->offset(), string(_decoder->error_text));
                    return false;
                case detail::LzwStatus::need_room:
                    return false;
                case detail::LzwStatus::need_input:
                    return _pos == 0;
            }
            return false;
        }

        // The decoder takes every byte it is given before it asks for more
        SGCL_INLINE_HOT optional<io::error> _fill() {
            _in_begin = _in_end = 0;
            auto r = _in.read(_input.room(0, _input.size()));
            return _took(r);
        }

        async::task<optional<io::error>> _async_fill() noexcept {
            _in_begin = _in_end = 0;
            _input.to_managed(0);   // the read may run on the pool: into managed memory, which the slice holds
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
        std::unique_ptr<detail::LzwDecoder> _decoder;
        detail::InputBuffer<InputBytes> _input;   // plain until a task's first read, managed from then on (detail/block.h)
        std::vector<uint8_t> _output;
        size_t _pos = 0;          // the end of the decoded bytes in _output
        size_t _from = 0;         // the first not yet handed out
        size_t _in_begin = 0;     // the input not yet taken: [_in_begin, _in_end)
        size_t _in_end = 0;
        bool _source_ended = false;
        bool _ended = false;
        optional<error> _error;
        static constexpr size_t YieldEvery = size_t(64) << 10;
        size_t _since_yield = 0;   // bytes handed out by the task's reads since it last let the worker go
        order _order;
        int _literal_width;
    };
}
