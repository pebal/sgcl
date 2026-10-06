//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "brotli_decode.h"
#include "zstd_match.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Brotli's encoder (RFC 7932), written from the RFC.
//
// The matches come from the match finders of zstd's levels (zstd_match.h:
// the fast table, the double table, the tagged rows with a lazy parse, the
// binary tree with the optimal parser), the window 16 bytes short of a power
// of two as brotli's is; the qualities map onto them. Each meta-block takes
// up to 2^lgblock bytes of input: its commands (literals inserted, a copy
// and its distance — one of the four last when it is, coded with the
// RFC's offsets otherwise), the literals' contexts and their map, then the
// prefix codes of the counts, length-limited to 15 bits and written simple
// (up to four symbols) or complex (the code lengths run-length coded under
// a code of their own). A meta-block that would not be smaller than its
// bytes goes stored.
namespace sgcl::compress::detail {
    // ---- the bit writer -----------------------------------------------------

    // Bits least significant first into a buffer of bytes: whole bytes in
    // buf[0, used), the bits of a partial byte waiting in acc; room made by
    // reserve() before a stretch of writes (eight bytes of slack beyond it)
    struct BrotliBitWriter {
        std::vector<uint8_t> buf;
        size_t used = 0;
        uint64_t acc = 0;
        unsigned bits = 0;

        void reserve(size_t n) noexcept {
            if (buf.size() < used + n + 16) {
                buf.resize(std::max(used + n + 16, buf.size() * 2));
            }
        }

        // n <= 32 bits of v (acc holds 31 at most between calls)
        SGCL_INLINE_HOT void add(uint64_t v, unsigned n) noexcept {
            acc |= v << bits;
            bits += n;
            if (bits >= 32) {
                uint64_t word = acc;
                if constexpr (std::endian::native == std::endian::big) {
                    word = __builtin_bswap64(word);
                }
                std::memcpy(buf.data() + used, &word, 8);
                const unsigned whole = bits >> 3;
                used += whole;
                acc >>= 8 * whole;
                bits &= 7;
            }
        }

        // The whole bytes of acc out
        void settle() noexcept {
            reserve(8);
            while (bits >= 8) {
                buf[used++] = uint8_t(acc);
                acc >>= 8;
                bits -= 8;
            }
        }

        // To a byte boundary with zero bits
        void align() noexcept {
            add(0, (8 - bits % 8) % 8);
            settle();
        }

        // Whole bytes now
        void bytes(const uint8_t* p, size_t n) noexcept {
            align();
            reserve(n);
            sgcl::detail::copy_bytes(buf.data() + used, p, n);
            used += n;
        }

        size_t size_bits() const noexcept {
            return used * 8 + bits;
        }

        // The whole bytes made, taken by the caller
        template<class Out>
        void drain(Out& out) noexcept {
            settle();
            if (used) {
                append_bytes(out, buf.data(), used);
                used = 0;
            }
        }

        struct Mark {
            size_t used;
            uint64_t acc;
            unsigned bits;
        };

        Mark mark() const noexcept {
            return {used, acc, bits};
        }

        void restore(const Mark& m) noexcept {
            used = m.used;
            acc = m.acc;
            bits = m.bits;
        }
    };

    // ---- codes ---------------------------------------------------------------

    // The insert length's code and the copy length's
    SGCL_INLINE_HOT unsigned brotli_insert_code(uint32_t n) noexcept {
        if (n < 6) {
            return n;
        }
        if (n < 130) {
            const unsigned nbits = unsigned(31 - std::countl_zero(n - 2)) - 1;
            return (nbits << 1) + unsigned((n - 2) >> nbits) + 2;
        }
        if (n < 2114) {
            return unsigned(31 - std::countl_zero(n - 66)) + 10;
        }
        if (n < 6210) {
            return 21;
        }
        if (n < 22594) {
            return 22;
        }
        return 23;
    }

    SGCL_INLINE_HOT unsigned brotli_copy_code(uint32_t n) noexcept {
        if (n < 10) {
            return n - 2;
        }
        if (n < 134) {
            const unsigned nbits = unsigned(31 - std::countl_zero(n - 6)) - 1;
            return (nbits << 1) + unsigned((n - 6) >> nbits) + 4;
        }
        if (n < 2118) {
            return unsigned(31 - std::countl_zero(n - 70)) + 12;
        }
        return 23;
    }

    // The command code of an insert code, a copy code, and whether the
    // distance is the last one taken without a code
    SGCL_INLINE_HOT unsigned brotli_command_code(unsigned ic, unsigned cc, bool implicit) noexcept {
        const unsigned low = ((ic & 7) << 3) | (cc & 7);
        if (implicit && ic < 8 && cc < 16) {
            return (cc < 8 ? 0u : 64u) + low;
        }
        static constexpr uint8_t cell[3][3] = {{2, 3, 6}, {4, 5, 8}, {7, 9, 10}};
        return 64u * cell[ic >> 3][cc >> 3] + low;
    }

    // A code's canonical codes, reversed for the writer (the first bit
    // read is the code's most significant)
    inline void brotli_codes(const uint8_t* lengths, unsigned alphabet, uint16_t* codes) noexcept {
        unsigned counts[16] = {};
        for (unsigned s = 0; s < alphabet; ++s) {
            ++counts[lengths[s]];
        }
        counts[0] = 0;
        unsigned next[16];
        unsigned code = 0;
        for (unsigned l = 1; l < 16; ++l) {
            code = (code + counts[l - 1]) << 1;
            next[l] = code;
        }
        for (unsigned s = 0; s < alphabet; ++s) {
            const unsigned l = lengths[s];
            codes[s] = l ? uint16_t(brotli_reverse(next[l]++, l)) : 0;
        }
    }

    // A prefix code for counts over an alphabet: its lengths (15 at most)
    // and codes. used: how many symbols occur (0 or 1 gives one of no bits)
    struct BrotliCode {
        uint8_t lengths[704];
        uint16_t codes[704];
        unsigned alphabet = 0;
        unsigned used = 0;
        unsigned single = 0;

        void build(const uint32_t* counts, unsigned n) noexcept {
            alphabet = n;
            used = 0;
            for (unsigned s = 0; s < n; ++s) {
                if (counts[s]) {
                    ++used;
                    single = s;
                }
            }
            if (used <= 1) {
                std::fill(lengths, lengths + n, uint8_t(0));
                std::fill(codes, codes + n, uint16_t(0));
                return;
            }
            huf_lengths_n<704>(counts, n, 15, lengths);
            brotli_codes(lengths, n, codes);
        }

