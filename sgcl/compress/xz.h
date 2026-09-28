//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/xz_format.h"
#include "../core/array.h"
#include "../io/detail/bytes.h"

namespace sgcl::compress {
    // xz (.xz, .txz, .tar.xz): LZMA2 in the container of xz-file-format
    // 1.2 — blocks, an index of their sizes checked at the end, a check of
    // the data (CRC-32, CRC-64, SHA-256 or none), and before LZMA2 up to
    // three filters: a branch converter for machine code (x86, ARM,
    // ARM-Thumb, ARM64, PowerPC, SPARC, IA-64, RISC-V) and Delta. Streams
    // one after another, with zero bytes between them in fours, are read
    // as one, as xz reads them. The levels are xz's -0 to -9 and -e, as in
    // compress::lzma.
    class xz {
    public:
        using error = compress::error;

        enum class check : uint8_t {
            none = 0,
            crc32 = 1,
            crc64 = 4,
            sha256 = 10
        };

        // The branch converters: the processor the data's code is for
        enum class filter : uint8_t {
            x86,
            arm,
            armt,
            arm64,
            powerpc,
            sparc,
            ia64,
            riscv
        };

        struct options {
            compress::level level;
            bool extreme = false;
            xz::check check = xz::check::crc64;   // xz's default
            optional<xz::filter> bcj;             // a branch converter before LZMA2
            uint16_t delta = 0;                   // Delta's distance, 1..256, before LZMA2 (after the converter); 0: none
            uint32_t dictionary = 0;              // bytes, 4 KiB to 1.5 GiB; 0: the level's
        };

        class writer;
        class reader;

        static vector<byte> compress(const slice<const byte>& data) {
            return compress(data, options{});
        }

        // Options out of range are the program's mistake: std::invalid_argument
        // (the writer's first write reports it as errc::invalid_argument).
        // One block, its sizes in its header.
        static vector<byte> compress(const slice<const byte>& data, const options& o) {
            auto s = _setup(o);
            if (!s) {
                throw std::invalid_argument(std::string("compress::xz: ") + s.error());
            }
            const uint8_t* p = detail::bytes(data);
            const size_t n = data.size();
            std::vector<uint8_t> out;
            out.reserve(n / 3 + 128);
            detail::xz_format::stream_header(out, s->check);
            std::vector<detail::xz_format::Record> records;
            if (n) {
                std::vector<uint8_t> plain;
                const uint8_t* coded = p;
                if (!s->chain.empty()) {
                    plain.assign(p, p + n);
                    s->chain.apply(plain.data(), n);
                    coded = plain.data();
                }
                auto enc = std::make_unique<detail::Lzma2Encoder>(s->lzma);
                enc->attach(coded, n);
                std::vector<uint8_t> body;
                body.reserve(n / 3 + 64);
                (void)enc->run(true, body);
                enc->finish(body);
                s->filters[s->count - 1].value = enc->dictionary();
                size_t before = out.size();
                detail::xz_format::block_header(out, s->filters, s->count, body.size(), n);
                size_t header = out.size() - before;
                out.insert(out.end(), body.begin(), body.end());
                while ((out.size() - before) % 4) {
                    out.push_back(0);
                }
                detail::xz_format::Check c;
                c.reset(s->check);
                c.update(p, n);
                uint8_t sum[64];
                c.value(sum);
                size_t cs = detail::xz_format::check_size(s->check);
                out.insert(out.end(), sum, sum + cs);
                records.push_back({header + body.size() + cs, n});
            }
            detail::xz_format::index_and_footer(out, records, s->check);
            return detail::to_vector(out.data(), out.size());
        }

        static vector<byte> compress(const string& text) {
            return compress(io::detail::bytes_of(text), options{});
        }

        static vector<byte> compress(const string& text, const options& o) {
            return compress(io::detail::bytes_of(text), o);
        }

        static expected<vector<byte>, error> decompress(const slice<const byte>& data) {
            return decompress(data, limits{});
        }

