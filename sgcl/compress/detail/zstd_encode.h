//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec_stream.h"
#include "zstd_block_encode.h"
#include "zstd_decode.h"
#include "zstd_match.h"
#include "../../hash/xxhash.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

// Zstandard's frame encoder: the header (a single segment when the content's
// size is known and fits the window, else a window descriptor), blocks of up
// to 128 KB — compressed where that is smaller, one byte repeated where the
// block is one byte, raw otherwise — and the content's checksum. A block
// kept raw leaves the encoder's state as it was, as the decoder's is left.
namespace sgcl::compress::detail {
    // What the encoder takes from the options, checked
    struct ZstdSettings {
        int level = 3;
        bool checksum = true;
        bool content_size = true;
        unsigned window_log = 0;
        bool write_dictionary_id = true;
        const char* error = nullptr;

        template<class O>
        static ZstdSettings of(const O& o) noexcept {
            ZstdSettings s;
            s.level = o.level.value();
            s.checksum = o.checksum;
            s.content_size = o.content_size;
            s.window_log = o.window_log;
            s.write_dictionary_id = o.write_dictionary_id;
            if (s.window_log && (s.window_log < 10 || s.window_log > 31)) {
                s.error = "a window_log of 10..31";
            }
            return s;
        }
    };

    // The frame's header
    template<class Out>
    void zstd_frame_header(Out& out, const ZstdSettings& s, unsigned window_log, const uint64_t* content_size, uint32_t dictionary_id) noexcept {
        uint8_t h[18];
        size_t k = 0;
        h[k++] = 0x28;
        h[k++] = 0xB5;
        h[k++] = 0x2F;
        h[k++] = 0xFD;
        const bool single = content_size && *content_size <= (uint64_t(1) << window_log);
        unsigned fcs = 0;
        if (content_size) {
            const uint64_t v = *content_size;
            fcs = v < 256 ? 0 : v < 65536 + 256 ? 1 : v <= 0xFFFFFFFFu ? 2 : 3;
            if (fcs == 0 && !single) {
                fcs = 1;   // without a single segment, flag 0 means no size at all
            }
        }
        const unsigned did = !dictionary_id ? 0 : dictionary_id < 256 ? 1 : dictionary_id < 65536 ? 2 : 3;
        h[k++] = uint8_t((content_size ? fcs : 0) << 6 | (single ? 0x20 : 0) | (s.checksum ? 0x04 : 0) | did);
        if (!single) {
            h[k++] = uint8_t((window_log - 10) << 3);
        }
        const unsigned did_bytes = did == 0 ? 0 : did == 1 ? 1 : did == 2 ? 2 : 4;
        for (unsigned i = 0; i < did_bytes; ++i) {
            h[k++] = uint8_t(dictionary_id >> (8 * i));
        }
        if (content_size) {
            uint64_t v = *content_size;
            const unsigned bytes = fcs == 0 ? 1 : fcs == 1 ? 2 : fcs == 2 ? 4 : 8;
            if (fcs == 1) {
                v -= 256;
            }
            for (unsigned i = 0; i < bytes; ++i) {
                h[k++] = uint8_t(v >> (8 * i));
            }
        }
        append_bytes(out, h, k);
    }

    // The engine of a frame: the match finder, the state the decoder will
    // mirror, the room of a block's sequences and literals and of its output
    struct ZstdEngine {
        ZstdMatcher matcher;
        ZstdEncodeState state;
        ZstdBlockWork work;
        std::unique_ptr<ZstdSequence[]> seqs {new ZstdSequence[ZstdBlockWork::MaxSequences]};
        std::unique_ptr<uint8_t[]> literals {new uint8_t[ZstdBlockMax + ZstdOutSlack]};
        std::unique_ptr<uint8_t[]> block {new uint8_t[5 * ZstdBlockMax + 4096]};
        uint32_t next = 0;   // the first index never used
        bool raw_literals = false;   // the negative levels: literals left raw (faster, as zstd --fast)