        SGCL_INLINE_HOT void put(BrotliBitWriter& w, unsigned s) const noexcept {
            w.add(codes[s], lengths[s]);
        }

        // The bits the counts cost under the code
        uint64_t cost(const uint32_t* counts) const noexcept {
            uint64_t bits = 0;
            for (unsigned s = 0; s < alphabet; ++s) {
                bits += uint64_t(counts[s]) * lengths[s];
            }
            return bits;
        }
    };

    // The code written (RFC 7932, 3.4 and 3.5): simple for up to four
    // symbols, else the code lengths run-length coded under a code-length
    // code of up to 5 bits
    inline void brotli_write_code(BrotliBitWriter& w, BrotliCode& c) noexcept {
        unsigned bits = 0;
        while ((1u << bits) < c.alphabet) {
            ++bits;
        }
        if (c.used <= 4) {
            unsigned sym[4];
            unsigned k = 0;
            if (c.used <= 1) {
                sym[k++] = c.single;
            } else {
                for (unsigned s = 0; s < c.alphabet; ++s) {
                    if (c.lengths[s]) {
                        sym[k++] = s;
                    }
                }
                // shortest first; the ones of a length in order (the decoder's
                // canonical codes are by symbol within a length)
                std::stable_sort(sym, sym + k, [&](unsigned a, unsigned b) { return c.lengths[a] < c.lengths[b]; });
            }
            // the lengths a simple code gives, and the codes for them
            if (k == 4 && c.lengths[sym[0]] == 1) {
                c.lengths[sym[0]] = 1;
                c.lengths[sym[1]] = 2;
                c.lengths[sym[2]] = 3;
                c.lengths[sym[3]] = 3;
            } else if (k == 4) {
                for (unsigned i = 0; i < 4; ++i) {
                    c.lengths[sym[i]] = 2;
                }
            } else if (k == 3) {
                c.lengths[sym[0]] = 1;
                c.lengths[sym[1]] = 2;
                c.lengths[sym[2]] = 2;
            } else if (k == 2) {
                c.lengths[sym[0]] = 1;
                c.lengths[sym[1]] = 1;
            } else {
                c.lengths[sym[0]] = 0;
            }
            if (k >= 2) {
                brotli_codes(c.lengths, c.alphabet, c.codes);
            }
            w.add(1, 2);
            w.add(k - 1, 2);
            for (unsigned i = 0; i < k; ++i) {
                w.add(sym[i], bits);
            }
            if (k == 4) {
                w.add(c.lengths[sym[0]] == 1 ? 1 : 0, 1);
            }
            return;
        }
        // the lengths as code-length symbols: 0..15 as they are, 16 a run of
        // the last non-zero length, 17 a run of zeros (each with its extra bits)
        unsigned n = c.alphabet;
        while (n > 0 && c.lengths[n - 1] == 0) {
            --n;
        }
        std::vector<uint8_t> syms;
        std::vector<uint8_t> extra;
        syms.reserve(n);
        extra.reserve(n);
        unsigned prev = 8;
        auto runs = [&](unsigned value, unsigned count) {
            if (value == 0) {
                if (count < 3) {
                    for (unsigned i = 0; i < count; ++i) {
                        syms.push_back(0);
                        extra.push_back(0);
                    }
                    return;
                }
                // chained 17s: count - 3 = 8 * (1 + the count before) + extra
                uint8_t s[16], e[16];
                unsigned k = 0;
                uint32_t r = count - 3;
                for (;;) {
                    s[k] = 17;
                    e[k++] = uint8_t(r & 7);
                    r >>= 3;
                    if (r == 0) {
                        break;
                    }
                    --r;
                }
                while (k) {
                    --k;
                    syms.push_back(s[k]);
                    extra.push_back(e[k]);
                }
                return;
            }
            if (value != prev) {
                syms.push_back(uint8_t(value));
                extra.push_back(0);
                prev = value;
                --count;
            }
            if (count < 3) {
                for (unsigned i = 0; i < count; ++i) {
                    syms.push_back(uint8_t(value));
                    extra.push_back(0);
                }
                return;
            }
            uint8_t s[16], e[16];
            unsigned k = 0;
            uint32_t r = count - 3;
            for (;;) {
                s[k] = 16;
                e[k++] = uint8_t(r & 3);
                r >>= 2;
                if (r == 0) {
                    break;
                }
                --r;
            }
            while (k) {
                --k;
                syms.push_back(s[k]);
                extra.push_back(e[k]);
            }
        };
        for (unsigned i = 0; i < n;) {
            unsigned j = i + 1;
            while (j < n && c.lengths[j] == c.lengths[i]) {
                ++j;
            }
            runs(c.lengths[i], j - i);
            i = j;
        }
        // the code-length code
        uint32_t counts[18] = {};
        for (auto s : syms) {
            ++counts[s];
        }
        uint8_t cl[18];
        unsigned cl_used = 0;
        unsigned cl_single = 0;
        for (unsigned s = 0; s < 18; ++s) {
            if (counts[s]) {
                ++cl_used;
                cl_single = s;
            }
        }
        if (cl_used == 1) {
            std::fill(cl, cl + 18, uint8_t(0));
            cl[cl_single] = 1;   // written as the one length; read as a symbol of no bits
        } else {
            huf_lengths_n<18>(counts, 18, 5, cl);
        }
        uint16_t cl_codes[18];
        brotli_codes(cl, 18, cl_codes);
        // HSKIP: the leading code lengths of the order that are zero (0, 2 or 3)
        unsigned hskip = 0;
        if (cl[BrotliCodeLengthOrder[0]] == 0 && cl[BrotliCodeLengthOrder[1]] == 0) {
            hskip = cl[BrotliCodeLengthOrder[2]] == 0 ? 3 : 2;
        }
        unsigned last = 17;
        while (last > 0 && cl[BrotliCodeLengthOrder[last]] == 0) {
            --last;
        }
        w.add(hskip, 2);
        // the fixed code of the code-length code lengths (by length value:
        // the bits as read, and their count)
        static constexpr uint8_t fixed_code[6] = {0, 7, 3, 2, 1, 15};
        static constexpr uint8_t fixed_len[6] = {2, 4, 3, 2, 2, 4};
        for (unsigned i = hskip; i <= last; ++i) {
            const unsigned l = cl[BrotliCodeLengthOrder[i]];
            w.add(fixed_code[l], fixed_len[l]);
        }
        if (cl_used == 1) {
            cl[cl_single] = 0;   // a code of one symbol: no bits for it
        }
        for (size_t i = 0; i < syms.size(); ++i) {
            const unsigned s = syms[i];
            w.add(cl_codes[s], cl[s]);
            if (s == 16) {
                w.add(extra[i], 2);
            } else if (s == 17) {
                w.add(extra[i], 3);
            }
        }
    }

