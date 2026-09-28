//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "inflate.h"

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
    // The encoder of DEFLATE (RFC 1951): LZ77 over a window of 32 KB with
    // chains of positions under a hash of three bytes, greedy matching at
    // the fast levels and lazy matching (a match taken only when the next
    // position has no longer one) from level 4; the symbols of a block
    // gathered with their frequencies, and the block written with the
    // shortest of its three forms — its own Huffman codes, the fixed
    // codes, or stored. Plain memory: the window, the chains and the
    // symbols are std::vectors of the encoder.

    // Appends bits least significant first, as the format packs them
    class BitWriter {
    public:
        explicit BitWriter(std::vector<uint8_t>& out) noexcept
        : _out(out) {
        }

        void put(uint32_t value, uint32_t count) {
            _bits |= uint64_t(value) << _count;
            _count += count;
            if (_count >= 32) {
                uint8_t b[4] = {uint8_t(_bits), uint8_t(_bits >> 8), uint8_t(_bits >> 16), uint8_t(_bits >> 24)};
                _out.insert(_out.end(), b, b + 4);
                _bits >>= 32;
                _count -= 32;
            }
        }

        // To the byte boundary with zero bits
        void align() {
            while (_count > 0) {
                _out.push_back(uint8_t(_bits));
                _bits >>= 8;
                _count = _count > 8 ? _count - 8 : 0;
            }
            _bits = 0;
        }

        std::vector<uint8_t>& out() noexcept {
            return _out;
        }

        uint32_t pending_bits() const noexcept {
            return _count;
        }

        uint64_t pending_value() const noexcept {
            return _bits;
        }

        void restore(uint64_t bits, uint32_t count) noexcept {
            _bits = bits;
            _count = count;
        }

    private:
        std::vector<uint8_t>& _out;
        uint64_t _bits = 0;
        uint32_t _count = 0;
    };

    // Code lengths of at most max_bits for these frequencies: a Huffman
    // code (the two least frequent merged, again and again), its lengths
    // clamped to max_bits and the Kraft sum brought back to one by
    // lengthening the shortest codes that can take it; symbols of
    // frequency 0 get no code. At least two codes are made when a table
    // has any, so that every code the encoder writes is complete.
    inline void huffman_lengths(const uint32_t* freq, unsigned count, unsigned max_bits, uint8_t* lengths) {
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
        std::sort(leaves, leaves + n, [](const Leaf& a, const Leaf& b) {
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
        auto smallest = [&]() {
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

    // The tuning of a level: matches this long are good enough to stop
    // searching as hard, the lazy search stops above max_lazy, a match of
    // nice length ends the search, and at most max_chain positions are
    // tried. Levels 1 to 3 match greedily.
    struct LevelConfig {
        uint16_t good, max_lazy, nice, max_chain;
        bool lazy;
    };

    inline constexpr LevelConfig level_config(int level) noexcept {
        switch (level) {
            case 1: return {4, 4, 8, 4, false};
            case 2: return {4, 5, 16, 8, false};
            case 3: return {4, 6, 32, 32, false};
            case 4: return {4, 4, 16, 16, true};
            case 5: return {8, 16, 32, 32, true};
            case 7: return {8, 32, 128, 256, true};
            case 8: return {32, 128, 258, 1024, true};
            case 9: return {32, 258, 258, 4096, true};
            default: return {8, 16, 128, 128, true};
        }
    }

    class Deflater {
    public:
        static constexpr uint32_t HashBits = 15;
        static constexpr uint32_t HashSize = 1u << HashBits;
        static constexpr uint32_t WindowMask = WindowSize - 1;
        static constexpr uint32_t MinMatch = 3;
        static constexpr uint32_t MinLookahead = MaxMatch + MinMatch + 1;
        static constexpr uint32_t BufferSize = 2 * WindowSize;
        static constexpr uint32_t MaxSymbols = 16384;
        static constexpr int HuffmanOnly = -2;

        // level: 0 stores, 1..9, HuffmanOnly; the dictionary's last 32 KB
        // are the history the first matches may reach
        explicit Deflater(int level, const uint8_t* dictionary = nullptr, size_t dictionary_size = 0)
        : _level(level)
        , _config(level_config(level))
        , _window(BufferSize + 8)
        , _head(HashSize, 0)
        , _prev(WindowSize, 0) {
            _lit.reserve(MaxSymbols + 1);
            _dist.reserve(MaxSymbols + 1);
            _seed(dictionary, dictionary_size);
        }

        // Back to the start with the same memory, and a dictionary again
        void reset(const uint8_t* dictionary, size_t dictionary_size) {
            reset();
            _seed(dictionary, dictionary_size);
        }

        // The dictionary's last 32 KB as the history, in the chains
        void _seed(const uint8_t* dictionary, size_t dictionary_size) {
            if (dictionary_size) {
                if (dictionary_size > WindowSize) {
                    dictionary += dictionary_size - WindowSize;
                    dictionary_size = WindowSize;
                }
                std::memcpy(_window.data(), dictionary, dictionary_size);
                _end = uint32_t(dictionary_size);
                _pos = _end;
                _block_start = _end;
                if (_level > 0) {
                    for (uint32_t p = 0; p + MinMatch <= _end; ++p) {
                        _insert(p);
                    }
                }
            }
        }

        // Back to the start: the same level, no history, nothing pending
        void reset() {
            std::fill(_head.begin(), _head.end(), 0u);
            std::fill(_prev.begin(), _prev.end(), 0u);
            _end = _pos = _block_start = 0;
            _lit.clear();
            _dist.clear();
            std::fill(std::begin(_lit_freq), std::end(_lit_freq), 0u);
            std::fill(std::begin(_dist_freq), std::end(_dist_freq), 0u);
            _match_available = false;
            _prev_length = MinMatch - 1;
            _bits_value = 0;
            _bits_count = 0;
        }

        // Compresses what the window holds of data and appends to out the
        // blocks it completes; the rest waits for more input, flush or finish
        void write(const uint8_t* data, size_t n, std::vector<uint8_t>& out) {
            while (n) {
                if (_end == BufferSize) {
                    // at level 0 the block is the bytes: written while they
                    // are in the window. A coded block goes on across the
                    // move (its start below the window), and may then no
                    // longer be stored, as in zlib
                    if (_level == 0 && _block_start < int64_t(WindowSize)) {
                        BitWriter w = _writer(out);
                        _flush_block(false, w);
                        _save(w);
                    }
                    _slide();
                }
                size_t room = BufferSize - _end;
                size_t take = std::min(room, n);
                std::memcpy(_window.data() + _end, data, take);
                _end += uint32_t(take);
                data += take;
                n -= take;
                _compress(false, out);
            }
        }

        // Everything written so far out, and the output on a byte
        // boundary: a sync flush (an empty stored block after the data)
        void flush(std::vector<uint8_t>& out) {
            _compress(true, out);
            BitWriter w = _writer(out);
            _flush_block(false, w);
            // the empty stored block
            w.put(0, 3);
            w.align();
            const uint8_t marker[4] = {0, 0, 0xFF, 0xFF};
            out.insert(out.end(), marker, marker + 4);
            _save(w);
        }

        // The last block: everything out, the stream ended and aligned
        void finish(std::vector<uint8_t>& out) {
            _compress(true, out);
            BitWriter w = _writer(out);
            _flush_block(true, w);
            w.align();
            _save(w);
        }

    private:
        uint32_t _hash(uint32_t p) const noexcept {
            const uint8_t* s = _window.data() + p;
            uint32_t v = uint32_t(s[0]) | uint32_t(s[1]) << 8 | uint32_t(s[2]) << 16;
            return (v * 0x9E3779B1u) >> (32 - HashBits);
        }

        // position p under its hash; the head and the chain keep p + 1, 0 is none
        uint32_t _insert(uint32_t p) noexcept {
            uint32_t h = _hash(p);
            uint32_t prev = _head[h];
            _prev[p & WindowMask] = prev;
            _head[h] = p + 1;
            return prev;
        }

        // The longest match at p among the chain starting at candidate
        // (p + 1 encoded), longer than best; its distance in *distance. A
        // candidate is turned away by two loads when it cannot win: its first
        // three bytes (the hash's collisions) and the two bytes where the best
        // match so far ends.
        uint32_t _longest_match(uint32_t p, uint32_t candidate, uint32_t best, uint32_t& distance) const noexcept {
            uint32_t chain = _config.max_chain;
            if (best >= _config.good) {
                chain >>= 2;
            }
            uint32_t limit = p > WindowSize ? p - WindowSize : 0;
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
            auto load32 = [](const uint8_t* q) noexcept {
                uint32_t v;
                std::memcpy(&v, q, 4);
                return v;
            };
            // the window has 8 bytes of slack past its end, so the word at the start reads in bounds
            const uint32_t start = load32(cur) & 0xFFFFFF;
            uint16_t tail = best >= 1 ? load16(cur + best - 1) : 0;
            while (candidate > limit && chain--) {
                uint32_t c = candidate - 1;
                const uint8_t* m = w + c;
                candidate = _prev[c & WindowMask];
                // a slot overwritten by a newer position (32 KB on) would lead
                // the chain round again: it goes only to older positions
                if (candidate > c) {
                    candidate = 0;
                }
                if ((best >= 1 && load16(m + best - 1) != tail) || (load32(m) & 0xFFFFFF) != start) {
                    continue;
                }
                uint32_t len = MinMatch;
#if defined(__ARM_NEON)
                // sixteen bytes a step: the bytes equal as a mask of four bits
                // each (vshrn), the first that differs its trailing zeros / 4
                while (len + 16 <= max_len) {
                    uint8x16_t eq = vceqq_u8(vld1q_u8(cur + len), vld1q_u8(m + len));
                    uint64_t mask = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(eq), 4)), 0);
                    if (mask != ~uint64_t(0)) {
                        len += uint32_t(std::countr_zero(~mask)) >> 2;
                        goto measured;
                    }
                    len += 16;
                }
#endif
                while (len + 8 <= max_len) {
                    uint64_t a, b;
                    std::memcpy(&a, cur + len, 8);
                    std::memcpy(&b, m + len, 8);
                    uint64_t x = a ^ b;
                    if (x) {
                        len += uint32_t(std::countr_zero(x)) >> 3;
                        goto measured;
                    }
                    len += 8;
                }
                while (len < max_len && m[len] == cur[len]) {
                    ++len;
                }
            measured:
                len = std::min(len, max_len);
                if (len > best) {
                    best = len;
                    distance = p - c;
                    if (len >= nice) {
                        break;
                    }
                    tail = load16(cur + best - 1);
                }
            }
            return best;
        }

        void _literal(uint8_t b) {
            _lit.push_back(b);
            _dist.push_back(0);
            ++_lit_freq[b];
        }

        void _match(uint32_t length, uint32_t distance) {
            uint32_t lc = length_code(length);
            _lit.push_back(uint16_t(253 + length));   // 256.. for 3..258: a value of 256 or more is a match
            _dist.push_back(uint16_t(distance));
            ++_lit_freq[257 + lc];
            ++_dist_freq[distance_code(distance)];
        }

        // Moves the second half of the window to the first
        void _slide() {
            std::memmove(_window.data(), _window.data() + WindowSize, WindowSize);
            for (auto& h : _head) {
                h = h > WindowSize ? h - WindowSize : 0;
            }
            for (auto& h : _prev) {
                h = h > WindowSize ? h - WindowSize : 0;
            }
            _end -= WindowSize;
            _pos -= WindowSize;
            _block_start -= WindowSize;
        }

        // Matches over the window up to MinLookahead from its end (or to
        // its end when there is no more input), writing out the blocks
        // that fill up
        void _compress(bool final, std::vector<uint8_t>& out) {
            uint32_t stop = final ? _end : (_end > MinLookahead ? _end - MinLookahead : 0);
            if (_level == 0) {
                // stored: the block is the bytes, written in pieces of 64 KB
                _pos = std::max(_pos, stop);
                if (int64_t(_pos) - _block_start >= 65535) {
                    BitWriter w = _writer(out);
                    _flush_block(false, w);
                    _save(w);
                }
                return;
            }
            if (_level == HuffmanOnly) {
                while (_pos < stop) {
                    _literal(_window[_pos++]);
                    if (_lit.size() >= MaxSymbols) {
                        BitWriter w = _writer(out);
                        _flush_block(false, w);
                        _save(w);
                    }
                }
                return;
            }
            while (_pos < stop) {
                uint32_t candidate = _pos + MinMatch <= _end ? _insert(_pos) : 0;
                if (!_config.lazy) {
                    uint32_t distance = 0;
                    uint32_t len = candidate ? _longest_match(_pos, candidate, MinMatch - 1, distance) : 0;
                    if (len >= MinMatch) {
                        _match(len, distance);
                        // the positions inside a short match go into the chains too
                        uint32_t last = _pos + len;
                        if (len <= _config.max_lazy) {
                            for (uint32_t q = _pos + 1; q < last && q + MinMatch <= _end; ++q) {
                                _insert(q);
                            }
                        }
                        _pos = last;
                    } else {
                        _literal(_window[_pos++]);
                    }
                } else {
                    uint32_t distance = 0;
                    uint32_t len = MinMatch - 1;
                    if (candidate && _prev_length < _config.max_lazy) {
                        len = _longest_match(_pos, candidate, _prev_length, distance);
                        // a match of three bytes far away costs more than three literals
                        if (len == MinMatch && distance > 4096) {
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
                }
                if (_lit.size() >= MaxSymbols) {
                    BitWriter w = _writer(out);
                    _flush_block(false, w);
                    _save(w);
                }
            }
            if (final && _match_available) {
                _literal(_window[_pos - 1]);
                _match_available = false;
                _prev_length = MinMatch - 1;
            }
        }

        BitWriter _writer(std::vector<uint8_t>& out) {
            BitWriter w(out);
            w.restore(_bits_value, _bits_count);
            return w;
        }

        void _save(const BitWriter& w) noexcept {
            _bits_value = w.pending_value();
            _bits_count = w.pending_bits();
        }

        // The block of the symbols gathered (or, at level 0, of the bytes
        // since the block's start), in its shortest form
        void _flush_block(bool last, BitWriter& w) {
            // the bytes the block covers: the symbols end where _pos is,
            // less a literal the lazy search still holds back
            uint32_t end = _pos - (_match_available ? 1 : 0);
            uint64_t raw = uint64_t(int64_t(end) - _block_start);
            if (_level == 0) {
                if (raw || last) {
                    _stored(last, w, uint32_t(_block_start), uint32_t(raw));
                }
                _block_start = end;
                return;
            }
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
            std::memcpy(all, lit_len, hlit);
            std::memcpy(all + hlit, dist_len, hdist);
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
                _stored(last, w, uint32_t(_block_start), uint32_t(raw));
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

        void _symbols(BitWriter& w, const uint8_t* lit_len, unsigned lit_count, const uint8_t* dist_len, unsigned dist_count) {
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

        // Stored blocks of at most 65535 bytes over [start, start + n) of
        // the window; the last one marked last when asked (one empty block
        // when n is 0)
        void _stored(bool last, BitWriter& w, uint32_t start, uint32_t n) {
            do {
                uint32_t len = std::min<uint32_t>(n, 65535);
                n -= len;
                w.put(last && n == 0 ? 1 : 0, 1);
                w.put(0, 2);
                w.align();
                uint8_t header[4] = {uint8_t(len), uint8_t(len >> 8), uint8_t(~len), uint8_t(~len >> 8)};
                auto& out = w.out();
                out.insert(out.end(), header, header + 4);
                out.insert(out.end(), _window.data() + start, _window.data() + start + len);
                start += len;
            } while (n);
        }

        int _level;
        LevelConfig _config;
        std::vector<uint8_t> _window;     // two windows: the history and what is being matched
        std::vector<uint32_t> _head;      // the latest position (+1) under each hash
        std::vector<uint32_t> _prev;      // the position before it (+1) under the same hash, by position mod 32 KB
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
