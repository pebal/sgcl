//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/copy.h"
#include "detail/lzma_encoder.h"
#include "detail/stream.h"
#include "../core/array.h"
#include "../core/detail/bytes.h"
#include "../io/detail/bytes.h"

namespace sgcl::compress {
    // LZMA in its first file format, "LZMA alone" (.lzma, what `xz
    // --format=lzma` and `lzma` write): a header of 13 bytes — the
    // properties lc, lp and pb in one byte, the dictionary's size, the
    // size decompressed or all ones when it is not known — then the data
    // of the range coder, which ends with a marker when the size is not
    // known. The levels are xz's -0 to -9 (0 the fastest, 6 the default,
    // 9 the smallest) with its dictionaries of 256 KiB to 64 MiB; extreme
    // is its -e. compress() knows the size and writes it with no marker
    // (as the LZMA SDK does); a writer does not, and ends with the marker
    // (as xz does). Both are read by every decoder of the format.
    class lzma {
    public:
        using error = compress::error;

        struct options {
            compress::level level;
            bool extreme = false;
            uint32_t dictionary = 0;   // bytes, 4 KiB to 1.5 GiB; 0: the level's
            uint8_t lc = 3;            // literal context bits, 0..8
            uint8_t lp = 0;            // literal position bits, 0..4
            uint8_t pb = 2;            // position bits, 0..4
        };

        class writer;
        class reader;

        static constexpr size_t HeaderSize = 13;

        SGCL_INLINE_HOT static vector<byte> compress(const slice<const byte>& data) noexcept {
            return compress(data, options{});
        }

        // Options out of range are the program's mistake: std::invalid_argument
        // (the writer's first write reports it as errc::invalid_argument)
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            auto s = _settings(o);
            if (!s) {
                throw std::invalid_argument(std::string("compress::lzma: ") + s.error());
            }
            auto enc = std::make_unique<detail::LzmaEncoder>(*s);
            const uint8_t* p = detail::bytes(data);
            enc->attach(p, data.size());
            detail::LentOutput lent;   // the thread's room, kept from call to call
            std::vector<uint8_t>& out = lent.out();
            out.reserve(data.size() / 3 + 64);
            _header(out, s->props, enc->dictionary(), data.size());
            (void)enc->run(true, out);
            enc->finish(false, out);
            return detail::to_vector(out.data(), out.size());
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text) noexcept {
            return compress(io::detail::bytes_of(text), options{});
        }

        SGCL_INLINE_HOT static vector<byte> compress(const string& text, const options& o) {
            return compress(io::detail::bytes_of(text), o);
        }