    // The window bits, as the stream's header writes them (RFC 7932, 9.1)
    inline void brotli_write_wbits(BrotliBitWriter& w, unsigned wbits) noexcept {
        if (wbits == 16) {
            w.add(0, 1);
        } else if (wbits == 17) {
            w.add(1, 7);
        } else if (wbits > 17) {
            w.add(1 | ((wbits - 17) << 1), 4);
        } else {
            w.add(1 | ((wbits - 8) << 4), 7);
        }
    }

    // ---- context maps -----------------------------------------------------------

    // n * log2(n), for the cost of counts
    inline float brotli_nlogn(uint32_t n) noexcept {
        static const auto table = [] {
            std::unique_ptr<float[]> t(new float[4096]);
            t[0] = 0;
            for (uint32_t i = 1; i < 4096; ++i) {
                t[i] = float(double(i) * std::log2(double(i)));
            }
            return t;
        }();
        return n < 4096 ? table[n] : float(double(n) * std::log2(double(n)));
    }

    // The 64 contexts' counts (64 x 256) clustered into at most `most` trees,
    // a pair merged while that costs less (and while there are too many):
    // the map of a context to its tree, the trees numbered as they first
    // appear; their count
    // Each cluster keeps the symbols it has as a mask of 256 bits, and a
    // cost walks those alone, in increasing order as the whole walk did (the
    // same sums, the same choices): a meta-block a flush, of a few KB, has
    // some tens of symbols a context, and the 2016 pairs' costs of 256 each
    // took milliseconds a meta-block
    inline unsigned brotli_cluster(const uint32_t* counts, unsigned most, uint8_t* map) noexcept {
        struct Cluster {
            uint32_t h[256];
            uint64_t has[4];
            float cost;
            bool alive;
        };
        // the cost of the sum of a and b (b may be a), over the symbols of either
        auto cost_of = [](const Cluster& a, const Cluster& b) noexcept {
            uint32_t total = 0;
            unsigned used = 0;
            float sum = 0;
            for (unsigned w = 0; w < 4; ++w) {
                uint64_t m = a.has[w] | b.has[w];
                while (m) {
                    const unsigned k = w * 64 + unsigned(std::countr_zero(m));
                    m &= m - 1;
                    const uint32_t v = &a == &b ? a.h[k] : a.h[k] + b.h[k];
                    total += v;
                    ++used;
                    sum += brotli_nlogn(v);
                }
            }
            if (used <= 1) {
                return 12.0f;
            }
            return brotli_nlogn(total) - sum + 20.0f + 5.0f * float(used);
        };
        std::vector<Cluster> c(64);
        int owner[64];
        unsigned live = 0;
        for (unsigned x = 0; x < 64; ++x) {
            const uint32_t* h = counts + size_t(x) * 256;
            std::copy(h, h + 256, c[x].h);
            for (unsigned w = 0; w < 4; ++w) {
                uint64_t m = 0;
                for (unsigned b = 0; b < 64; ++b) {
                    m |= uint64_t(h[w * 64 + b] != 0) << b;
                }
                c[x].has[w] = m;
            }
            const bool any = (c[x].has[0] | c[x].has[1] | c[x].has[2] | c[x].has[3]) != 0;
            c[x].alive = any;
            c[x].cost = any ? cost_of(c[x], c[x]) : 0;
            owner[x] = int(x);
            live += any;
        }
        if (live <= 1) {
            std::fill(map, map + 64, uint8_t(0));
            return 1;
        }
        // the cost change of merging a and b
        auto delta = [&](unsigned a, unsigned b) noexcept {
            return cost_of(c[a], c[b]) - c[a].cost - c[b].cost;
        };
        std::vector<float> d(64 * 64, 0);
        for (unsigned a = 0; a < 64; ++a) {
            for (unsigned b = a + 1; b < 64; ++b) {
                if (c[a].alive && c[b].alive) {
                    d[a * 64 + b] = delta(a, b);
                }
            }
        }
        for (;;) {
            float best = 1e30f;
            unsigned ba = 0, bb = 0;
            for (unsigned a = 0; a < 64; ++a) {
                if (!c[a].alive) {
                    continue;
                }
                for (unsigned b = a + 1; b < 64; ++b) {
                    if (c[b].alive && d[a * 64 + b] < best) {
                        best = d[a * 64 + b];
                        ba = a;
                        bb = b;
                    }
                }
            }
            if (best >= 0 && live <= most) {
                break;
            }
            for (unsigned k = 0; k < 256; ++k) {
                c[ba].h[k] += c[bb].h[k];
            }
            for (unsigned w = 0; w < 4; ++w) {
                c[ba].has[w] |= c[bb].has[w];
            }
            c[ba].cost = cost_of(c[ba], c[ba]);
            c[bb].alive = false;
            --live;
            for (unsigned x = 0; x < 64; ++x) {
                if (owner[x] == int(bb)) {
                    owner[x] = int(ba);
                }
            }
            for (unsigned x = 0; x < 64; ++x) {
                if (x != ba && c[x].alive) {
                    const float v = delta(std::min(x, ba), std::max(x, ba));
                    d[std::min(x, ba) * 64 + std::max(x, ba)] = v;
                }
            }
            if (live == 1) {
                break;
            }
        }
        // the trees numbered in the order the contexts meet them; an empty
        // context goes with tree 0
        int number[64];
        std::fill(number, number + 64, -1);
        unsigned trees = 0;
        for (unsigned x = 0; x < 64; ++x) {
            if (!c[owner[x]].alive) {
                continue;
            }
            const unsigned o = unsigned(owner[x]);
            bool empty = true;
            for (unsigned b = 0; b < 256 && empty; ++b) {
                empty = counts[size_t(x) * 256 + b] == 0;
            }
            if (empty) {
                continue;
            }
            if (number[o] < 0) {
                number[o] = int(trees++);
            }
        }
        for (unsigned x = 0; x < 64; ++x) {
            bool empty = true;
            for (unsigned b = 0; b < 256 && empty; ++b) {
                empty = counts[size_t(x) * 256 + b] == 0;
            }
            map[x] = empty ? 0 : uint8_t(number[owner[x]]);
        }
        return std::max(trees, 1u);
    }

