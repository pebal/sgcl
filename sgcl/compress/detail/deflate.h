//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "inflate.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace sgcl::compress::detail {
    // The encoder of DEFLATE (RFC 1951): LZ77 over a window of 32 KB. Levels
    // 1 to 6 (6 the default) look a position up in tables of the latest
    // position under a hash of four bytes (and of seven from level 3), no
    // chains, with one step of lazy matching from level 4: the kind of
    // encoder Go's levels 1 to 6 are. Levels 7 to 9 walk chains of positions
    // under a hash of three bytes with lazy matching (a match taken only when
    // the next position has no longer one), zlib's 7 to 9.
    // The positions in the tables are absolute, so a move of the window
    // copies its last 32 KB and touches no table. The symbols of a block are
    // gathered with their frequencies, and the block written with the
    // shortest of its three forms — its own Huffman codes, the fixed codes,
    // or stored. Level 0 frames the input as stored blocks, a whole block
    // straight from the input. Plain memory: the window, the tables and the
    // symbols are std::vectors of the encoder.

    // The two ways the encoder puts bytes into its output: a std::vector
    // here, the stream's managed output beside its own type (stream.h)
    SGCL_INLINE_HOT void append_bytes(std::vector<uint8_t>& out, const uint8_t* p, size_t n) noexcept {
        out.insert(out.end(), p, p + n);
    }

    SGCL_INLINE_HOT void append_byte(std::vector<uint8_t>& out, uint8_t b) noexcept {
        out.push_back(b);
    }

    // Appends bits least significant first, as the format packs them
    template<class Out>
    class BitWriter {
    public:
        SGCL_INLINE_HOT explicit BitWriter(Out& out) noexcept
        : _out(out) {
        }

        void put(uint32_t value, uint32_t count) noexcept {
            _bits |= uint64_t(value) << _count;
            _count += count;
            if (_count >= 32) {
                uint8_t b[4] = {uint8_t(_bits), uint8_t(_bits >> 8), uint8_t(_bits >> 16), uint8_t(_bits >> 24)};
                append_bytes(_out, b, 4);
                _bits >>= 32;
                _count -= 32;
            }
        }

        // To the byte boundary with zero bits
        void align() noexcept {
            while (_count > 0) {
                append_byte(_out, uint8_t(_bits));
                _bits >>= 8;
                _count = _count > 8 ? _count - 8 : 0;
            }
            _bits = 0;
        }

        SGCL_INLINE_HOT Out& out() noexcept {
            return _out;
        }

        SGCL_INLINE_HOT uint32_t pending_bits() const noexcept {
            return _count;
        }

        SGCL_INLINE_HOT uint64_t pending_value() const noexcept {
            return _bits;
        }

        SGCL_INLINE_HOT void restore(uint64_t bits, uint32_t count) noexcept {
            _bits = bits;
            _count = count;
        }

    private:
        Out& _out;
        uint64_t _bits = 0;
        uint32_t _count = 0;
    };

    // Code lengths of at most max_bits for these frequencies: a Huffman
    // code (the two least frequent merged, again and again), its lengths
    // clamped to max_bits and the Kraft sum brought back to one by
    // lengthening the shortest codes that can take it; symbols of
    // frequency 0 get no code. At least two codes are made when a table
    // has any, so that every code the encoder writes is complete.
    inline void huffman_lengths(const uint32_t* freq, unsigned count, unsigned max_bits, uint8_t* lengths) noexcept {
        std::fill(lengths, lengths + count, uint8_t(0));
        struct Leaf {
            uint32_t freq;
            uint16_t symbol;
        };
        Leaf leaves[320];
        unsigned n = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (freq[i]) {
                leaves[n++] = {freq[i], uint16_t(i)};
            }
        }
        if (n == 0) {
            return;
        }
        if (n == 1) {
            // a second code beside the only one, so that the code is complete
            lengths[leaves[0].symbol] = 1;
            lengths[leaves[0].symbol == 0 ? 1 : 0] = 1;
            return;
        }
        std::sort(leaves, leaves + n, [](const Leaf& a, const Leaf& b) noexcept {
            return a.freq != b.freq ? a.freq < b.freq : a.symbol < b.symbol;
        });
        // The two-queue construction over the sorted leaves: the internal
        // nodes come out in order of weight, so the smallest two are
        // always at the heads of the two queues
        uint32_t weight[640];
        int16_t parent[640];
        for (unsigned i = 0; i < n; ++i) {
            weight[i] = leaves[i].freq;
        }
        unsigned leaf = 0, node = n, next = n;
        auto smallest = [&]() noexcept {
            if (leaf < n && (node >= next || weight[leaf] <= weight[node])) {
                return leaf++;
            }
            return node++;
        };
        for (unsigned k = 0; k < n - 1; ++k) {
            unsigned a = smallest();
            unsigned b = smallest();
            weight[next] = weight[a] + weight[b];
            parent[a] = int16_t(next);
            parent[b] = int16_t(next);
            ++next;
        }
        // depths from the root down: the root is the last node
        uint8_t depth[640];
        depth[next - 1] = 0;
        for (int i = int(next) - 2; i >= 0; --i) {
            depth[i] = uint8_t(std::min(depth[parent[i]] + 1, 63));
        }
        // how many leaves at each length, clamped, then the Kraft sum fixed
        unsigned bl_count[64] = {};
        for (unsigned i = 0; i < n; ++i) {
            ++bl_count[std::min<unsigned>(depth[i], max_bits)];
        }
        for (unsigned d = max_bits + 1; d < 64; ++d) {
            bl_count[max_bits] += bl_count[d];
            bl_count[d] = 0;
        }
        uint64_t total = 0;
        for (unsigned len = 1; len <= max_bits; ++len) {
            total += uint64_t(bl_count[len]) << (max_bits - len);
        }
        while (total > (uint64_t(1) << max_bits)) {
            // one code of the longest length goes, and a shorter code is
            // split in two one level down: the sum drops by one unit
            --bl_count[max_bits];
            for (unsigned len = max_bits - 1; len > 0; --len) {
                if (bl_count[len]) {
                    --bl_count[len];
                    bl_count[len + 1] += 2;
                    break;
                }
            }
            --total;
        }
        // the longest codes to the least frequent symbols
        unsigned i = 0;
        for (unsigned len = max_bits; len >= 1; --len) {
            for (unsigned c = 0; c < bl_count[len]; ++c) {
                lengths[leaves[i++].symbol] = uint8_t(len);
            }
        }
    }

    // The canonical codes of the lengths, bit-reversed for writing
    inline void canonical_codes(const uint8_t* lengths, unsigned count, uint16_t* codes) noexcept {
        unsigned bl_count[16] = {};
        for (unsigned i = 0; i < count; ++i) {
            ++bl_count[lengths[i]];
        }
        bl_count[0] = 0;
        uint32_t next_code[16] = {};
        uint32_t code = 0;
        for (unsigned len = 1; len <= 15; ++len) {
            code = (code + bl_count[len - 1]) << 1;
            next_code[len] = code;
        }
        for (unsigned i = 0; i < count; ++i) {
            codes[i] = lengths[i] ? uint16_t(reverse_bits(next_code[lengths[i]]++, lengths[i])) : 0;
        }
    }

    inline uint32_t length_code(uint32_t length) noexcept {
        // 3..258 to 257..285
        static const auto table = [] {
            struct T {
                uint8_t code[259];
            } t{};
            for (unsigned c = 0; c < 29; ++c) {
                unsigned hi = c == 28 ? 258 : LengthBase[c] + (1u << LengthExtra[c]) - 1;
                for (unsigned l = LengthBase[c]; l <= hi && l <= 258; ++l) {
                    t.code[l] = uint8_t(c);
                }
            }
            t.code[258] = 28;
            return t;
        }();
        return table.code[length];
    }

    inline uint32_t distance_code(uint32_t distance) noexcept {
        // 1..32768: the codes of 1..256 from a table, the larger by the top bits
        static const auto table = [] {
            struct T {
                uint8_t small[257];
                uint8_t large[256];
            } t{};
            for (unsigned c = 0; c < 30; ++c) {
                unsigned lo = DistanceBase[c];
                unsigned hi = lo + (1u << DistanceExtra[c]) - 1;
                for (unsigned d = lo; d <= hi && d <= 256; ++d) {
                    t.small[d] = uint8_t(c);
                }
                for (unsigned d = lo; d <= hi; ++d) {
                    if (d > 256) {
                        t.large[(d - 1) >> 7] = uint8_t(c);
                    }
                }
            }
            return t;
        }();
        return distance <= 256 ? table.small[distance] : table.large[(distance - 1) >> 7];
    }

    // The tuning of a level. Levels 1 to 6 are the table encoder: one
    // table of the latest position under a hash of four bytes (hash_bits)
    // and from level 3 one under seven (long_bits), one or two candidates a
    // position and no chains, the positions inside a match inserted every
    // insert_step-th (0: none), a run of misses skipped faster at levels 1
    // and 2, one step of lazy matching from 4, two positions a bucket of the
    // long table at 6. Levels 7 to 9 are chains
    // under a hash of three bytes (hash_bits of heads, the previous
    // position of each in a ring of 32 KB; four bytes made binary data 2 %
    // larger, the matches of three bytes lost), zlib's tuning: matches this long
    // (good) are good enough to search a quarter as hard, the lazy search
    // stops above max_lazy, a match of nice length ends the search, at most
    // max_chain positions are tried, zlib's own 7 to 9. Go 1.27's levels 1
    // to 6 are table encoders too: Go's 6 is this 6, in speed and in size.
    struct LevelConfig {
        uint16_t good, max_lazy, nice, max_chain;
        bool lazy;
        bool fast;
        uint8_t hash_bits;
        uint8_t long_bits;
        uint8_t insert_step;
        bool skip;
        uint8_t ways = 1;   // the positions a bucket of the long table keeps, newest first
    };

    inline constexpr LevelConfig level_config(int level) noexcept {
        switch (level) {
            case 1: return {0, 0, 258, 0, false, true, 14, 0, 4, true};
            case 2: return {0, 0, 258, 0, false, true, 15, 0, 2, true};
            case 3: return {0, 0, 258, 0, false, true, 15, 15, 2, false};
            case 4: return {0, 0, 258, 0, true, true, 16, 15, 1, false};
            case 5: return {0, 0, 258, 0, true, true, 16, 16, 1, false};
            case 7: return {8, 32, 128, 256, true, false, 16, 0, 0, false};
            case 8: return {32, 128, 258, 1024, true, false, 17, 0, 0, false};
            case 9: return {32, 258, 258, 4096, true, false, 17, 0, 0, false};
            default: return {0, 0, 258, 0, true, true, 16, 16, 1, false, 2};   // 6, the default
        }
    }

    // How many bytes of b match a from byte start on, at most max_len:
    // sixteen at a step where NEON is, eight (XOR, trailing zeros) else
    inline uint32_t match_length(const uint8_t* a, const uint8_t* b, uint32_t start, uint32_t max_len) noexcept {
        uint32_t len = start;
#if defined(__ARM_NEON)
        // sixteen bytes a step: the bytes equal as a mask of four bits
        // each (vshrn), the first that differs its trailing zeros / 4
        while (len + 16 <= max_len) {
            uint8x16_t eq = vceqq_u8(vld1q_u8(a + len), vld1q_u8(b + len));
            uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
            if (mask != ~uint64_t(0)) {
                return std::min(len + (uint32_t(std::countr_zero(~mask)) >> 2), max_len);
            }
            len += 16;
        }
#endif
        while (len + 8 <= max_len) {
            uint64_t x, y;
            std::memcpy(&x, a + len, 8);
            std::memcpy(&y, b + len, 8);
            if (uint64_t d = x ^ y) {
                return std::min(len + (uint32_t(std::countr_zero(d)) >> 3), max_len);
            }
            len += 8;
        }
        while (len < max_len && a[len] == b[len]) {
            ++len;
        }
        return len;
    }

    class Deflater {
    public:
        static constexpr uint32_t WindowMask = WindowSize - 1;
        static constexpr uint32_t MinMatch = 3;
        static constexpr uint32_t MinLookahead = MaxMatch + MinMatch + 1;
        static constexpr uint32_t InputSize = uint32_t(64) << 10;       // the input matched between two moves of the window
        static constexpr uint32_t BufferSize = WindowSize + InputSize;   // the history and the input
        static constexpr uint32_t Slack = 16;                            // read past the end by the loads of 8 and 16 bytes
        static constexpr uint32_t StoredMax = 65535;
        static constexpr uint32_t MaxSymbols = 16384;
        static inline uint32_t rebase_at = uint32_t(1) << 31;          // absolute positions brought back to 0 past this (lowered by a test)
        static constexpr int HuffmanOnly = -2;

        // level: 0 stores, 1..9, HuffmanOnly; the dictionary's last 32 KB
        // are the history the first matches may reach
        SGCL_INLINE_HOT explicit Deflater(int level, const uint8_t* dictionary = nullptr, size_t dictionary_size = 0) noexcept
        : Deflater(level, false, dictionary, dictionary_size) {
        }

        // filtered: data of small differences, as PNG's filtered rows (zlib's
        // Z_FILTERED): the chain levels (7 to 9) take no match of 5 bytes or
        // fewer, the bytes going as literals, whose codes such data makes
        // short; the table levels (1 to 6) as without it
        Deflater(int level, bool filtered, const uint8_t* dictionary = nullptr, size_t dictionary_size = 0) noexcept
        : _level(level)
        , _filtered(filtered)
        , _config(level_config(level))
        , _window((level == 0 ? StoredMax : BufferSize) + Slack) {
            if (level >= 1) {
                _head.assign(size_t(1) << _config.hash_bits, 0u);
                if (_config.fast) {
                    if (_config.long_bits) {
                        _long.assign(size_t(_config.ways) << _config.long_bits, 0u);
                    }
                } else {
                    _prev.assign(WindowSize, 0u);
                }
            }
            _lit.reserve(MaxSymbols + 1);
            _dist.reserve(MaxSymbols + 1);
            _seed(dictionary, dictionary_size);
        }

        // Back to the start with the same memory, and a dictionary again
        SGCL_INLINE_HOT void reset(const uint8_t* dictionary, size_t dictionary_size) noexcept {
            reset();
            _seed(dictionary, dictionary_size);
        }

        // Back to the start at another level, for a stream of input_size
        // bytes, compressed as by one made for it (the one-shot compress's
        // Deflater, kept by the thread: stream.h). The tables are not
        // cleared, as reset() does not clear them: every entry, in a table
        // this level uses or not, is at most the old _offset + _end, and the
        // positions now begin more than a window past it, out of reach of
        // every match. They are cleared, and the positions begin at 0, when
        // the stream could reach the rebase: the rebase moves the chains'
        // ring by an amount not a multiple of its size, which one made for
        // the stream would not have met there. A table of another size is
        // resized, its new part zero; the window only grows (level 0 uses
        // its first StoredMax).
        void reset(int level, const uint8_t* dictionary, size_t dictionary_size, uint64_t input_size) noexcept {
            _reset(std::min<uint64_t>(dictionary_size, WindowSize) + input_size);
            _level = level;
            _filtered = false;
            _config = level_config(level);
            const size_t window = (level == 0 ? StoredMax : BufferSize) + Slack;
            if (_window.size() < window) {
                _window.resize(window);
            }
            if (level >= 1) {
                _head.resize(size_t(1) << _config.hash_bits);
                if (_config.fast) {
                    if (_config.long_bits) {
                        _long.resize(size_t(_config.ways) << _config.long_bits);
                    }
                } else {
                    _prev.resize(WindowSize);
                }
            }
            _seed(dictionary, dictionary_size);
        }

        // The dictionary's last 32 KB as the history, in the tables
        void _seed(const uint8_t* dictionary, size_t dictionary_size) noexcept {
            if (dictionary_size && _level != 0) {
                if (dictionary_size > WindowSize) {
                    dictionary += dictionary_size - WindowSize;
                    dictionary_size = WindowSize;
                }
                sgcl::detail::copy_bytes(_window.data(), dictionary, dictionary_size);
                _end = uint32_t(dictionary_size);
                _pos = _end;
                _block_start = _end;
                if (_level == HuffmanOnly) {
                    return;   // the history is never looked at (a kept Deflater may still hold tables of another level)
                }
                for (uint32_t p = 0; p + 8 <= _end; ++p) {
                    _config.fast ? _insert_fast(p) : (void)_insert(p);
                }
                if (!_config.fast) {
                    for (uint32_t p = _end >= 8 ? _end - 7 : 0; p + MinMatch <= _end; ++p) {
                        _insert(p);   // the last positions, which the chains take from 3 bytes
                    }
                }
            }
        }

        // Back to the start: the same level, no history, nothing pending.
        // The tables are not cleared: the positions of the new stream begin
        // more than a window past the last of the old one, so every entry
        // the old stream left is out of reach of every new match (cleared
        // only when the positions come near the rebase)
        SGCL_INLINE_HOT void reset() noexcept {
            _reset(0);
        }

        // reset(), the tables cleared also when the next stream's first
        // ahead bytes would bring its positions to the rebase; without
        // tables (level 0, Huffman only) the positions begin at 0, where
        // tables made for the next level start
        void _reset(uint64_t ahead) noexcept {
            if (_head.empty()) {
                _offset = 0;
            } else {
                uint64_t next = uint64_t(_offset) + _end + WindowSize + 1;
                if (next + ahead >= rebase_at) {
                    std::fill(_head.begin(), _head.end(), 0u);
                    std::fill(_prev.begin(), _prev.end(), 0u);
                    std::fill(_long.begin(), _long.end(), 0u);
                    next = 0;
                }
                _offset = uint32_t(next);
            }
            _end = _pos = _block_start = 0;
            _lit.clear();
            _dist.clear();
            std::fill(std::begin(_lit_freq), std::end(_lit_freq), 0u);
            std::fill(std::begin(_dist_freq), std::end(_dist_freq), 0u);
            _match_available = false;
            _prev_length = MinMatch - 1;
            _misses = 0;
            _bits_value = 0;
            _bits_count = 0;
        }

        // Compresses what the window holds of data and appends to out the
        // blocks it completes; the rest waits for more input, flush or finish
        template<class Out>
        void write(const uint8_t* data, size_t n, Out& out) noexcept {
            if (_level == 0) {
                _write_stored(data, n, out);
                return;
            }
            while (n) {
                if (_end == BufferSize) {
                    _slide();
                }
                size_t room = BufferSize - _end;
                size_t take = std::min(room, n);
                sgcl::detail::copy_bytes(_window.data() + _end, data, take);
                _end += uint32_t(take);
                data += take;
                n -= take;
                _compress(false, out);
            }
        }

        // Everything written so far out, and the output on a byte
        // boundary: a sync flush (an empty stored block after the data)
        template<class Out>
        void flush(Out& out) noexcept {
            if (_level != 0) {
                _compress(true, out);
            }
            auto w = _writer(out);
            if (_level == 0) {
                if (_end) {
                    _stored(false, w, _window.data(), _end);
                    _end = 0;
                }
            } else {
                _flush_block(false, w);
            }
            // the empty stored block
            w.put(0, 3);
            w.align();
            const uint8_t marker[4] = {0, 0, 0xFF, 0xFF};
            append_bytes(out, marker, 4);
            _save(w);
        }

        // The last block: everything out, the stream ended and aligned
        template<class Out>
        void finish(Out& out) noexcept {
            if (_level == 0) {
                auto w = _writer(out);
                _stored(true, w, _window.data(), _end);
                _end = 0;
                w.align();
                _save(w);
                return;
            }
            _compress(true, out);
            auto w = _writer(out);
            _flush_block(true, w);
            w.align();
            _save(w);
        }

    private:
        SGCL_INLINE_HOT static uint32_t _load32(const uint8_t* q) noexcept {
            uint32_t v;
            std::memcpy(&v, q, 4);
            return v;
        }

        SGCL_INLINE_HOT static uint64_t _load64(const uint8_t* q) noexcept {
            uint64_t v;
            std::memcpy(&v, q, 8);
            return v;
        }

        SGCL_INLINE_HOT uint32_t _hash4(uint32_t v) const noexcept {
            return (v * 0x9E3779B1u) >> (32 - _config.hash_bits);
        }

        SGCL_INLINE_HOT uint32_t _hash7(uint64_t v) const noexcept {
            return uint32_t(((v << 8) * 0xCF1BBCDCB7A56463ull) >> (64 - _config.long_bits));
        }

        // position p under its hash (chains); the head and the ring keep
        // the absolute position + 1, 0 is none
        uint32_t _insert(uint32_t p) noexcept {
            const uint32_t abs = _offset + p;
            uint32_t h = _hash4(_load32(_window.data() + p) & 0xFFFFFF);   // three bytes: the word's fourth is masked off
            uint32_t prev = _head[h];
            _prev[abs & WindowMask] = prev;
            _head[h] = abs + 1;
            return prev;
        }

        // position p in the tables of the table encoder (8 bytes readable)
        void _insert_fast(uint32_t p) noexcept {
            const uint64_t v = _load64(_window.data() + p);
            const uint32_t e = _offset + p + 1;
            _head[_hash4(uint32_t(v))] = e;
            if (_config.long_bits) {
                uint32_t* bucket = &_long[size_t(_hash7(v)) * _config.ways];
                if (_config.ways == 2) {
                    bucket[1] = bucket[0];
                }
                bucket[0] = e;
            }
        }

        // The longest match at p among the chain starting at candidate
        // (an absolute position + 1), longer than best; its distance in
        // *distance. A candidate is turned away by two loads when it cannot
        // win: its first three bytes (the hash's collisions) and the two
        // bytes where the best match so far ends.
        uint32_t _longest_match(uint32_t p, uint32_t candidate, uint32_t best, uint32_t& distance) const noexcept {
            uint32_t chain = _config.max_chain;
            if (best >= _config.good) {
                chain >>= 2;
            }
            const uint32_t abs = _offset + p;
            const uint32_t limit = abs > WindowSize ? abs - WindowSize : 0;
            uint32_t max_len = std::min<uint32_t>(MaxMatch, _end - p);
            if (best >= max_len || max_len < MinMatch) {
                return best;
            }
            uint32_t nice = std::min<uint32_t>(_config.nice, max_len);
            const uint8_t* w = _window.data();
            const uint8_t* cur = w + p;
            auto load16 = [](const uint8_t* q) noexcept {
                uint16_t v;
                std::memcpy(&v, q, 2);
                return v;
            };
            // the window has slack past its end, so the word at the start reads in bounds
            const uint32_t start = _load32(cur) & 0xFFFFFF;
            uint16_t tail = best >= 1 ? load16(cur + best - 1) : 0;
            while (candidate > limit && chain--) {
                uint32_t c = candidate - 1;
                const uint8_t* m = w + (c - _offset);
                candidate = _prev[c & WindowMask];
                // a slot overwritten by a newer position (32 KB on) would lead
                // the chain round again: it goes only to older positions
                if (candidate > c) {
                    candidate = 0;
                }
                if ((best >= 1 && load16(m + best - 1) != tail) || (_load32(m) & 0xFFFFFF) != start) {
                    continue;
                }
                uint32_t len = match_length(cur, m, MinMatch, max_len);
                if (len > best) {
                    best = len;
                    distance = abs - c;
                    if (len >= nice) {
                        break;
                    }
                    tail = load16(cur + best - 1);
                }
            }
            return best;
        }

        // The match at p of the table encoder: the long table's candidate
        // (level 3 on), then the short one's; at least 4 bytes, within the
        // window. The tables get p meanwhile when insert is asked
        uint32_t _table_match(uint32_t p, uint32_t& distance, bool insert) noexcept {
            const uint8_t* w = _window.data();
            const uint64_t v = _load64(w + p);
            const uint32_t abs = _offset + p;
            const uint32_t max_len = std::min<uint32_t>(MaxMatch, _end - p);
            uint32_t best = 0;
            auto consider = [&](uint32_t e) noexcept {
                if (e == 0) {
                    return;
                }
                const uint32_t c = e - 1;
                if (c >= abs || abs - c > WindowSize) {
                    return;
                }
                const uint8_t* m = w + (c - _offset);
                if (_load32(m) != uint32_t(v)) {
                    return;
                }
                uint32_t len = match_length(w + p, m, 4, max_len);
                if (len > best) {
                    best = len;
                    distance = abs - c;
                }
            };
            if (_config.long_bits) {
                uint32_t* bucket = &_long[size_t(_hash7(v)) * _config.ways];
                consider(bucket[0]);
                if (_config.ways == 2) {
                    consider(bucket[1]);
                }
                if (insert) {
                    if (_config.ways == 2) {
                        bucket[1] = bucket[0];
                    }
                    bucket[0] = abs + 1;
                }
            }
            uint32_t& slot = _head[_hash4(uint32_t(v))];
            if (best < 16) {
                consider(slot);
            }
            if (insert) {
                slot = abs + 1;
            }
            return best;
        }

        SGCL_INLINE_HOT void _literal(uint8_t b) noexcept {
            _lit.push_back(b);
            _dist.push_back(0);
            ++_lit_freq[b];
        }

        SGCL_INLINE_HOT void _match(uint32_t length, uint32_t distance) noexcept {
            uint32_t lc = length_code(length);
            _lit.push_back(uint16_t(253 + length));   // 256.. for 3..258: a value of 256 or more is a match
            _dist.push_back(uint16_t(distance));
            ++_lit_freq[257 + lc];
            ++_dist_freq[distance_code(distance)];
        }

        // The window's last 32 KB before the next byte to match moved to
        // the front, with what follows it; the tables keep absolute
        // positions and are not touched (brought back to 0 only past the
        // rebase). The two runs never overlap: the window slides only when
        // it is full, and _compress has then matched to within MinLookahead
        // of its end, so what is moved (32 KB and the lookahead) starts past
        // 64 KB less the lookahead
        void _slide() noexcept {
            const uint32_t keep = std::min(_pos, WindowSize);
            const uint32_t delta = _pos - keep;
            sgcl::detail::copy_bytes(_window.data(), _window.data() + delta, _end - delta);
            _offset += delta;
            _end -= delta;
            _pos -= delta;
            _block_start -= delta;
            if (_offset >= rebase_at) {
                const uint32_t r = _offset;
                for (auto* t : {&_head, &_prev, &_long}) {
                    for (auto& e : *t) {
                        e = e > r ? e - r : 0;
                    }
                }
                _offset = 0;
            }
        }

        // Matches over the window up to MinLookahead from its end (or to
        // its end when there is no more input), writing out the blocks
        // that fill up
        template<class Out>
        void _compress(bool final, Out& out) noexcept {
            uint32_t stop = final ? _end : (_end > MinLookahead ? _end - MinLookahead : 0);
            if (_level == HuffmanOnly) {
                while (_pos < stop) {
                    _literal(_window[_pos++]);
                    _full(out);
                }
                return;
            }
            if (_config.fast) {
                _compress_fast(stop, out);
                return;
            }
            while (_pos < stop) {
                uint32_t candidate = _pos + MinMatch <= _end ? _insert(_pos) : 0;
                uint32_t distance = 0;
                uint32_t len = MinMatch - 1;
                if (candidate && _prev_length < _config.max_lazy) {
                    len = _longest_match(_pos, candidate, _prev_length, distance);
                    // a match of three bytes far away costs more than three
                    // literals; filtered data takes none of 5 bytes or fewer
                    if ((len == MinMatch && distance > 256) || (_filtered && len <= 5)) {
                        len = MinMatch - 1;
                    }
                }
                if (_prev_length >= MinMatch && len <= _prev_length) {
                    // the match at the previous position is the better one
                    _match(_prev_length, _prev_distance);
                    uint32_t last = _pos - 1 + _prev_length;
                    for (uint32_t q = _pos + 1; q < last && q + MinMatch <= _end; ++q) {
                        _insert(q);
                    }
                    _pos = last;
                    _match_available = false;
                    _prev_length = MinMatch - 1;
                } else {
                    if (_match_available) {
                        _literal(_window[_pos - 1]);
                    }
                    _match_available = true;
                    _prev_length = len;
                    _prev_distance = distance;
                    ++_pos;
                }
                _full(out);
            }
            if (final && _match_available) {
                _literal(_window[_pos - 1]);
                _match_available = false;
                _prev_length = MinMatch - 1;
            }
        }

        // The table encoder (levels 1 to 5) from _pos to stop
        template<class Out>
        void _compress_fast(uint32_t stop, Out& out) noexcept {
            const uint8_t* w = _window.data();
            while (_pos < stop) {
                if (_pos + 8 > _end) {
                    _literal(w[_pos++]);   // the last bytes of the stream
                    _full(out);
                    continue;
                }
                uint32_t distance = 0;
                uint32_t len = _table_match(_pos, distance, true);
                if (len == 0) {
                    _literal(w[_pos++]);
                    if (_config.skip && ++_misses > 32) {
                        // a run that does not compress: further on in growing steps
                        for (uint32_t k = std::min<uint32_t>((_misses - 32) >> 5, 32); k && _pos < stop; --k) {
                            _literal(w[_pos++]);
                        }
                    }
                    _full(out);
                    continue;
                }
                _misses = 0;
                if (_config.lazy && len < 32 && _pos + 1 + 8 <= _end && _pos + 1 < stop) {
                    uint32_t next_distance = 0;
                    uint32_t next = _table_match(_pos + 1, next_distance, true);
                    if (next > len) {
                        _literal(w[_pos++]);
                        len = next;
                        distance = next_distance;
                    }
                }
                _match(len, distance);
                const uint32_t last = _pos + len;
                if (_config.insert_step) {
                    for (uint32_t q = _pos + 1; q < last && q + 8 <= _end; q += _config.insert_step) {
                        _insert_fast(q);
                    }
                }
                _pos = last;
                _full(out);
            }
        }

        // A block written out when the symbols fill it
        template<class Out>
        SGCL_INLINE_HOT void _full(Out& out) noexcept {
            if (_lit.size() >= MaxSymbols) {
                auto w = _writer(out);
                _flush_block(false, w);
                _save(w);
            }
        }

        // Level 0: the input framed as stored blocks of 65535 bytes, a
        // whole block straight from the input, only a block's remainder
        // kept in the window until the next write, flush or finish
        template<class Out>
        void _write_stored(const uint8_t* data, size_t n, Out& out) noexcept {
            auto w = _writer(out);
            while (n) {
                if (_end == 0 && n >= StoredMax) {
                    _stored(false, w, data, StoredMax);
                    data += StoredMax;
                    n -= StoredMax;
                    continue;
                }
                size_t take = std::min<size_t>(StoredMax - _end, n);
                sgcl::detail::copy_bytes(_window.data() + _end, data, take);
                _end += uint32_t(take);
                data += take;
                n -= take;
                if (_end == StoredMax) {
                    _stored(false, w, _window.data(), _end);
                    _end = 0;
                }
            }
            _save(w);
        }

        template<class Out>
        SGCL_INLINE_HOT BitWriter<Out> _writer(Out& out) noexcept {
            BitWriter<Out> w(out);
            w.restore(_bits_value, _bits_count);
            return w;
        }

        template<class Out>
        SGCL_INLINE_HOT void _save(const BitWriter<Out>& w) noexcept {
            _bits_value = w.pending_value();
            _bits_count = w.pending_bits();
        }

        // The block of the symbols gathered (or, at level 0, of the bytes
        // since the block's start), in its shortest form
        template<class Out>
        void _flush_block(bool last, BitWriter<Out>& w) noexcept {
            // the bytes the block covers: the symbols end where _pos is,
            // less a literal the lazy search still holds back
            uint32_t end = _pos - (_match_available ? 1 : 0);
            uint64_t raw = uint64_t(int64_t(end) - _block_start);
            if (_lit.empty() && !last) {
                return;
            }
            _lit_freq[256] = 1;
            uint8_t lit_len[286], dist_len[30];
            huffman_lengths(_lit_freq, 286, 15, lit_len);
            huffman_lengths(_dist_freq, 30, 15, dist_len);
            if (std::all_of(dist_len, dist_len + 30, [](uint8_t l) { return l == 0; })) {
                dist_len[0] = 1;
                dist_len[1] = 1;
            }
            unsigned hlit = 286;
            while (hlit > 257 && lit_len[hlit - 1] == 0) {
                --hlit;
            }
            unsigned hdist = 30;
            while (hdist > 1 && dist_len[hdist - 1] == 0) {
                --hdist;
            }
            // the code lengths of both tables, run-length coded
            uint8_t all[316];
            sgcl::detail::copy_bytes(all, lit_len, hlit);
            sgcl::detail::copy_bytes(all + hlit, dist_len, hdist);
            uint16_t rle[316];
            uint8_t rle_extra[316];
            unsigned rle_n = 0;
            uint32_t cl_freq[19] = {};
            for (unsigned i = 0; i < hlit + hdist;) {
                uint8_t v = all[i];
                unsigned run = 1;
                while (i + run < hlit + hdist && all[i + run] == v) {
                    ++run;
                }
                unsigned left = run;
                if (v == 0) {
                    while (left >= 11) {
                        unsigned r = std::min(left, 138u);
                        rle[rle_n] = 18;
                        rle_extra[rle_n++] = uint8_t(r - 11);
                        ++cl_freq[18];
                        left -= r;
                    }
                    if (left >= 3) {
                        rle[rle_n] = 17;
                        rle_extra[rle_n++] = uint8_t(left - 3);
                        ++cl_freq[17];
                        left = 0;
                    }
                } else {
                    rle[rle_n] = v;
                    rle_extra[rle_n++] = 0;
                    ++cl_freq[v];
                    --left;
                    while (left >= 3) {
                        unsigned r = std::min(left, 6u);
                        rle[rle_n] = 16;
                        rle_extra[rle_n++] = uint8_t(r - 3);
                        ++cl_freq[16];
                        left -= r;
                    }
                }
                while (left) {
                    rle[rle_n] = v;
                    rle_extra[rle_n++] = 0;
                    ++cl_freq[v];
                    --left;
                }
                i += run;
            }
            uint8_t cl_len[19];
            huffman_lengths(cl_freq, 19, 7, cl_len);
            unsigned hclen = 19;
            while (hclen > 4 && cl_len[CodeLengthOrder[hclen - 1]] == 0) {
                --hclen;
            }
            // the sizes of the three forms, in bits
            uint64_t dynamic_bits = 3 + 14 + uint64_t(hclen) * 3;
            for (unsigned i = 0; i < rle_n; ++i) {
                dynamic_bits += cl_len[rle[i]] + (rle[i] == 16 ? 2 : rle[i] == 17 ? 3 : rle[i] == 18 ? 7 : 0);
            }
            uint64_t fixed_bits = 3;
            uint64_t extra_bits = 0;
            for (unsigned s = 0; s < 286; ++s) {
                if (!_lit_freq[s]) {
                    continue;
                }
                uint32_t fl = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
                dynamic_bits += uint64_t(_lit_freq[s]) * lit_len[s];
                fixed_bits += uint64_t(_lit_freq[s]) * fl;
                if (s >= 257) {
                    extra_bits += uint64_t(_lit_freq[s]) * LengthExtra[s - 257];
                }
            }
            for (unsigned s = 0; s < 30; ++s) {
                if (!_dist_freq[s]) {
                    continue;
                }
                dynamic_bits += uint64_t(_dist_freq[s]) * dist_len[s];
                fixed_bits += uint64_t(_dist_freq[s]) * 5;
                extra_bits += uint64_t(_dist_freq[s]) * DistanceExtra[s];
            }
            dynamic_bits += extra_bits;
            fixed_bits += extra_bits;
            // stored: only while the bytes the block covers are all in the window
            uint64_t stored_bits = raw && _block_start >= 0 ? raw * 8 + ((raw + 65534) / 65535) * 40 + 8 : UINT64_MAX;
            if (stored_bits < dynamic_bits && stored_bits < fixed_bits) {
                _stored(last, w, _window.data() + _block_start, uint32_t(raw));
            } else if (fixed_bits <= dynamic_bits) {
                w.put(last ? 1 : 0, 1);
                w.put(1, 2);
                uint8_t fl[288], fd[30];
                for (unsigned s = 0; s < 288; ++s) {
                    fl[s] = s < 144 ? 8 : s < 256 ? 9 : s < 280 ? 7 : 8;
                }
                std::fill(fd, fd + 30, uint8_t(5));
                _symbols(w, fl, 288, fd, 30);
            } else {
                w.put(last ? 1 : 0, 1);
                w.put(2, 2);
                w.put(hlit - 257, 5);
                w.put(hdist - 1, 5);
                w.put(hclen - 4, 4);
                for (unsigned i = 0; i < hclen; ++i) {
                    w.put(cl_len[CodeLengthOrder[i]], 3);
                }
                uint16_t cl_code[19];
                canonical_codes(cl_len, 19, cl_code);
                for (unsigned i = 0; i < rle_n; ++i) {
                    w.put(cl_code[rle[i]], cl_len[rle[i]]);
                    if (rle[i] >= 16) {
                        w.put(rle_extra[i], rle[i] == 16 ? 2 : rle[i] == 17 ? 3 : 7);
                    }
                }
                _symbols(w, lit_len, 286, dist_len, 30);
            }
            _lit.clear();
            _dist.clear();
            std::fill(std::begin(_lit_freq), std::end(_lit_freq), 0u);
            std::fill(std::begin(_dist_freq), std::end(_dist_freq), 0u);
            _block_start = end;
        }

        template<class Out>
        void _symbols(BitWriter<Out>& w, const uint8_t* lit_len, unsigned lit_count, const uint8_t* dist_len, unsigned dist_count) noexcept {
            uint16_t lit_code[288], dist_code[30];
            canonical_codes(lit_len, lit_count, lit_code);
            canonical_codes(dist_len, dist_count, dist_code);
            for (size_t i = 0; i < _lit.size(); ++i) {
                uint32_t v = _lit[i];
                if (v < 256) {
                    w.put(lit_code[v], lit_len[v]);
                    continue;
                }
                uint32_t length = v - 253;
                uint32_t lc = length_code(length);
                w.put(lit_code[257 + lc], lit_len[257 + lc]);
                if (LengthExtra[lc]) {
                    w.put(length - LengthBase[lc], LengthExtra[lc]);
                }
                uint32_t distance = _dist[i];
                uint32_t dc = distance_code(distance);
                w.put(dist_code[dc], dist_len[dc]);
                if (DistanceExtra[dc]) {
                    w.put(distance - DistanceBase[dc], DistanceExtra[dc]);
                }
            }
            w.put(lit_code[256], lit_len[256]);
        }

        // Stored blocks of at most 65535 bytes over the n bytes at p (of the
        // window, or of the input at level 0); the last one marked last
        // when asked (one empty block when n is 0)
        template<class Out>
        void _stored(bool last, BitWriter<Out>& w, const uint8_t* p, uint32_t n) noexcept {
            do {
                uint32_t len = std::min<uint32_t>(n, 65535);
                n -= len;
                w.put(last && n == 0 ? 1 : 0, 1);
                w.put(0, 2);
                w.align();
                uint8_t header[4] = {uint8_t(len), uint8_t(len >> 8), uint8_t(~len), uint8_t(~len >> 8)};
                append_bytes(w.out(), header, 4);
                append_bytes(w.out(), p, len);
                p += len;
            } while (n);
        }

        int _level;
        bool _filtered = false;
        LevelConfig _config;
        std::vector<uint8_t> _window;     // the history (32 KB) and the input being matched (64 KB); level 0: a stored block's remainder
        std::vector<uint32_t> _head;      // the latest absolute position (+1) under each hash (chains: 3 bytes; the table encoder: 4) (0: none)
        std::vector<uint32_t> _prev;      // chains: the position before it (+1) under the same hash, by position mod 32 KB
        std::vector<uint32_t> _long;      // the table encoder from level 3: the latest position (+1) under each hash of 7 bytes
        uint32_t _offset = 0;             // the absolute position of the window's first byte
        uint32_t _misses = 0;             // the table encoder's positions without a match in a row
        uint32_t _end = 0;                // the bytes in the window
        uint32_t _pos = 0;                // the next byte to match
        int64_t _block_start = 0;         // the first byte of the current block; below 0 once it slid out of the window
        std::vector<uint16_t> _lit;       // a literal byte, or 253 + length for a match (256..511)
        std::vector<uint16_t> _dist;      // a match's distance
        uint32_t _lit_freq[286] = {};
        uint32_t _dist_freq[30] = {};
        bool _match_available = false;    // the lazy search holds the byte before _pos
        uint32_t _prev_length = MinMatch - 1;
        uint32_t _prev_distance = 0;
        uint64_t _bits_value = 0;         // the bits of the last block not yet a whole byte
        uint32_t _bits_count = 0;
    };
}