        // Every stream, one after another, up to the limit's size; every
        // block's check, the index and the footers checked
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l) {
            namespace f = detail::xz_format;
            using detail::Parsed;
            const uint8_t* p = detail::bytes(data);
            const size_t n = data.size();
            const size_t slack = detail::LzmaDecoder::CopySlack;
            const uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
            uint64_t guess = std::min<uint64_t>(std::max<uint64_t>(uint64_t(n) * 8, uint64_t(1) << 16), uint64_t(256) << 20);
            size_t capacity = size_t(std::min<uint64_t>(guess, ceiling));
            vector<byte> result;
            result.resize(capacity + slack);
            auto out = [&]() noexcept {
                return reinterpret_cast<uint8_t*>(result.data());
            };
            auto fail = [](errc code, uint64_t at, const char* text) {
                return unexpected<error>(error(code, at, text ? string(text) : string()));
            };
            size_t total = 0;
            size_t at = 0;
            std::unique_ptr<detail::Lzma2Decoder> dec;
            detail::FilterChain chain;
            std::vector<f::Record> records;
            for (bool first = true;; first = false) {
                if (!first && at == n) {
                    break;
                }
                uint8_t check;
                auto sh = f::parse_stream_header(p + at, n - at, check);
                if (sh.status != Parsed::ok) {
                    return fail(sh.status == Parsed::more ? errc::unexpected_end : sh.code, at, sh.text);
                }
                at += sh.size;
                records.clear();
                const size_t cs = f::check_size(check);
                for (;;) {
                    if (at >= n) {
                        return fail(errc::unexpected_end, n, "xz: unexpected end before the index");
                    }
                    if (p[at] == 0) {
                        break;
                    }
                    f::BlockHeader h;
                    auto bh = f::parse_block_header(p + at, n - at, h);
                    if (bh.status != Parsed::ok) {
                        return fail(bh.status == Parsed::more ? errc::unexpected_end : bh.code, at, bh.text);
                    }
                    const size_t block_at = at;
                    at += h.size;
                    uint32_t dictionary = h.filters[h.count - 1].value;
                    if (_memory(dictionary, h.uncompressed) > l.max_memory) {
                        return fail(errc::too_large, block_at, "xz: the dictionary needs more memory than the limit allows");
                    }
                    if (h.uncompressed != UINT64_MAX && total + h.uncompressed > l.max_size) {
                        return fail(errc::too_large, block_at, "xz: decompressed data past the limit");
                    }
                    if (!dec) {
                        dec = std::make_unique<detail::Lzma2Decoder>();
                    }
                    dec->reset(dictionary);
                    const uint8_t* in = p + at;
                    const uint8_t* end = p + n;
                    if (h.compressed != UINT64_MAX && h.compressed < uint64_t(n - at)) {
                        end = in + h.compressed;
                    }
                    size_t pos = 0;
                    for (;;) {
                        auto st = dec->decode<true>(out() + total, capacity + slack - total, pos, capacity - total, in, end, true);
                        if (st == detail::LzmaStatus::done) {
                            break;
                        }
                        if (st == detail::LzmaStatus::failed) {
                            return fail(dec->error, at + dec->taken(), dec->error_text);
                        }
                        if (capacity >= ceiling || uint64_t(capacity) > l.max_size) {
                            return fail(errc::too_large, at + dec->taken(), "xz: decompressed data past the limit");
                        }
                        capacity = size_t(std::min<uint64_t>({uint64_t(capacity) * 2, ceiling, uint64_t(SIZE_MAX) - slack}));
                        result.resize(capacity + slack);
                    }
                    uint64_t compressed = dec->taken();
                    if ((h.compressed != UINT64_MAX && h.compressed != compressed) || (h.uncompressed != UINT64_MAX && h.uncompressed != pos)) {
                        return fail(errc::corrupt, block_at, "xz: a block's sizes differ from its header's");
                    }
                    if (total + pos > l.max_size) {
                        return fail(errc::too_large, at + compressed, "xz: decompressed data past the limit");
                    }
                    f::decoding_chain(h, chain);
                    chain.apply(out() + total, pos);
                    at += size_t(compressed);
                    for (size_t pad = size_t((4 - (h.size + compressed) % 4) % 4); pad; --pad, ++at) {
                        if (at >= n) {
                            return fail(errc::unexpected_end, n, "xz: unexpected end in a block's padding");
                        }
                        if (p[at]) {
                            return fail(errc::corrupt, at, "xz: block padding not zero");
                        }
                    }
                    if (n - at < cs) {
                        return fail(errc::unexpected_end, n, "xz: unexpected end in a block's check");
                    }
                    if (cs) {
                        f::Check c;
                        c.reset(check);
                        c.update(out() + total, pos);
                        uint8_t sum[64];
                        c.value(sum);
                        if (std::memcmp(sum, p + at, cs) != 0) {
                            return fail(errc::checksum, at, "xz: the block's check does not match its data");
                        }
                    }
                    at += cs;
                    records.push_back({h.size + compressed + cs, pos});
                    total += pos;
                }
                // the index, then the footer
                f::IndexReader index;
                index.start();
                size_t used;
                auto ix = index.step(p + at + 1, n - at - 1, records, used);
                if (ix.status != Parsed::ok) {
                    return fail(ix.status == Parsed::more ? errc::unexpected_end : ix.code, at, ix.text);
                }
                at += 1 + used;
                auto ft = f::parse_footer(p + at, n - at, index.size(), check);
                if (ft.status != Parsed::ok) {
                    return fail(ft.status == Parsed::more ? errc::unexpected_end : ft.code, at, ft.text);
                }
                at += ft.size;
                // stream padding: zeros in fours
                size_t zeros = 0;
                while (at < n && p[at] == 0) {
                    ++at;
                    ++zeros;
                }
                if (zeros % 4) {
                    return fail(errc::corrupt, at, "xz: stream padding not a multiple of four bytes");
                }
            }
            result.resize(total);
            return result;
        }