    // VarLenUint8 (RFC 7932, 9.2): 0, or 1 and the bits of v
    inline void brotli_write_var8(BrotliBitWriter& w, unsigned v) noexcept {
        if (v == 0) {
            w.add(0, 1);
            return;
        }
        const unsigned nbits = unsigned(31 - std::countl_zero(v));
        w.add(1, 1);
        w.add(nbits, 3);
        w.add(v - (1u << nbits), nbits);
    }

    // A context map (RFC 7932, 7.3): NTREES, the map moved to front and its
    // runs of zeros coded with RLEMAX, under a code of its own
    inline void brotli_write_map(BrotliBitWriter& w, const uint8_t* map, size_t size, unsigned trees) noexcept {
        brotli_write_var8(w, trees - 1);
        // move to front
        std::vector<uint8_t> v(size);
        uint8_t mtf[256];
        for (unsigned k = 0; k < 256; ++k) {
            mtf[k] = uint8_t(k);
        }
        for (size_t i = 0; i < size; ++i) {
            unsigned idx = 0;
            while (mtf[idx] != map[i]) {
                ++idx;
            }
            v[i] = uint8_t(idx);
            const uint8_t value = mtf[idx];
            for (unsigned k = idx; k > 0; --k) {
                mtf[k] = mtf[k - 1];
            }
            mtf[0] = value;
        }
        // runs of zeros: RLEMAX from the longest
        size_t longest = 0;
        for (size_t i = 0; i < size;) {
            if (v[i] != 0) {
                ++i;
                continue;
            }
            size_t j = i;
            while (j < size && v[j] == 0) {
                ++j;
            }
            longest = std::max(longest, j - i);
            i = j;
        }
        unsigned rle = 0;
        if (longest >= 2) {
            rle = std::min(16u, unsigned(63 - std::countl_zero(uint64_t(longest))));
        }
        // the symbols: 0 one zero, 1..rle a run of 2^k + extra zeros, else value + rle
        std::vector<uint16_t> sym;
        std::vector<uint32_t> extra;
        for (size_t i = 0; i < size;) {
            if (v[i] != 0) {
                sym.push_back(uint16_t(v[i] + rle));
                extra.push_back(0);
                ++i;
                continue;
            }
            size_t j = i;
            while (j < size && v[j] == 0) {
                ++j;
            }
            size_t run = j - i;
            while (run) {
                if (run == 1 || rle == 0) {
                    sym.push_back(0);
                    extra.push_back(0);
                    --run;
                    continue;
                }
                const unsigned k = std::min(rle, unsigned(63 - std::countl_zero(uint64_t(run))));
                const size_t take = std::min(run, (size_t(1) << (k + 1)) - 1);
                sym.push_back(uint16_t(k));
                extra.push_back(uint32_t(take - (size_t(1) << k)));
                run -= take;
            }
            i = j;
        }
        std::vector<uint32_t> counts(trees + rle, 0);
        for (auto x : sym) {
            ++counts[x];
        }
        BrotliCode code;
        code.build(counts.data(), trees + rle);
        if (rle) {
            w.add(1, 1);
            w.add(rle - 1, 4);
        } else {
            w.add(0, 1);
        }
        brotli_write_code(w, code);
        for (size_t i = 0; i < sym.size(); ++i) {
            code.put(w, sym[i]);
            if (sym[i] >= 1 && sym[i] <= rle) {
                w.add(extra[i], sym[i]);
            }
        }
        w.add(1, 1);   // IMTF
    }

    // ---- the encoder -----------------------------------------------------------

    // A command: literals inserted, then a copy from a distance (0: none,
    // the meta-block ends with the literals)
    struct BrotliCommand {
        uint32_t insert;
        uint32_t copy;
        uint32_t distance;
        bool word = false;   // the copy is a word of the static dictionary (the distance past the window)
        uint8_t more = 0;    // the bytes its transform adds to the copy's length (a word's space)
    };

    // The static dictionary's words by their first four bytes: those of the
    // identity transform and of the word with a space after it (transform
    // 1), the common ones in text, for the encoder's search
    struct BrotliWords {
        static constexpr unsigned Bits = 15;
        uint8_t bytes[BrotliDictionarySize];
        std::vector<uint32_t> head;    // the last entry of a bucket, or ~0
        std::vector<uint32_t> next;    // the entry before in its bucket
        std::vector<uint32_t> entry;   // the length << 24 | the word's index

        static uint32_t hash(const uint8_t* p) noexcept {
            uint32_t v;
            std::memcpy(&v, p, 4);
            return (v * 0x9E3779B1u) >> (32 - Bits);
        }

        BrotliWords() noexcept : head(size_t(1) << Bits, ~0u) {
            for (uint32_t i = 0; i < BrotliDictionarySize; ++i) {
                bytes[i] = brotli_dictionary_byte(i);
            }
            for (unsigned len = 4; len <= 24; ++len) {
                const uint32_t count = uint32_t(1) << BrotliDictionaryBits[len];
                for (uint32_t i = 0; i < count; ++i) {
                    const uint32_t h = hash(bytes + BrotliDictionaryOffsets[len] + i * len);
                    next.push_back(head[h]);
                    head[h] = uint32_t(entry.size());
                    entry.push_back(len << 24 | i);
                }
            }
        }

        static const BrotliWords& get() noexcept {
            static const auto w = std::make_unique<BrotliWords>();
            return *w;
        }

        // The longest word at p (avail bytes there), plain or with a space
        // after it: its length in the data (0 for none) and its word id's
        // transform-free index and transform
        unsigned find(const uint8_t* p, size_t avail, uint32_t& index, unsigned& transform) const noexcept {
            if (avail < 4) {
                return 0;
            }
            unsigned best = 0;
            for (uint32_t e = head[hash(p)]; e != ~0u; e = next[e]) {
                const unsigned len = entry[e] >> 24;
                const uint32_t i = entry[e] & 0xFFFFFF;
                if (len > avail || len + 1 <= best) {
                    continue;
                }
                if (std::memcmp(bytes + BrotliDictionaryOffsets[len] + i * len, p, len) != 0) {
                    continue;
                }
                const bool space = len < avail && p[len] == ' ';
                const unsigned made = len + (space ? 1 : 0);
                if (made > best) {
                    best = made;
                    index = i;
                    transform = space ? 1 : 0;
                }
            }
            return best;
        }
    };