        // One block [src, src + n), its history from low (indexes from
        // base), written as a block of the frame (last says whether it ends it)
        template<class Out>
        void compress(Out& out, const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, bool last) noexcept {
            const uint32_t end = uint32_t(src + n - base);
            if (end > next) {
                next = end;
            }
            auto header = [&](unsigned type, size_t size) noexcept {
                const uint32_t v = uint32_t(last) | type << 1 | uint32_t(size) << 3;
                const uint8_t b[3] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16)};
                append_bytes(out, b, 3);
            };
            if (n == 0) {
                header(0, 0);
                return;
            }
            bool same = true;
            for (size_t i = 1; i < n && same; ++i) {
                same = src[i] == src[0];
            }
            if (same && n > 3) {
                header(1, n);
                append_bytes(out, src, 1);
                return;
            }
            uint32_t rep[3] = {state.rep[0], state.rep[1], state.rep[2]};
            ZstdSink sink {seqs.get(), literals.get()};
            matcher.parse(base, low, src, n, rep, sink);
            const size_t count = size_t(sink.seq - seqs.get());
            const size_t lit_count = size_t(sink.lit - literals.get());
            const size_t size = zstd_write_block(seqs.get(), count, literals.get(), lit_count, state, work, block.get(), raw_literals);
            if (size >= n) {
                // kept raw: the decoder's state does not move
                header(0, n);
                append_bytes(out, src, n);
                return;
            }
            zstd_commit_block(count, rep, state, work);
            header(2, size);
            append_bytes(out, block.get(), size);
        }

        // The index a stretch of n bytes may start at, with `keep` bytes of
        // history before it: the tables shifted down before 2^31
        uint32_t room(size_t n, size_t keep) noexcept {
            if (uint64_t(next) + n + ZstdBlockMax >= (uint64_t(1) << 31)) {
                // a multiple of 2^chain_log: the tree's nodes sit at their
                // position's index modulo that, and stay where they are
                const uint32_t align = uint32_t(1) << matcher.params().chain_log;
                const uint32_t delta = (next > keep ? next - uint32_t(keep) : 0) & ~(align - 1);
                matcher.shift(delta);
                next -= delta;
            }
            return next;
        }
    };

    // A whole compress in memory with no dictionary: the blocks straight
    // from the input, its history the input itself
    template<class Out>
    void zstd_compress_frame(Out& out, const ZstdSettings& s, const uint8_t* p, size_t n) noexcept {
        const uint64_t size = n;
        const ZstdParams params = zstd_params(s.level, s.content_size ? size : UINT64_MAX, s.window_log);
        zstd_frame_header(out, s, params.window_log, s.content_size ? &size : nullptr, 0);
        std::unique_ptr<ZstdEngine> engine(new ZstdEngine);
        engine->matcher.configure(params);
        engine->raw_literals = s.level < 0;
        const size_t window = size_t(1) << params.window_log;
        const size_t block_max = std::min(window, ZstdBlockMax);
        size_t at = 0;
        do {
            const size_t k = std::min(block_max, n - at);
            const size_t keep = std::min(at, window);
            const uint32_t start = engine->room(k, keep);
            const uint8_t* src = p + at;
            engine->compress(out, src - start, p, src, k, at + k == n);
            at += k;
        } while (at < n);
        if (s.checksum) {
            const uint32_t c = uint32_t(hash::detail::xxh64(p, n, 0));
            const uint8_t b[4] = {uint8_t(c), uint8_t(c >> 8), uint8_t(c >> 16), uint8_t(c >> 24)};
            append_bytes(out, b, 4);
        }
    }

    // The encoder's state at the start of a frame made with a dictionary of
    // zstd's format: its offsets, its Huffman table (for literals that use
    // the table before) and its three tables (for the mode "the one before")
    inline void zstd_state_from_dictionary(const ZstdDictionaryData& d, ZstdEncodeState& st) noexcept {
        st.reset();
        if (!d.has_entropy) {
            return;
        }
        for (int i = 0; i < 3; ++i) {
            st.rep[i] = d.entropy.rep[i];
        }
        // the Huffman lengths from the decoding table's entries
        const HufTable& h = d.entropy.huf;
        std::fill(std::begin(st.huf_lengths), std::end(st.huf_lengths), uint8_t(0));
        const unsigned size = 1u << h.max_bits;
        for (unsigned u = 0; u < size; ++u) {
            st.huf_lengths[h.entry[u].symbol] = h.entry[u].bits;
        }
        st.huf_max_bits = h.max_bits;
        huf_codes(st.huf_lengths, 256, h.max_bits, st.huf_codes);
        st.huf_valid = true;
        st.ll.encoder.build(d.entropy.literal.entry, d.entropy.literal.log);
        st.of.encoder.build(d.entropy.offset.entry, d.entropy.offset.log);
        st.ml.encoder.build(d.entropy.match.entry, d.entropy.match.log);
        st.ll.valid = st.of.valid = st.ml.valid = true;
    }

    // The frame's writer: what is written gathers in a buffer of the
    // dictionary, the history and up to two blocks; a block goes out when a
    // byte past it has come (the last block is marked only at the close),
    // all of it at a flush; when the buffer fills, its last window moves to
    // the front
    class ZstdFrameEncoder {
    public:
        static constexpr const char* name = "zstd";

        template<class O>
        explicit ZstdFrameEncoder(const O& o, uint64_t size_hint = UINT64_MAX) noexcept
        : _s(ZstdSettings::of(o))
        , _dictionary(ZstdDictionaryAccess::get(o.dictionary)) {
            if (_s.error) {
                return;
            }
            const int level = _s.level;
            if (level < -131072 || level > 22 || level == 0) {
                _s.error = "a level of -131072..-1 or 1..22";
                return;
            }
            // with a dictionary the window covers it too, so that a short
            // input reaches all of it
            const size_t ds0 = _dictionary ? _dictionary->content.size() : 0;
            _params = zstd_params(level, size_hint == UINT64_MAX ? size_hint : size_hint + ds0, _s.window_log);
            _window = size_t(1) << _params.window_log;
            _block_max = std::min(_window, ZstdBlockMax);
            const size_t d = _dictionary ? _dictionary->content.size() : 0;
            _capacity = d + 2 * _window + 2 * _block_max + ZstdOutSlack;
            _buffer.reset(new uint8_t[_capacity]);
            _engine.reset(new ZstdEngine);
            _engine->matcher.configure(_params);
            _engine->raw_literals = level < 0;
            reset();
        }

        SGCL_INLINE_HOT const char* setup_error() const noexcept {
            return _s.error;
        }

        template<class Out>
        void start(Out& out, const uint64_t* content_size = nullptr) noexcept {
            const uint32_t id = _dictionary && _s.write_dictionary_id ? _dictionary->id : 0;
            _sized = content_size && _s.content_size;
            zstd_frame_header(out, _s, _params.window_log, _sized ? content_size : nullptr, id);
        }

        template<class Out>
        void write(const uint8_t* p, size_t n, Out& out) noexcept {
            if (_s.checksum) {
                _hasher.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            }
            while (n) {
                if (_end + _block_max + ZstdOutSlack > _capacity) {
                    _slide();
                }
                const size_t room = _capacity - ZstdOutSlack - _end;
                const size_t k = std::min(n, room);
                sgcl::detail::copy_bytes(_buffer.get() + _end, p, k);
                _end += k;
                p += k;
                n -= k;
                // whole blocks with a byte after them go out
                while (_end - _pending > _block_max) {
                    _block(out, _block_max, false);
                }
            }
        }

        template<class Out>
        void flush(Out& out) noexcept {
            while (_end > _pending) {
                _block(out, std::min(_block_max, _end - _pending), false);
            }
        }

        template<class Out>
        void finish(Out& out) noexcept {
            while (_end - _pending > _block_max) {
                _block(out, _block_max, false);
            }
            _block(out, _end - _pending, true);
            if (_s.checksum) {
                const uint32_t c = uint32_t(_hasher.value());
                const uint8_t b[4] = {uint8_t(c), uint8_t(c >> 8), uint8_t(c >> 16), uint8_t(c >> 24)};
                append_bytes(out, b, 4);
            }
        }

        void reset() noexcept {
            if (_s.error) {
                return;
            }
            _hasher.reset();
            _engine->matcher.reset();
            const ZstdDictionaryData* d = _dictionary.get();
            const size_t ds = d ? d->content.size() : 0;
            // the indexes go on from where the last stream left them
            _origin = _engine->room(_capacity, 0);
            if (ds) {
                sgcl::detail::copy_bytes(_buffer.get(), d->content.data(), ds);
                _engine->matcher.load(_buffer.get() - _origin, _buffer.get(), _buffer.get() + ds);
                zstd_state_from_dictionary(*d, _engine->state);
            } else {
                _engine->state.reset();
            }
            _engine->next = _origin + uint32_t(ds);
            _low = 0;
            _pending = ds;
            _end = ds;
        }

    private:
        template<class Out>
        void _block(Out& out, size_t k, bool last) noexcept {
            uint8_t* const src = _buffer.get() + _pending;
            _engine->compress(out, _buffer.get() - _origin, _buffer.get() + _low, src, k, last);
            _pending += k;
        }

        // The last window of what went out, and what has not, to the front
        void _slide() noexcept {
            const size_t keep_from = _pending > _window ? _pending - _window : 0;
            const size_t from = std::max(keep_from, _low);
            if (from == 0) {
                return;
            }
            std::memmove(_buffer.get(), _buffer.get() + from, _end - from);
            _origin += uint32_t(from);
            _pending -= from;
            _end -= from;
            _low = _low > from ? _low - from : 0;
            if (uint64_t(_origin) + _capacity >= (uint64_t(1) << 31) - ZstdBlockMax) {
                // a multiple of the tree's ring (as ZstdEngine::room's)
                const uint32_t align = uint32_t(1) << _params.chain_log;
                const uint32_t delta = _origin & ~(align - 1);
                _engine->matcher.shift(delta);
                _engine->next -= std::min(_engine->next, delta);
                _origin -= delta;
            }
        }

        ZstdSettings _s;
        root_ptr<ZstdDictionaryData> _dictionary;   // a root: the encoder lives outside the managed heap
        ZstdParams _params {};
        size_t _window = 0;
        size_t _block_max = 0;
        size_t _capacity = 0;
        std::unique_ptr<uint8_t[]> _buffer;
        std::unique_ptr<ZstdEngine> _engine;
        uint32_t _origin = 0;    // the index of the buffer's first byte
        size_t _low = 0;         // the history from here
        size_t _pending = 0;     // the bytes not yet in a block from here
        size_t _end = 0;         // the end of what was written
        bool _sized = false;
        hash::xxh64 _hasher;
    };
}