    private:
        friend class writer;
        friend class reader;

        static constexpr uint32_t MaxDictionary = uint32_t(3) << 29;

        struct Setup {
            detail::LzmaEncoderSettings lzma;
            detail::xz_format::Filter filters[4];
            size_t count = 0;
            detail::FilterChain chain;   // the encoding stages
            uint8_t check = 4;
        };

        static uint64_t _filter_id(xz::filter k) noexcept {
            switch (k) {
                case filter::x86: return 0x04;
                case filter::powerpc: return 0x05;
                case filter::ia64: return 0x06;
                case filter::arm: return 0x07;
                case filter::armt: return 0x08;
                case filter::sparc: return 0x09;
                case filter::arm64: return 0x0A;
                case filter::riscv: return 0x0B;
            }
            return 0;
        }

        static expected<Setup, const char*> _setup(const options& o) {
            int level = o.level.value();
            if (level < 0) {
                return unexpected<const char*>("a level of 0..9 (huffman_only is DEFLATE's)");
            }
            if (o.dictionary && (o.dictionary < detail::lzma_model::DictionaryMin || o.dictionary > MaxDictionary)) {
                return unexpected<const char*>("a dictionary of 4 KiB to 1.5 GiB");
            }
            uint8_t c = uint8_t(o.check);
            if (!detail::xz_format::check_supported(c)) {
                return unexpected<const char*>("check none, crc32, crc64 or sha256");
            }
            if (o.delta > 256) {
                return unexpected<const char*>("a Delta distance of 1..256");
            }
            if (o.bcj && _filter_id(*o.bcj) == 0) {
                return unexpected<const char*>("an unknown branch converter");
            }
            Setup s;
            s.lzma = detail::LzmaEncoderSettings::of(level, o.extreme);
            if (o.dictionary) {
                s.lzma.props.dictionary = o.dictionary;
            }
            s.check = c;
            if (o.bcj) {
                uint64_t id = _filter_id(*o.bcj);
                s.filters[s.count++] = {id, 0};
                detail::SimpleKind kind;
                detail::xz_format::simple_kind(id, kind);
                detail::SimpleFilter f;
                f.init(kind, true);
                s.chain.add(f);
            }
            if (o.delta) {
                s.filters[s.count++] = {detail::xz_format::FilterDelta, o.delta};
                detail::SimpleFilter f;
                f.init(detail::SimpleKind::delta, true, 0, o.delta);
                s.chain.add(f);
            }
            s.filters[s.count++] = {detail::xz_format::FilterLzma2, s.lzma.props.dictionary};
            return s;
        }