    struct BrotliSettings {
        int quality = 11;
        unsigned window_log = 22;
        const char* error = nullptr;

        template<class O>
        static BrotliSettings of(const O& o) noexcept {
            BrotliSettings s;
            s.quality = o.level.value();
            s.window_log = o.window_log;
            if (s.window_log < 10 || s.window_log > 24) {
                s.error = "a window_log of 10..24";
            }
            return s;
        }
    };

    // The match finder's parameters of a quality: the zstd level whose
    // output in brotli's coding comes nearest the ratio of libbrotli's
    // quality (measured on text: 2..9 at or a little under its sizes; 10 and
    // 11 the optimal parser, short of libbrotli's by 2-4%)
    inline ZstdParams brotli_params(int quality, uint64_t size, unsigned window_log) noexcept {
        static constexpr int levels[12] = {-2, 1, 2, 3, 5, 6, 7, 8, 10, 12, 16, 22};
        ZstdParams p = zstd_params(levels[std::clamp(quality, 0, 11)], size, window_log);
        p.window_trim = 16;
        return p;
    }

    class BrotliEngine {
    public:
        explicit BrotliEngine(const BrotliSettings& s, uint64_t size_hint) noexcept
        : _s(s) {
            _params = brotli_params(s.quality, size_hint, s.window_log);
            _matcher.configure(_params);
            _block_log = s.quality <= 4 ? 18 : 20;
            if (s.quality >= 10) {
                // the optimal parser's prices are zstd's: a lazy parse beside
                // it, the smaller meta-block of the two kept
                ZstdParams lazy = zstd_params(12, size_hint, s.window_log);
                lazy.window_log = _params.window_log;
                lazy.hash_log = std::min(lazy.hash_log, lazy.window_log + 1);
                lazy.chain_log = std::min(lazy.chain_log, lazy.window_log + 1);
                lazy.window_trim = 16;
                _alt = std::make_unique<ZstdMatcher>();
                _alt->configure(lazy);
            }
        }

        SGCL_INLINE_HOT unsigned window_log() const noexcept {
            return _params.window_log;
        }

        // The window the matcher reaches
        SGCL_INLINE_HOT size_t window() const noexcept {
            return (size_t(1) << _params.window_log) - 16;
        }

        // The largest meta-block's input
        SGCL_INLINE_HOT size_t block() const noexcept {
            return size_t(1) << _block_log;
        }

        void start(BrotliBitWriter& w) noexcept {
            w.reserve(16);
            brotli_write_wbits(w, _params.window_log);
        }

        // One meta-block of [src, src + n), its history from low (indexes
        // from base)
        void compress(BrotliBitWriter& w, const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, bool last) noexcept;

        // An empty last meta-block, to a byte boundary
        void finish(BrotliBitWriter& w) noexcept {
            w.reserve(16);
            w.add(3, 2);
            w.align();
        }

        // An empty metadata meta-block: everything so far to a byte boundary
        void flush(BrotliBitWriter& w) noexcept {
            w.reserve(16);
            w.add(0, 1);      // ISLAST
            w.add(3, 2);      // MNIBBLES: metadata
            w.add(0, 1);      // reserved
            w.add(0, 2);      // MSKIPBYTES: none
            w.align();
        }

        // A new stream: the tables cleared, the offsets as a stream starts
        void reset() noexcept {
            _matcher.reset();
            if (_alt) {
                _alt->reset();
            }
            _rep_alt[0] = 1;
            _rep_alt[1] = 4;
            _rep_alt[2] = 8;
            _next = 0;
            _rep[0] = 1;
            _rep[1] = 4;
            _rep[2] = 8;
            _ring[0] = 4;
            _ring[1] = 16;
            _ring[2] = 15;
            _ring[3] = 11;
            _ring_at = 0;
        }

        // Every index lowered by a multiple of the tree's ring below delta;
        // what it was lowered by
        uint32_t shift(uint32_t delta) noexcept {
            const uint32_t align = uint32_t(1) << _matcher.params().chain_log;
            delta &= ~(align - 1);
            _matcher.shift(delta);
            if (_alt) {
                _alt->shift(delta);
            }
            _next -= std::min(_next, delta);
            return delta;
        }

        // The index a stretch of n bytes may start at, with `keep` bytes of
        // history before it (the tables shifted down before 2^31)
        uint32_t room(size_t n, size_t keep) noexcept {
            if (uint64_t(_next) + n + ZstdBlockMax >= (uint64_t(1) << 31)) {
                const uint32_t align = uint32_t(1) << _matcher.params().chain_log;
                const uint32_t delta = (_next > keep ? _next - uint32_t(keep) : 0) & ~(align - 1);
                _matcher.shift(delta);
                if (_alt) {
                    _alt->shift(delta);
                }
                _next -= delta;
            }
            return _next;
        }

    private:
        void _parse(ZstdMatcher& m, uint32_t (&zrep)[3], std::vector<BrotliCommand>& out, const uint8_t* base, const uint8_t* low, const uint8_t* src,
                    size_t n) noexcept;
        void _write(BrotliBitWriter& w, const std::vector<BrotliCommand>& commands, const uint8_t* src, size_t n, size_t before, bool last) noexcept;
        void _words(std::vector<BrotliCommand>& commands, const uint8_t* src, size_t before) noexcept;

        BrotliSettings _s;
        ZstdParams _params;
        ZstdMatcher _matcher;
        unsigned _block_log = 18;
        uint32_t _next = 0;
        uint32_t _rep[3] = {1, 4, 8};             // the matcher's (zstd's) last offsets
        uint32_t _ring[4] = {4, 16, 15, 11};      // brotli's last distances: the last at _ring_at, older below it
        unsigned _ring_at = 0;
        std::vector<BrotliCommand> _commands;
        std::unique_ptr<ZstdMatcher> _alt;               // qualities 10 and 11: a lazy parse beside the optimal one
        uint32_t _rep_alt[3] = {1, 4, 8};
        std::vector<BrotliCommand> _commands_alt;
        // a command's codes
        struct Coded {
            uint16_t cmd;
            uint16_t dist;        // the distance code, 0xFFFF for none
            uint32_t dist_extra;
            uint8_t dist_bits;
            uint8_t ic, cc;       // the insert and copy codes
        };
        std::vector<Coded> _coded;
        std::unique_ptr<ZstdSequence[]> _seqs {new ZstdSequence[ZstdBlockWork::MaxSequences]};
        std::unique_ptr<uint8_t[]> _lits {new uint8_t[ZstdBlockMax + ZstdOutSlack]};
    };