        // A literal, a character array, a std::string_view: the text's
        // bytes as the string's overload takes them (an exact match, else
        // the two conversions, to a string and to bytes, tie)
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text) noexcept {
            return compress(slice<const byte>(text), options{});
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT static vector<byte> compress(const T& text, const options& o) {
            return compress(slice<const byte>(text), o);
        }

        SGCL_INLINE_HOT static expected<vector<byte>, error> decompress(const slice<const byte>& data) noexcept {
            return decompress(data, limits{});
        }

        // Up to the limit's size, with no more memory for the dictionary
        // than it allows; nothing after the data is read
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) noexcept {
            const uint8_t* p = detail::bytes(data);
            const size_t n = data.size();
            detail::LzmaProperties props;
            uint64_t size;
            if (auto e = _parse_header(p, n, l, props, size)) {
                return unexpected<error>(*e);
            }
            using Decoder = detail::LzmaDecoder;
            // the output is the dictionary: a match reaches back into it
            const uint64_t ceiling = size != UINT64_MAX ? size : l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
            // a size in the header is believed only as far as the output grows to it
            uint64_t guess = std::min<uint64_t>(std::max<uint64_t>(uint64_t(n) * 8, uint64_t(1) << 16), uint64_t(256) << 20);
            size_t capacity = size_t(std::min<uint64_t>(guess, ceiling));
            vector<byte> result;
            result.resize(capacity + Decoder::CopySlack);
            auto dec = std::make_unique<Decoder>();
            dec->reset(props, size);
            const uint8_t* in = p + HeaderSize;
            size_t pos = 0;
            for (;;) {
                auto out = reinterpret_cast<uint8_t*>(result.data());
                auto st = dec->decode<true>(out, capacity + Decoder::CopySlack, pos, capacity, in, p + n, true);
                if (st == detail::LzmaStatus::done) {
                    break;
                }
                if (st == detail::LzmaStatus::failed) {
                    return unexpected<error>(error(dec->error, HeaderSize + dec->taken(), dec->error_text ? string(dec->error_text) : string()));
                }
                // need_room: grown, up to the limit
                if (capacity >= ceiling || uint64_t(capacity) > l.max_size) {
                    return unexpected<error>(error(errc::too_large, HeaderSize + dec->taken(), "lzma: decompressed data past the limit"));
                }
                capacity = size_t(std::min<uint64_t>({uint64_t(capacity) * 2, ceiling, uint64_t(SIZE_MAX) - Decoder::CopySlack}));
                result.resize(capacity + Decoder::CopySlack);
            }
            if (pos > l.max_size) {
                return unexpected<error>(error(errc::too_large, HeaderSize + dec->taken(), "lzma: decompressed data past the limit"));
            }
            result.resize(pos);
            return result;
        }

    private:
        friend class writer;
        friend class reader;

        // The encoder's settings of the options, or what is wrong with them
        static expected<detail::LzmaEncoderSettings, const char*> _settings(const options& o) noexcept {
            int level = o.level.value();
            if (level < 0) {
                return unexpected<const char*>("a level of 0..9 (huffman_only is DEFLATE's)");
            }
            if (o.lc > 8 || o.lp > 4 || o.pb > 4) {
                return unexpected<const char*>("lc 0..8, lp 0..4, pb 0..4");
            }
            if (o.dictionary && (o.dictionary < detail::lzma_model::DictionaryMin || o.dictionary > MaxDictionary)) {
                return unexpected<const char*>("a dictionary of 4 KiB to 1.5 GiB");
            }
            auto s = detail::LzmaEncoderSettings::of(level, o.extreme);
            s.props.lc = o.lc;
            s.props.lp = o.lp;
            s.props.pb = o.pb;
            if (o.dictionary) {
                s.props.dictionary = o.dictionary;
            }
            return s;
        }

        static constexpr uint32_t MaxDictionary = uint32_t(3) << 29;

        SGCL_INLINE_HOT static void _header(std::vector<uint8_t>& out, const detail::LzmaProperties& props, uint32_t dictionary, uint64_t size) noexcept {
            out.push_back(props.to_byte());
            detail::put_le32(out, dictionary);
            detail::put_le32(out, uint32_t(size));
            detail::put_le32(out, uint32_t(size >> 32));
        }

        // What the decoder needs besides its output: the literal
        // probabilities, and the dictionary when it is a window of its own
        SGCL_INLINE_HOT static uint64_t _memory(const detail::LzmaProperties& props, uint64_t size) noexcept {
            uint64_t dictionary = std::max(props.dictionary, detail::lzma_model::DictionaryMin);
            return std::min(dictionary, size) + uint64_t(props.literal_probs()) * 2 + sizeof(detail::LzmaDecoder);
        }

        // The header of the data: its properties and its size (UINT64_MAX:
        // not known); the memory it asks for held against the limit
        static optional<error> _parse_header(const uint8_t* p, size_t n, const limits& l, detail::LzmaProperties& props, uint64_t& size) noexcept {
            if (n < HeaderSize) {
                return error(errc::unexpected_end, n, "lzma: unexpected end in the header");
            }
            if (!detail::LzmaProperties::from_byte(p[0], props)) {
                return error(errc::invalid_header, 0, "lzma: properties byte past 224");
            }
            props.dictionary = detail::le32(p + 1);
            size = uint64_t(detail::le32(p + 5)) | uint64_t(detail::le32(p + 9)) << 32;
            if (_memory(props, size) > l.max_memory) {
                return error(errc::too_large, 0, "lzma: the dictionary needs more memory than the limit allows");
            }
            if (size != UINT64_MAX && size > l.max_size) {
                return error(errc::too_large, 0, "lzma: decompressed data past the limit");
            }
            return nullopt;
        }
    };

    // What is written, compressed into out. The stream's size is not
    // known ahead: the header says so, and close() writes the end marker
    // and the coder's last bytes, leaving out open. The work waits for a
    // lookahead of about 4 KB beyond each position (the parser prices the
    // bytes ahead of it), so output comes some way behind the input, and
    // LZMA has no flush: nothing is decodable before close(). The first
    // failure — of out, or of options out of range — is kept: every write
    // and close after it gives it at once and writes nothing, so a stream
    // may be written freely and checked once, at the close.
    class lzma::writer final
    : public io::mixin::writer<lzma::writer> {
    public:
        using io::mixin::writer<lzma::writer>::write;
        using io::mixin::writer<lzma::writer>::async_write;

        SGCL_INLINE_HOT explicit writer(const io::writer& out) noexcept
        : writer(out, options{}) {
        }

        SGCL_INLINE_HOT writer(const io::writer& out, const options& o) noexcept
        : _out(out) {
            auto s = lzma::_settings(o);
            if (s) {
                _settings = *s;
                _encoder = std::make_unique<detail::LzmaEncoder>(_settings);
            } else {
                _invalid = true;
            }
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;

        // The other left closed, its stream and its encoder gone with the
        // move (its options kept): its writes give io::errc::closed, its
        // close does nothing, and a reset gives it a new stream
        SGCL_INLINE_HOT writer(writer&& o) noexcept
        : _out(std::move(o._out))
        , _settings(o._settings)
        , _encoder(std::move(o._encoder))
        , _pending(std::move(o._pending))
        , _block(std::move(o._block))
        , _error(std::move(o._error))
        , _started(o._started)
        , _closed(o._closed)
        , _invalid(o._invalid) {
            o._out = io::writer();
            o._pending = std::vector<uint8_t>();
            o._error = nullopt;
            o._started = true;
            o._closed = true;
        }

        SGCL_INLINE_HOT writer& operator=(writer&& o) noexcept {
            return detail::move_into(*this, std::move(o));
        }

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto e = _check("write")) {
                return io::detail::fail(*e);
            }
            const uint8_t* p = detail::bytes(data);
            size_t n = data.size();
            for (;;) {
                size_t k = _encoder->append(p, n);
                p += k;
                n -= k;
                (void)_encoder->run(false, _pending);
                if (auto e = _drain()) {
                    return io::detail::fail(*e);
                }
                if (!n) {
                    return data.size();
                }
            }
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
            if (auto e = _check("write")) {
                co_return io::detail::fail(*e);
            }
            const uint8_t* p = detail::bytes(data);
            size_t n = data.size();
            for (;;) {
                size_t k = _encoder->append(p, n);
                p += k;
                n -= k;
                // the work in portions, the worker let go between them
                while (_encoder->run(false, _pending, Portion) != detail::LzmaRun::done) {
                    if (auto e = co_await _async_drain()) {
                        co_return io::detail::fail(*e);
                    }
                    co_await async::yield();
                }
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                if (!n) {
                    co_return data.size();
                }
            }
        }

        // The rest compressed, the end marker, the coder's last bytes; out
        // stays open. A second close does nothing.
        expected<void, io::error> close() {
            if (_closed && !_error) {
                return {};
            }
            if (auto e = _check("close")) {
                return io::detail::fail(*e);
            }
            _closed = true;
            (void)_encoder->run(true, _pending);
            _encoder->finish(true, _pending);
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
            while (_encoder->run(true, _pending, Portion) != detail::LzmaRun::done) {
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                co_await async::yield();
            }
            _encoder->finish(true, _pending);
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _closed;
        }

        // The first failure, kept
        SGCL_INLINE_HOT const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same settings: the encoder's
        // memory (the window, the tables) kept
        void reset(const io::writer& out) noexcept {
            _out = out;
            if (_encoder) {
                _encoder->restart();
            } else if (!_invalid) {
                _encoder = std::make_unique<detail::LzmaEncoder>(_settings);   // moved from: its encoder went with the move
            }
            _pending.clear();
            _started = false;
            _closed = false;
            _error = nullopt;
        }

    private:
        static constexpr uint64_t Portion = uint64_t(64) << 10;

        optional<io::error> _check(const char* op) noexcept {
            if (_error) {
                return _error;
            }
            if (_closed) {
                _error = io::error(io::errc::closed, op, "lzma");   // kept as every error
                return _error;
            }
            if (!_encoder) {
                _error = io::error(make_error_code(errc::invalid_argument), op, "lzma");
                return _error;
            }
            if (!_started) {
                _started = true;
                lzma::_header(_pending, _settings.props, _encoder->dictionary(), UINT64_MAX);
            }
            return nullopt;
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

        // A task's write is given a managed block, which the slice holds:
        // the write may outlive this frame on a pool
        async::task<optional<io::error>> _async_drain() noexcept {
            size_t at = 0;
            while (at < _pending.size()) {
                if (!_block) {
                    _block = make_tracked<io::detail::CopyBlock>();
                }
                size_t k = std::min(_pending.size() - at, _block->size());
                sgcl::detail::copy_bytes(_block->data(), _pending.data() + at, k);
                auto w = co_await _out.async_write(slice<const byte>(_block, _block->data(), k));
                if (!w) {
                    _pending.clear();
                    _error = w.error();
                    co_return _error;
                }
                at += k;
            }
            _pending.clear();
            co_return nullopt;
        }

        io::writer _out;
        detail::LzmaEncoderSettings _settings;
        std::unique_ptr<detail::LzmaEncoder> _encoder;
        std::vector<uint8_t> _pending;
        tracked_ptr<io::detail::CopyBlock> _block;
        optional<io::error> _error;
        bool _started = false;
        bool _closed = false;
        bool _invalid = false;   // options out of range: no encoder, ever
    };

    // What the data read from in decompresses to. The dictionary is a
    // window of its own, as large as the header asks (or as the data, when
    // the header gives its size), and is held against limits.max_memory
    // before it is taken: more is errc::too_large at the first read. The
    // reader reads its input 64 KB at a time, so it may read past the end
    // of the LZMA data. A failure is the error of the read that reaches it
    // and of every read after, last_error() the whole of it.
    class lzma::reader final
    : public io::mixin::reader<lzma::reader> {
    public:
        SGCL_INLINE_HOT explicit reader(const io::reader& in) noexcept
        : reader(in, limits{}) {
        }

        SGCL_INLINE_HOT reader(const io::reader& in, const limits& l) noexcept
        : _in(in)
        , _limits(l)
        , _decoder(std::make_unique<detail::LzmaDecoder>())
        , _input(new uint8_t[InputBytes]) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;

        // The other left without a stream, its decoder and its buffers gone
        // with the move (its limits kept): its reads give io::errc::closed,
        // its close closes nothing, and a reset gives it a new stream
        reader(reader&& o) noexcept
        : _in(std::move(o._in))
        , _limits(o._limits)
        , _decoder(std::move(o._decoder))
        , _input(std::move(o._input))
        , _block(std::move(o._block))
        , _window(std::move(o._window))
        , _window_size(o._window_size)
        , _pos(o._pos)
        , _from(o._from)
        , _in_begin(o._in_begin)
        , _in_end(o._in_end)
        , _consumed(o._consumed)
        , _source_ended(o._source_ended)
        , _ended(o._ended)
        , _started(o._started)
        , _error(std::move(o._error))
        , _since_yield(o._since_yield) {
            o._in = io::reader();
            o._block = nullptr;
            o._window_size = 0;
            o._pos = o._from = o._in_begin = o._in_end = 0;
            o._ended = true;
            o._error = detail::moved_from_error("lzma");
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
                    return io::detail::fail(detail::to_io_error(*_error, "lzma"));
                }
                if (_ended || out.empty()) {
                    return 0;
                }
                if (_advance()) {
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
                    // the task lets the worker go
                    _since_yield += n;
                    if (_since_yield >= YieldEvery) {
                        _since_yield = 0;
                        co_await async::yield();
                    }
                    co_return n;
                }
                if (_error) {
                    co_return io::detail::fail(detail::to_io_error(*_error, "lzma"));
                }
                if (_ended || out.empty()) {
                    co_return 0;
                }
                if (_advance()) {
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

        // A new stream from in: the dictionary kept when it is large enough
        void reset(const io::reader& in) noexcept {
            _in = in;
            if (!_decoder) {
                // moved from: the decoder and the input went with the move
                _decoder = std::make_unique<detail::LzmaDecoder>();
                _input.reset(new uint8_t[InputBytes]);
            }
            _in_begin = _in_end = 0;
            _consumed = 0;
            _pos = _from = 0;
            _source_ended = false;
            _ended = false;
            _started = false;
            _error = nullopt;
            _since_yield = 0;
        }

    private:
        static constexpr size_t InputBytes = size_t(64) << 10;
        static constexpr size_t Chunk = size_t(256) << 10;   // decoded before a hand-out, at most
        static constexpr size_t YieldEvery = size_t(64) << 10;

        SGCL_INLINE_HOT size_t _hand_out(const slice<byte>& out) noexcept {
            size_t n = std::min(out.size(), _pos - _from);
            if (n) {
                detail::copy_out(out.data(), _window.get() + _from, n);
                _from += n;
            }
            return n;
        }

        void _fail(errc code, uint64_t offset, const char* text) noexcept {
            _error = error(code, offset, text ? string(text) : string());
        }

        // Works on the input held; true when it needs more of it
        bool _advance() noexcept {
            if (!_started) {
                size_t have = _in_end - _in_begin;
                if (have < HeaderSize && !_source_ended) {
                    return true;
                }
                detail::LzmaProperties props;
                uint64_t size;
                if (auto e = lzma::_parse_header(_input.get() + _in_begin, have, _limits, props, size)) {
                    _error = *e;
                    return false;
                }
                _in_begin += HeaderSize;
                uint64_t dictionary = std::max(props.dictionary, detail::lzma_model::DictionaryMin);
                size_t want = size_t(std::max<uint64_t>(std::min(dictionary, size), 1));
                if (_window_size < want || _window_size > want * 2) {
                    _window.reset(new uint8_t[want]);
                }
                _window_size = want;
                _decoder->reset(props, size);
                _started = true;
            }
            if (_pos != _from) {
                return false;   // hand out first
            }
            if (_pos == _window_size) {
                _pos = _from = 0;
            }
            const uint8_t* in = _input.get() + _in_begin;
            size_t limit = std::min(_window_size, _pos + Chunk);
            auto st = _decoder->decode<false>(_window.get(), _window_size, _pos, limit, in, _input.get() + _in_end, _source_ended);
            _in_begin = size_t(in - _input.get());
            switch (st) {
                case detail::LzmaStatus::done:
                    _ended = true;
                    return false;
                case detail::LzmaStatus::failed:
                    _fail(_decoder->error, HeaderSize + _decoder->taken(), _decoder->error_text);
                    return false;
                case detail::LzmaStatus::need_room:
                    return false;
                case detail::LzmaStatus::need_input:
                    break;
            }
            return _pos == _from;
        }

        // room for more input: what is left moved to the front
        SGCL_INLINE_HOT size_t _make_room() noexcept {
            if (_in_begin) {
                sgcl::detail::move_bytes(_input.get(), _input.get() + _in_begin, _in_end - _in_begin);
                _consumed += _in_begin;
                _in_end -= _in_begin;
                _in_begin = 0;
            }
            return InputBytes - _in_end;
        }

        SGCL_INLINE_HOT optional<io::error> _fill() {
            size_t room = _make_room();
            auto r = _in.read(slice<byte>(reinterpret_cast<byte*>(_input.get() + _in_end), room));
            return _took(r, r ? *r : 0);
        }

        // A task's read is given a managed block, which the slice holds:
        // the read may outlive this frame on a pool
        async::task<optional<io::error>> _async_fill() noexcept {
            size_t room = std::min(_make_room(), config::io_copy_buffer_size);
            if (!_block) {
                _block = make_tracked<io::detail::CopyBlock>();
            }
            auto r = co_await _in.async_read(slice<byte>(_block, _block->data(), room));
            if (r && *r) {
                sgcl::detail::copy_bytes(_input.get() + _in_end, _block->data(), *r);
            }
            co_return _took(r, r ? *r : 0);
        }

        SGCL_INLINE_HOT optional<io::error> _took(const expected<size_t, io::error>& r, size_t n) noexcept {
            if (!r) {
                _error = error(r.error(), _started ? HeaderSize + _decoder->taken() : _consumed + _in_end);
                return r.error();
            }
            if (n == 0) {
                _source_ended = true;
            }
            _in_end += n;
            return nullopt;
        }

        io::reader _in;
        limits _limits;
        std::unique_ptr<detail::LzmaDecoder> _decoder;
        std::unique_ptr<uint8_t[]> _input;
        tracked_ptr<io::detail::CopyBlock> _block;
        std::unique_ptr<uint8_t[]> _window;   // the dictionary, round which pos goes
        size_t _window_size = 0;
        size_t _pos = 0;          // the end of the decoded bytes in the window
        size_t _from = 0;         // the first not yet handed out
        size_t _in_begin = 0;     // the input not yet decoded: [_in_begin, _in_end)
        size_t _in_end = 0;
        uint64_t _consumed = 0;
        bool _source_ended = false;
        bool _ended = false;
        bool _started = false;    // the header read
        optional<error> _error;
        size_t _since_yield = 0;
    };
}