        // What a decoder takes because the header asks: the dictionary
        // (as large as the block, when its header gives its size), the
        // probabilities, the filters' buffer
        static uint64_t _memory(uint32_t dictionary, uint64_t uncompressed) noexcept {
            uint64_t d = std::max(dictionary, detail::lzma_model::DictionaryMin);
            return std::min(d, uncompressed) + (uint64_t(0x300) << 4) * 2 + sizeof(detail::Lzma2Decoder) + detail::FilterChain::Capacity;
        }
    };

    // What is written, compressed into out as one stream of one block
    // (xz's single-threaded form, the sizes not in the block's header).
    // The work waits for a lookahead of about 4 KB beyond each position;
    // xz has no flush: nothing is decodable before close(), which writes
    // the rest, the block's check, the index and the footer, and leaves
    // out open. The first failure — of out, or of options out of range —
    // is kept: every write and close after it gives it at once and writes
    // nothing, so a stream may be written freely and checked once, at the
    // close.
    class xz::writer final
    : public io::mixin::writer<xz::writer> {
    public:
        using io::mixin::writer<xz::writer>::write;
        using io::mixin::writer<xz::writer>::async_write;

        explicit writer(const io::writer& out)
        : writer(out, options{}) {
        }

        writer(const io::writer& out, const options& o)
        : _out(out) {
            auto s = xz::_setup(o);
            if (s) {
                _setup = std::make_unique<Setup>(std::move(*s));
                _encoder = std::make_unique<detail::Lzma2Encoder>(_setup->lzma);
                _start();
            }
        }

        writer(const writer&) = delete;
        writer& operator=(const writer&) = delete;
        writer(writer&&) noexcept = default;
        writer& operator=(writer&&) noexcept = default;

        expected<size_t, io::error> write(const slice<const byte>& data) {
            if (auto e = _check_state("write")) {
                return io::detail::fail(*e);
            }
            _take(detail::bytes(data), data.size(), 0);
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return data.size();
        }

        async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
            if (auto e = _check_state("write")) {
                co_return io::detail::fail(*e);
            }
            const uint8_t* p = detail::bytes(data);
            size_t n = data.size();
            // the work in portions, the worker let go between them
            while (n) {
                size_t k = std::min<size_t>(n, Portion);
                _take(p, k, Portion);
                p += k;
                n -= k;
                while (_run(false, Portion) != detail::LzmaRun::done) {
                    if (auto e = co_await _async_drain()) {
                        co_return io::detail::fail(*e);
                    }
                    co_await async::yield();
                }
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                if (n) {
                    co_await async::yield();
                }
            }
            co_return data.size();
        }

        // The rest compressed, the block's check, the index and the
        // footer; out stays open. A second close does nothing.
        expected<void, io::error> close() {
            if (_closed && !_error) {
                return {};
            }
            if (auto e = _check_state("close")) {
                return io::detail::fail(*e);
            }
            _closed = true;
            _flush_filters();
            (void)_run(true, 0);
            _finish();
            if (auto e = _drain()) {
                return io::detail::fail(*e);
            }
            return {};
        }

        async::task<expected<void, io::error>> async_close() {
            if (_closed && !_error) {
                co_return expected<void, io::error>();
            }
            if (auto e = _check_state("close")) {
                co_return io::detail::fail(*e);
            }
            _closed = true;
            _flush_filters();
            while (_run(true, Portion) != detail::LzmaRun::done) {
                if (auto e = co_await _async_drain()) {
                    co_return io::detail::fail(*e);
                }
                co_await async::yield();
            }
            _finish();
            if (auto e = co_await _async_drain()) {
                co_return io::detail::fail(*e);
            }
            co_return expected<void, io::error>();
        }

        bool is_closed() const noexcept {
            return _closed;
        }

        // The first failure, kept
        const optional<io::error>& last_error() const noexcept {
            return _error;
        }

        // A new stream into out with the same settings: the encoder's
        // memory (the window, the tables) kept
        void reset(const io::writer& out) {
            _out = out;
            if (_encoder) {
                _encoder->restart();
                _start();
            }
            _pending.clear();
            _closed = false;
            _error = nullopt;
        }

    private:
        static constexpr uint64_t Portion = uint64_t(64) << 10;

        void _start() {
            _chain = _setup->chain;
            _check.reset(_setup->check);
            _started = false;
            _block_started = false;
            _uncompressed = 0;
            _compressed = 0;
        }

        optional<io::error> _check_state(const char* op) {
            if (_error) {
                return _error;
            }
            if (_closed) {
                return io::error(io::errc::closed, op, "xz");
            }
            if (!_encoder) {
                _error = io::error(make_error_code(errc::invalid_argument), op, "xz");
                return _error;
            }
            if (!_started) {
                _started = true;
                detail::xz_format::stream_header(_pending, _setup->check);
            }
            return nullopt;
        }

        void _begin_block() {
            if (!_block_started) {
                _block_started = true;
                size_t before = _pending.size();
                detail::xz_format::block_header(_pending, _setup->filters, _setup->count, UINT64_MAX, UINT64_MAX);
                _header_size = _pending.size() - before;
            }
        }

        // Bytes of the data: through the filters into the encoder, the
        // encoder run as they come (without a bound when budget is 0)
        void _take(const uint8_t* p, size_t n, uint64_t budget) {
            if (!n) {
                return;
            }
            _begin_block();
            _check.update(p, n);
            _uncompressed += n;
            if (_chain.empty()) {
                _feed(p, n, budget);
                return;
            }
            while (n) {
                size_t k = std::min(n, _chain.room());
                _chain.push(p, k);
                p += k;
                n -= k;
                _chain.run(false);
                size_t r = _chain.ready_size();
                _feed(_chain.ready(), r, budget);
                _chain.take(r);
            }
        }

        void _feed(const uint8_t* p, size_t n, uint64_t budget) {
            for (;;) {
                size_t k = _encoder->append(p, n);
                p += k;
                n -= k;
                if (!n) {
                    if (!budget) {
                        (void)_run(false, 0);
                    }
                    return;
                }
                (void)_run(false, 0);
            }
        }

        void _flush_filters() {
            if (_chain.empty() || !_block_started) {
                return;
            }
            _chain.run(true);
            size_t r = _chain.ready_size();
            _feed(_chain.ready(), r, 1);
            _chain.take(r);
        }

        detail::LzmaRun _run(bool finish, uint64_t budget) {
            if (!_block_started) {
                return detail::LzmaRun::done;
            }
            size_t before = _pending.size();
            auto st = _encoder->run(finish, _pending, budget);
            _compressed += _pending.size() - before;
            return st;
        }

        // The block's end, the index and the footer
        void _finish() {
            std::vector<detail::xz_format::Record> records;
            if (_block_started) {
                size_t before = _pending.size();
                _encoder->finish(_pending);
                _compressed += _pending.size() - before;
                _pending.insert(_pending.end(), (4 - (_header_size + _compressed) % 4) % 4, uint8_t(0));
                size_t cs = detail::xz_format::check_size(_setup->check);
                uint8_t sum[64];
                _check.value(sum);
                _pending.insert(_pending.end(), sum, sum + cs);
                records.push_back({_header_size + _compressed + cs, _uncompressed});
            }
            detail::xz_format::index_and_footer(_pending, records, _setup->check);
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
        async::task<optional<io::error>> _async_drain() {
            size_t at = 0;
            while (at < _pending.size()) {
                if (!_block) {
                    _block = make_tracked<io::detail::CopyBlock>();
                }
                size_t k = std::min(_pending.size() - at, _block->size());
                std::memcpy(_block->data(), _pending.data() + at, k);
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
        std::unique_ptr<Setup> _setup;
        std::unique_ptr<detail::Lzma2Encoder> _encoder;
        detail::FilterChain _chain;
        detail::xz_format::Check _check;
        std::vector<uint8_t> _pending;
        tracked_ptr<io::detail::CopyBlock> _block;
        optional<io::error> _error;
        uint64_t _uncompressed = 0;
        uint64_t _compressed = 0;
        size_t _header_size = 0;
        bool _started = false;
        bool _block_started = false;
        bool _closed = false;
    };

    // What the data read from in decompresses to, every stream of it. A
    // block's dictionary is a window of its own, as large as the block's
    // header asks (or as the block, when the header gives its size), held
    // against limits.max_memory before it is taken. The block's check is
    // compared at its end, the index at the stream's; a failure is the
    // error of the read that reaches it and of every read after,
    // last_error() the whole of it. The reader reads its input 64 KB at a
    // time, so it may read past the end of the xz data.
    class xz::reader final
    : public io::mixin::reader<xz::reader> {
    public:
        explicit reader(const io::reader& in)
        : reader(in, limits{}) {
        }

        reader(const io::reader& in, const limits& l)
        : _in(in)
        , _limits(l)
        , _decoder(std::make_unique<detail::Lzma2Decoder>())
        , _input(new uint8_t[InputBytes]) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;
        reader(reader&&) noexcept = default;
        reader& operator=(reader&&) noexcept = default;

        expected<size_t, io::error> read(const slice<byte>& out) {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    return n;
                }
                if (_error) {
                    return io::detail::fail(detail::to_io_error(*_error, "xz"));
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

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            for (;;) {
                if (size_t n = _hand_out(out)) {
                    _since_yield += n;
                    if (_since_yield >= YieldEvery) {
                        _since_yield = 0;
                        co_await async::yield();
                    }
                    co_return n;
                }
                if (_error) {
                    co_return io::detail::fail(detail::to_io_error(*_error, "xz"));
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
        expected<void, io::error> close() {
            return _in.close();
        }

        async::task<expected<void, io::error>> async_close() {
            return _in.async_close();
        }

        const optional<error>& last_error() const noexcept {
            return _error;
        }

        // A new stream from in: the memory kept
        void reset(const io::reader& in) {
            _in = in;
            _in_begin = _in_end = 0;
            _consumed = 0;
            _pos = _from = 0;
            _source_ended = false;
            _ended = false;
            _phase = Phase::stream_header;
            _streams = 0;
            _error = nullopt;
            _since_yield = 0;
        }

    private:
        static constexpr size_t InputBytes = size_t(64) << 10;
        static constexpr size_t Chunk = size_t(256) << 10;
        static constexpr size_t YieldEvery = size_t(64) << 10;

        enum class Phase : uint8_t {
            stream_header,
            block_start,
            block_data,
            block_end,
            index,
            footer,
            stream_padding
        };

        uint64_t _offset() const noexcept {
            return _consumed + _in_begin;
        }

        void _fail(errc code, uint64_t at, const char* text) {
            _error = error(code, at, text ? string(text) : string());
        }

        size_t _hand_out(const slice<byte>& out) noexcept {
            if (_phase != Phase::block_data) {
                return 0;
            }
            const uint8_t* src;
            size_t n;
            if (_chain.empty()) {
                src = _window.get() + _from;
                n = std::min(out.size(), _pos - _from);
                _from += n;
            } else {
                src = _chain.ready();
                n = std::min(out.size(), _chain.ready_size());
                _chain.take(n);
            }
            if (n) {
                std::memcpy(out.data(), src, n);
                _check.update(src, n);
            }
            return n;
        }

        bool _pending_out() const noexcept {
            return _chain.empty() ? _pos != _from : _chain.ready_size() != 0;
        }

        // What there is of the input, and its end
        const uint8_t* _at() const noexcept {
            return _input.get() + _in_begin;
        }

        size_t _have() const noexcept {
            return _in_end - _in_begin;
        }

        // A header's Parsed: true to go on, false with the error set or
        // with more input needed (need = true)
        bool _parsed(const detail::Parsed& r, bool& need) {
            need = false;
            if (r.status == detail::Parsed::failed) {
                _fail(r.code, _offset(), r.text);
                return false;
            }
            if (r.status == detail::Parsed::more) {
                if (_source_ended) {
                    _fail(errc::unexpected_end, _offset() + _have(), "xz: unexpected end of the data");
                } else {
                    need = true;
                }
                return false;
            }
            _in_begin += r.size;
            return true;
        }

        // Works on the input held; true when it needs more of it
        bool _advance() {
            namespace f = detail::xz_format;
            for (;;) {
                bool need = false;
                switch (_phase) {
                    case Phase::stream_header: {
                        if (_streams > 0 && _have() == 0 && _source_ended) {
                            _ended = true;
                            return false;
                        }
                        if (!_parsed(f::parse_stream_header(_at(), _have(), _check_type), need)) {
                            return need;
                        }
                        ++_streams;
                        _records.clear();
                        _phase = Phase::block_start;
                        break;
                    }
                    case Phase::block_start: {
                        if (_have() == 0) {
                            if (_source_ended) {
                                _fail(errc::unexpected_end, _offset(), "xz: unexpected end before the index");
                                return false;
                            }
                            return true;
                        }
                        if (_at()[0] == 0) {
                            ++_in_begin;
                            _index.start();
                            _phase = Phase::index;
                            break;
                        }
                        uint64_t block_at = _offset();
                        if (!_parsed(f::parse_block_header(_at(), _have(), _header), need)) {
                            return need;
                        }
                        if (!_start_block(block_at)) {
                            return false;
                        }
                        _phase = Phase::block_data;
                        break;
                    }
                    case Phase::block_data: {
                        if (_pending_out()) {
                            return false;   // hand out first
                        }
                        if (_decoded) {
                            if (!_chain.empty() && !_flushed) {
                                _flushed = true;
                                _chain.run(true);
                                if (_pending_out()) {
                                    return false;
                                }
                            }
                            if ((_header.compressed != UINT64_MAX && _header.compressed != _decoder->taken()) ||
                                (_header.uncompressed != UINT64_MAX && _header.uncompressed != _block_out)) {
                                _fail(errc::corrupt, _block_at, "xz: a block's sizes differ from its header's");
                                return false;
                            }
                            _phase = Phase::block_end;
                            break;
                        }
                        if (_decode()) {
                            return true;
                        }
                        if (_error) {
                            return false;
                        }
                        break;
                    }
                    case Phase::block_end: {
                        uint64_t compressed = _decoder->taken();
                        size_t pad = size_t((4 - (_header.size + compressed) % 4) % 4);
                        size_t cs = f::check_size(_check_type);
                        if (_have() < pad + cs) {
                            if (_source_ended) {
                                _fail(errc::unexpected_end, _offset() + _have(), "xz: unexpected end in a block's check");
                                return false;
                            }
                            return true;
                        }
                        for (size_t i = 0; i < pad; ++i) {
                            if (_at()[i]) {
                                _fail(errc::corrupt, _offset() + i, "xz: block padding not zero");
                                return false;
                            }
                        }
                        if (cs) {
                            uint8_t sum[64];
                            _check.value(sum);
                            if (std::memcmp(sum, _at() + pad, cs) != 0) {
                                _fail(errc::checksum, _offset() + pad, "xz: the block's check does not match its data");
                                return false;
                            }
                        }
                        _in_begin += pad + cs;
                        _records.push_back({_header.size + compressed + cs, _block_out});
                        _phase = Phase::block_start;
                        break;
                    }
                    case Phase::index: {
                        size_t used;
                        auto r = _index.step(_at(), _have(), _records, used);
                        if (r.status == detail::Parsed::more) {
                            // what it read is taken: the index goes on in the next bytes
                            _in_begin += used;
                            if (_source_ended) {
                                _fail(errc::unexpected_end, _offset(), "xz: unexpected end in the index");
                                return false;
                            }
                            return true;
                        }
                        if (r.status == detail::Parsed::failed) {
                            _fail(r.code, _offset(), r.text);
                            return false;
                        }
                        _in_begin += used;
                        _phase = Phase::footer;
                        break;
                    }
                    case Phase::footer: {
                        if (!_parsed(f::parse_footer(_at(), _have(), _index.size(), _check_type), need)) {
                            return need;
                        }
                        _zeros = 0;
                        _phase = Phase::stream_padding;
                        break;
                    }
                    case Phase::stream_padding: {
                        while (_have() && _at()[0] == 0) {
                            ++_in_begin;
                            ++_zeros;
                        }
                        if (!_have()) {
                            if (!_source_ended) {
                                return true;
                            }
                            if (_zeros % 4) {
                                _fail(errc::corrupt, _offset(), "xz: stream padding not a multiple of four bytes");
                                return false;
                            }
                            _ended = true;
                            return false;
                        }
                        if (_zeros % 4) {
                            _fail(errc::corrupt, _offset(), "xz: stream padding not a multiple of four bytes");
                            return false;
                        }
                        _phase = Phase::stream_header;
                        break;
                    }
                }
            }
        }

        bool _start_block(uint64_t block_at) {
            uint32_t dictionary = _header.filters[_header.count - 1].value;
            if (xz::_memory(dictionary, _header.uncompressed) > _limits.max_memory) {
                _fail(errc::too_large, block_at, "xz: the dictionary needs more memory than the limit allows");
                return false;
            }
            uint64_t d = std::max(dictionary, detail::lzma_model::DictionaryMin);
            size_t want = size_t(std::max<uint64_t>(std::min(d, _header.uncompressed), 1));
            if (_window_capacity < want || _window_capacity > want * 2) {
                _window.reset(new uint8_t[want]);
                _window_capacity = want;
            }
            _window_size = want;
            _pos = _from = 0;
            _decoder->reset(dictionary);
            detail::xz_format::decoding_chain(_header, _chain);
            _check.reset(_check_type);
            _block_at = block_at;
            _block_out = 0;
            _decoded = false;
            _flushed = false;
            return true;
        }

        // Decodes some of the block into the window (and through the
        // filters); true when it needs more input
        bool _decode() {
            if (_pos == _window_size) {
                _pos = _from = 0;
            }
            size_t limit = std::min(_window_size, _pos + Chunk);
            if (!_chain.empty()) {
                limit = std::min(limit, _pos + _chain.room());
            }
            const uint8_t* in = _at();
            const uint8_t* end = _input.get() + _in_end;
            bool ended = _source_ended;
            if (_header.compressed != UINT64_MAX) {
                uint64_t left = _header.compressed - std::min(_header.compressed, _decoder->taken());
                if (left <= uint64_t(end - in)) {
                    end = in + left;
                    ended = true;
                }
            }
            size_t before = _pos;
            auto st = _decoder->decode<false>(_window.get(), _window_size, _pos, limit, in, end, ended);
            _in_begin = size_t(in - _input.get());
            size_t made = _pos - before;
            _block_out += made;
            if (made && !_chain.empty()) {
                _chain.push(_window.get() + before, made);
                _chain.run(false);
                _from = _pos;
            }
            switch (st) {
                case detail::LzmaStatus::done:
                    _decoded = true;
                    return false;
                case detail::LzmaStatus::failed:
                    _fail(_decoder->error, _block_at + _header.size + _decoder->taken(), _decoder->error_text);
                    return false;
                case detail::LzmaStatus::need_room:
                    return false;
                case detail::LzmaStatus::need_input:
                    return made == 0;
            }
            return false;
        }

        // room for more input: what is left moved to the front
        size_t _make_room() {
            if (_in_begin) {
                std::memmove(_input.get(), _input.get() + _in_begin, _in_end - _in_begin);
                _consumed += _in_begin;
                _in_end -= _in_begin;
                _in_begin = 0;
            }
            return InputBytes - _in_end;
        }

        optional<io::error> _fill() {
            size_t room = _make_room();
            auto r = _in.read(slice<byte>(reinterpret_cast<byte*>(_input.get() + _in_end), room));
            return _took(r, r ? *r : 0);
        }

        // A task's read is given a managed block, which the slice holds:
        // the read may outlive this frame on a pool
        async::task<optional<io::error>> _async_fill() {
            size_t room = std::min(_make_room(), config::io_copy_buffer_size);
            if (!_block) {
                _block = make_tracked<io::detail::CopyBlock>();
            }
            auto r = co_await _in.async_read(slice<byte>(_block, _block->data(), room));
            if (r && *r) {
                std::memcpy(_input.get() + _in_end, _block->data(), *r);
            }
            co_return _took(r, r ? *r : 0);
        }

        optional<io::error> _took(const expected<size_t, io::error>& r, size_t n) {
            if (!r) {
                _error = error(r.error(), _consumed + _in_end);
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
        std::unique_ptr<detail::Lzma2Decoder> _decoder;
        std::unique_ptr<uint8_t[]> _input;
        tracked_ptr<io::detail::CopyBlock> _block;
        std::unique_ptr<uint8_t[]> _window;   // the block's dictionary, round which pos goes
        size_t _window_capacity = 0;
        size_t _window_size = 0;
        size_t _pos = 0;          // the end of the decoded bytes in the window
        size_t _from = 0;         // the first not yet handed out (no filters)
        size_t _in_begin = 0;     // the input not yet taken: [_in_begin, _in_end)
        size_t _in_end = 0;
        uint64_t _consumed = 0;
        detail::FilterChain _chain;
        detail::xz_format::Check _check;
        detail::xz_format::BlockHeader _header;
        detail::xz_format::IndexReader _index;
        std::vector<detail::xz_format::Record> _records;
        uint64_t _block_at = 0;
        uint64_t _block_out = 0;
        uint64_t _streams = 0;
        size_t _zeros = 0;
        uint8_t _check_type = 0;
        Phase _phase = Phase::stream_header;
        bool _decoded = false;    // the block's LZMA2 data ended
        bool _flushed = false;
        bool _source_ended = false;
        bool _ended = false;
        optional<error> _error;
        size_t _since_yield = 0;
    };
}