    // The commands of [src, src + n), the matcher's sequences turned into
    // distances (its values are against zstd's three offsets)
    inline void BrotliEngine::_parse(ZstdMatcher& m, uint32_t (&zrep)[3], std::vector<BrotliCommand>& out, const uint8_t* base, const uint8_t* low,
                                     const uint8_t* src, size_t n) noexcept {
        out.clear();
        size_t at = 0;
        uint32_t pending = 0;   // literals before the next command
        // (a parse no longer than the window: the matcher measures the
        // window from its end)
        const size_t most = std::min(ZstdBlockMax, window());
        while (at < n) {
            const size_t k = std::min(most, n - at);
            ZstdSink sink {_seqs.get(), _lits.get()};
            uint32_t rep[3] = {zrep[0], zrep[1], zrep[2]};
            m.parse(base, low, src + at, k, rep, sink);
            const size_t count = size_t(sink.seq - _seqs.get());
            size_t covered = 0;
            for (size_t i = 0; i < count; ++i) {
                const ZstdSequence& q = _seqs[i];
                uint32_t o;
                if (q.value > 3) {
                    o = q.value - 3;
                } else if (q.literals) {
                    o = zrep[q.value - 1];
                } else {
                    o = q.value == 3 ? zrep[0] - 1 : zrep[q.value];
                }
                zstd_update_rep(zrep, o, q.literals);
                out.push_back({pending + q.literals, q.length, o});
                pending = 0;
                covered += q.literals + q.length;
            }
            pending += uint32_t(k - covered);
            at += k;
            const uint32_t end = uint32_t(src + at - base);
            if (end > _next) {
                _next = end;
            }
        }
        if (pending) {
            out.push_back({pending, 0, 0});
        }
    }

    // Words of the static dictionary in the runs of literals: a word where
    // its reference costs fewer bits than its bytes as literals would (about
    // six a byte); a run split around it into two commands, the word's copy
    // reaching past the data the window holds (RFC 7932, 8)
    inline void BrotliEngine::_words(std::vector<BrotliCommand>& commands, const uint8_t* src, size_t before) noexcept {
        const BrotliWords& dict = BrotliWords::get();
        const size_t window = this->window();
        std::vector<BrotliCommand> out;
        out.reserve(commands.size() + 16);
        const uint8_t* p = src;
        size_t at = before;   // the stream's position of p
        bool any = false;
        for (const BrotliCommand& c : commands) {
            uint32_t from = 0;   // the literals of c not yet given out
            uint32_t i = 0;
            auto letter = [](uint8_t b) noexcept { return uint8_t((b | 0x20) - 'a') < 26 || b >= 0x80; };
            while (c.insert >= 4 && i + 4 <= c.insert) {
                // words begin where a word of the data does
                if ((i > 0 || at > 0) && letter(p[ptrdiff_t(i) - 1]) && letter(p[i])) {
                    ++i;
                    continue;
                }
                uint32_t index = 0;
                unsigned transform = 0;
                const unsigned len = dict.find(p + i, c.insert - i, index, transform);
                if (len == 0) {
                    ++i;
                    continue;
                }
                const unsigned word_len = len - transform;   // the word's own length
                const uint64_t max_distance = std::min<uint64_t>(window, at + i);
                const uint64_t id = uint64_t(index) | (uint64_t(transform) << BrotliDictionaryBits[word_len]);
                const uint64_t d = max_distance + 1 + id;
                const unsigned nbits = unsigned(63 - std::countl_zero(d + 3)) - 1;
                if (6 * len <= 18 + nbits || d > 0x3FFFFFF) {
                    ++i;
                    continue;
                }
                out.push_back({i - from, word_len, uint32_t(d), true, uint8_t(transform)});
                i += len;
                from = i;
                any = true;
            }
            if (c.insert > from || c.copy) {   // (literals that all went into words: no command left)
                out.push_back({c.insert - from, c.copy, c.distance, c.word, c.more});
            }
            p += c.insert + c.copy + c.more;
            at += c.insert + c.copy + c.more;
        }
        if (any) {
            commands.swap(out);
        }
    }

    inline void BrotliEngine::compress(BrotliBitWriter& w, const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, bool last) noexcept {
        if (n == 0) {
            if (last) {
                finish(w);
            }
            return;
        }
        const size_t before = size_t(src - low);
        _parse(_matcher, _rep, _commands, base, low, src, n);
        if (_s.quality >= 2) {
            _words(_commands, src, before);
        }
        w.reserve(2 * (n + n / 8 + (size_t(1) << 16)));   // the tables and the bytes, or the bytes stored
        if (!_alt) {
            _write(w, _commands, src, n, before, last);
            return;
        }
        // the two parses written in turn from the same point; the smaller kept
        _parse(*_alt, _rep_alt, _commands_alt, base, low, src, n);
        _words(_commands_alt, src, before);
        const BrotliBitWriter::Mark m = w.mark();
        uint32_t ring[4] = {_ring[0], _ring[1], _ring[2], _ring[3]};
        const unsigned ring_at = _ring_at;
        _write(w, _commands, src, n, before, last);
        const size_t first = w.size_bits();
        w.restore(m);
        std::copy(ring, ring + 4, _ring);
        _ring_at = ring_at;
        _write(w, _commands_alt, src, n, before, last);
        if (first < w.size_bits()) {
            w.restore(m);
            std::copy(ring, ring + 4, _ring);
            _ring_at = ring_at;
            _write(w, _commands, src, n, before, last);
        }
    }

    // The meta-block of the commands: compressed, or stored when that is
    // not smaller
    inline void BrotliEngine::_write(BrotliBitWriter& w, const std::vector<BrotliCommand>& commands, const uint8_t* src, size_t n, size_t before,
                                     bool last) noexcept {
        // the codes of the commands, their counts
        _coded.resize(commands.size());
        Coded* const coded = _coded.data();
        std::vector<uint32_t> cmd_counts(704, 0), dist_counts(64, 0);
        // the literals' contexts (from quality 5, a meta-block of 8 KB and
        // up): the mode by the bytes (UTF-8 for text, signed otherwise),
        // counts for each of 64 contexts. A smaller meta-block (a flush of
        // a live response a few KB) has too few literals a context to pay
        // for the map and the trees: measured, 4 KB came out larger than
        // without (code, programs, HTML) at three times the time, 8 KB 1-6 %
        // smaller
        const bool contexts = _s.quality >= 5 && n >= 8192;
        unsigned mode = 0;
        if (contexts) {
            size_t high = 0;
            for (size_t i = 0; i < n; i += 7) {
                high += src[i] >= 0x80;
            }
            mode = high * 4 < (n + 6) / 7 ? 2 : 3;
        }
        const uint8_t* const lut = BrotliContextLut + 512 * mode;
        std::vector<uint32_t> lit_counts(contexts ? 64 * 256 : 256, 0);
        uint32_t ring[4] = {_ring[0], _ring[1], _ring[2], _ring[3]};
        unsigned ring_at = _ring_at;
        const uint8_t* p = src;
        for (size_t i = 0; i < commands.size(); ++i) {
            const BrotliCommand& c = commands[i];
            if (contexts) {
                for (uint32_t j = 0; j < c.insert; ++j) {
                    const size_t at = size_t(p + j - src) + before;
                    const uint8_t p1 = at >= 1 ? p[ptrdiff_t(j) - 1] : 0;
                    const uint8_t p2 = at >= 2 ? p[ptrdiff_t(j) - 2] : 0;
                    ++lit_counts[size_t(lut[p1] | lut[256 + p2]) * 256 + p[j]];
                }
            } else {
                uint32_t* const h = lit_counts.data();
                for (uint32_t j = 0; j < c.insert; ++j) {
                    ++h[p[j]];
                }
            }
            p += c.insert + c.copy + c.more;
            const unsigned ic = brotli_insert_code(c.insert);
            Coded& k = coded[i];
            k.ic = uint8_t(ic);
            if (c.copy == 0) {
                // the last literals: a copy code of 0 the meta-block's end leaves out
                k.cmd = uint16_t(brotli_command_code(ic, 0, true));
                k.dist = 0xFFFF;
                ++cmd_counts[k.cmd];
                continue;
            }
            const unsigned cc = brotli_copy_code(c.copy);
            k.cc = uint8_t(cc);
            const uint32_t d = c.distance;
            unsigned code;
            const uint32_t l0 = ring[ring_at];
            const uint32_t l1 = ring[(ring_at - 1) & 3];
            if (c.word) {
                code = 16;   // a word: coded as it is, never the last distances
            } else if (d == l0) {
                code = 0;
            } else if (d == l1) {
                code = 1;
            } else if (d == ring[(ring_at - 2) & 3]) {
                code = 2;
            } else if (d == ring[(ring_at - 3) & 3]) {
                code = 3;
            } else if (const int64_t e0 = int64_t(d) - l0; e0 >= -3 && e0 <= 3) {
                // the last distance -1, +1, -2, +2, -3, +3: codes 4..9
                code = 4 + 2 * unsigned((e0 < 0 ? -e0 : e0) - 1) + (e0 > 0);
            } else if (const int64_t e1 = int64_t(d) - l1; e1 >= -3 && e1 <= 3) {
                code = 10 + 2 * unsigned((e1 < 0 ? -e1 : e1) - 1) + (e1 > 0);
            } else {
                code = 16;
            }
            k.dist_bits = 0;
            k.dist_extra = 0;
            if (code < 16) {
                k.dist = uint16_t(code);
            } else {
                // NPOSTFIX 0, NDIRECT 0: d + 3 = ((2 + low bit) << nbits) + extra
                const uint32_t v = d + 3;
                const unsigned top = unsigned(31 - std::countl_zero(v));
                const unsigned nbits = top - 1;
                const unsigned x = 2 * (nbits - 1) + ((v >> nbits) & 1);
                k.dist = uint16_t(16 + x);
                k.dist_bits = uint8_t(nbits);
                k.dist_extra = v & ((uint32_t(1) << nbits) - 1);
            }
            if (code != 0 && !c.word) {
                ring_at = (ring_at + 1) & 3;
                ring[ring_at] = d;
            }
            const bool implicit = code == 0 && ic < 8 && cc < 16;
            k.cmd = uint16_t(brotli_command_code(ic, cc, implicit));
            ++cmd_counts[k.cmd];
            if (implicit) {
                k.dist = 0xFFFF;
            } else {
                ++dist_counts[k.dist];
            }
        }
        // the contexts clustered into trees: the map and a code for each
        uint8_t map[64] = {};
        unsigned trees = 1;
        if (contexts) {
            trees = brotli_cluster(lit_counts.data(), _s.quality >= 7 ? 64 : 16, map);
        }
        std::vector<BrotliCode> lit(trees);
        {
            std::vector<uint32_t> merged(256);
            for (unsigned t = 0; t < trees; ++t) {
                std::fill(merged.begin(), merged.end(), 0u);
                for (unsigned x = 0; x < (contexts ? 64u : 1u); ++x) {
                    if (map[x] == t) {
                        for (unsigned b = 0; b < 256; ++b) {
                            merged[b] += lit_counts[size_t(x) * 256 + b];
                        }
                    }
                }
                lit[t].build(merged.data(), 256);
            }
        }
        BrotliCode cmd, dist;
        cmd.build(cmd_counts.data(), 704);
        dist.build(dist_counts.data(), 64);
        const BrotliBitWriter::Mark start_mark = w.mark();
        // the header: ISLAST, MNIBBLES, MLEN - 1, ISUNCOMPRESSED
        auto header = [&](bool is_last, bool stored) noexcept {
            w.add(is_last ? 1 : 0, 1);
            if (is_last) {
                w.add(0, 1);   // ISLASTEMPTY
            }
            const uint32_t m = uint32_t(n - 1);
            const unsigned nibbles = m < (1u << 16) ? 4 : m < (1u << 20) ? 5 : 6;
            w.add(nibbles - 4, 2);
            w.add(m, 4 * nibbles);
            if (!is_last) {
                w.add(stored ? 1 : 0, 1);
            }
        };
        header(last, false);
        w.add(0, 1);   // NBLTYPESL - 1 = 0
        w.add(0, 1);   // NBLTYPESI
        w.add(0, 1);   // NBLTYPESD
        w.add(0, 2);   // NPOSTFIX
        w.add(0, 4);   // NDIRECT
        w.add(mode, 2);   // the literals' context mode
        if (trees > 1) {
            brotli_write_map(w, map, 64, trees);
        } else {
            w.add(0, 1);   // NTREESL - 1 = 0
        }
        w.add(0, 1);   // NTREESD - 1 = 0
        for (auto& t : lit) {
            brotli_write_code(w, t);
        }
        brotli_write_code(w, cmd);
        brotli_write_code(w, dist);
        p = src;
        {
            // the writer's state in locals for the loop (its fields kept in
            // registers), given back after it
            BrotliBitWriter lw(std::move(w));
            for (size_t i = 0; i < commands.size(); ++i) {
                const BrotliCommand& c = commands[i];
                const Coded& k = coded[i];
                cmd.put(lw, k.cmd);
                const unsigned ic = k.ic;
                lw.add(c.insert - BrotliInsertBase[ic], BrotliInsertExtra[ic]);
                if (c.copy) {
                    const unsigned cc = k.cc;
                    lw.add(c.copy - BrotliCopyBase[cc], BrotliCopyExtra[cc]);
                }
                if (trees > 1) {
                    for (uint32_t j = 0; j < c.insert; ++j) {
                        const size_t at = size_t(p + j - src) + before;
                        const uint8_t p1 = at >= 1 ? p[ptrdiff_t(j) - 1] : 0;
                        const uint8_t p2 = at >= 2 ? p[ptrdiff_t(j) - 2] : 0;
                        lit[map[lut[p1] | lut[256 + p2]]].put(lw, p[j]);
                    }
                } else {
                    const uint16_t* const codes = lit[0].codes;
                    const uint8_t* const lens = lit[0].lengths;
                    for (uint32_t j = 0; j < c.insert; ++j) {
                        lw.add(codes[p[j]], lens[p[j]]);
                    }
                }
                p += c.insert + c.copy + c.more;
                if (k.dist != 0xFFFF) {
                    dist.put(lw, k.dist);
                    lw.add(k.dist_extra, k.dist_bits);
                }
            }
            w = std::move(lw);
        }
        if (last) {
            w.align();
        }
        // stored instead, when that is not larger
        const size_t made = w.size_bits() - (start_mark.used * 8 + start_mark.bits);
        if (made > n * 8 + 48) {
            w.restore(start_mark);
            if (last) {
                header(false, true);
                w.bytes(src, n);
                finish(w);
            } else {
                header(false, true);
                w.bytes(src, n);
            }
            return;   // the ring stays as it was: a stored meta-block moves nothing
        }
        std::copy(ring, ring + 4, _ring);
        _ring_at = ring_at;
    }

    // A whole compress in memory: the meta-blocks straight from the input,
    // the history the input itself
    template<class Out>
    void brotli_compress_all(Out& out, const BrotliSettings& s, const uint8_t* p, size_t n) noexcept {
        auto engine = std::make_unique<BrotliEngine>(s, n);
        BrotliBitWriter w;
        w.reserve(64);
        engine->start(w);
        const size_t block = engine->block();
        size_t at = 0;
        do {
            const size_t k = std::min(block, n - at);
            const size_t keep = std::min(at, engine->window());
            const uint32_t start = engine->room(k, keep);
            const uint8_t* src = p + at;
            engine->compress(w, src - start, p, src, k, at + k == n);
            at += k;
            w.drain(out);
        } while (at < n);
        w.drain(out);
    }

    // The stream's encoder (codec_stream.h): what is written gathers into
    // a buffer of two windows and a meta-block's input; a meta-block goes
    // out when its input is whole (and a byte after it), at a flush and at
    // the end
    class BrotliFrameEncoder {
    public:
        static constexpr const char* name = "brotli";

        template<class O>
        explicit BrotliFrameEncoder(const O& o, uint64_t size_hint = UINT64_MAX) noexcept
        : _s(BrotliSettings::of(o)) {
            if (_s.error) {
                return;
            }
            if (_s.quality < 0 || _s.quality > 11) {
                _s.error = "a level of 0..11";
                return;
            }
            _engine.reset(new BrotliEngine(_s, size_hint));
            _window = _engine->window();
            _block = _engine->block();
            _capacity = 2 * _window + 2 * _block + ZstdOutSlack;
            _buffer.reset(new uint8_t[_capacity]);
            reset();
        }

        SGCL_INLINE_HOT const char* setup_error() const noexcept {
            return _s.error;
        }

        template<class Out>
        void start(Out& out) noexcept {
            _engine->start(_w);
            _drain(out);
        }

        template<class Out>
        void write(const uint8_t* p, size_t n, Out& out) noexcept {
            while (n) {
                if (_end + _block + ZstdOutSlack > _capacity) {
                    _slide();
                }
                const size_t room = _capacity - ZstdOutSlack - _end;
                const size_t k = std::min(n, room);
                sgcl::detail::copy_bytes(_buffer.get() + _end, p, k);
                _end += k;
                p += k;
                n -= k;
                while (_end - _pending > _block) {
                    _meta(out, _block, false);
                }
            }
        }

        template<class Out>
        void flush(Out& out) noexcept {
            while (_end > _pending) {
                _meta(out, std::min(_block, _end - _pending), false);
            }
            _engine->flush(_w);
            _drain(out);
        }

        template<class Out>
        void finish(Out& out) noexcept {
            while (_end - _pending > _block) {
                _meta(out, _block, false);
            }
            if (_end > _pending) {
                _meta(out, _end - _pending, true);
            } else {
                _engine->finish(_w);
            }
            _drain(out);
        }

        void reset() noexcept {
            if (_s.error) {
                return;
            }
            _engine->reset();
            _origin = 0;
            _w.used = 0;
            _w.acc = 0;
            _w.bits = 0;
            _low = 0;
            _pending = 0;
            _end = 0;
        }

    private:
        template<class Out>
        void _drain(Out& out) noexcept {
            _w.drain(out);
        }

        template<class Out>
        void _meta(Out& out, size_t k, bool last) noexcept {
            uint8_t* const src = _buffer.get() + _pending;
            _engine->compress(_w, _buffer.get() - _origin, _buffer.get() + _low, src, k, last);
            _pending += k;
            _drain(out);
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
                _origin -= _engine->shift(_origin);
            }
        }

        BrotliSettings _s;
        std::unique_ptr<BrotliEngine> _engine;
        BrotliBitWriter _w;
        size_t _window = 0;
        size_t _block = 0;
        size_t _capacity = 0;
        std::unique_ptr<uint8_t[]> _buffer;
        uint32_t _origin = 0;    // the index of the buffer's first byte
        size_t _low = 0;
        size_t _pending = 0;
        size_t _end = 0;
    };
}
