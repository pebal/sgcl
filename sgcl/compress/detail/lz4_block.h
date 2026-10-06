//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// The LZ4 block format (LZ4 Block Format Description 1.6): a block is a run
// of sequences, each a token byte (the high four bits a count of literals,
// the low four a match length less four, 15 in either meaning more bytes
// follow, each added until one is not 255), the literals, a two-byte
// little-endian offset back into what was decoded (1..65535) and the match
// length's further bytes. The last sequence has literals only. A
// compressor keeps the last five bytes literals and starts no match in the
// last twelve, so that a decoder may copy in wide steps.
//
// The decoder copies literals and matches 16 or 32 bytes at a time, and
// an overlapping match (an offset under 16) by a pattern: it writes up to
// Lz4OutSlack bytes past the end of the room it was given, which the
// caller provides, and it reads no byte of the input past its end.
//
// Four compressors, by level:
//   - fast (1, and a negative level -N: acceleration N): one hash table of
//     4096 positions, keyed by five bytes, probed once a position; after
//     misses it steps over more and more bytes (acceleration raises the
//     step), and a match is extended backwards over the literals before it;
//   - two tables (2): the last position under a hash of eight bytes and
//     of four, both probed at every position;
//   - hash chains (3..9): a table of 32768 heads and a chain of 65536
//     links (one a position of the window), walked up to 2^(level-1)
//     candidates; near the end of each match a match that reaches farther
//     is looked for, allowed to begin inside it, and two ahead decide
//     which to keep and where to cut (_ahead);
//   - the optimal parser (10..12): the price in bytes of every way to reach
//     each position of a stretch of up to 4096, from the literals and every
//     match the chains give, and the cheapest path taken back.
//
// Positions are kept as 32-bit indexes from a base the caller chooses, so a
// window that moves only moves its base; before the indexes reach 2^31 the
// tables are shifted down.
namespace sgcl::compress::detail {
    inline constexpr size_t Lz4MinMatch = 4;
    inline constexpr size_t Lz4LastLiterals = 5;
    inline constexpr size_t Lz4MatchFindLimit = 12;   // no match starts in the last 12 bytes
    inline constexpr size_t Lz4MaxDistance = 65535;
    inline constexpr size_t Lz4OutSlack = 32;          // what the decoder may write past the room it was given
    inline constexpr size_t Lz4Window = 65536;

    // The largest a block of n bytes compresses to: every byte a literal,
    // a length byte every 255, the token
    SGCL_INLINE_HOT constexpr size_t lz4_bound(size_t n) noexcept {
        return n + n / 255 + 16;
    }

    SGCL_INLINE_HOT uint32_t lz4_read32(const uint8_t* p) noexcept {
        uint32_t v;
        std::memcpy(&v, p, 4);
        return v;
    }

    SGCL_INLINE_HOT uint64_t lz4_read64(const uint8_t* p) noexcept {
        uint64_t v;
        std::memcpy(&v, p, 8);
        return v;
    }

    SGCL_INLINE_HOT void lz4_copy8(uint8_t* d, const uint8_t* s) noexcept {
        std::memcpy(d, s, 8);
    }

    SGCL_INLINE_HOT void lz4_copy16(uint8_t* d, const uint8_t* s) noexcept {
        std::memcpy(d, s, 16);
    }

    // How many bytes from p equal those from m, up to limit (p's end): eight
    // at a time, the first difference found by counting the trailing zero
    // bits of the XOR (little-endian)
    SGCL_INLINE_HOT size_t lz4_count(const uint8_t* p, const uint8_t* m, const uint8_t* limit) noexcept {
        const uint8_t* start = p;
        while (p + 8 <= limit) {
            uint64_t x = lz4_read64(p) ^ lz4_read64(m);
            if (x) {
                if constexpr (std::endian::native == std::endian::little) {
                    return size_t(p - start) + size_t(std::countr_zero(x) >> 3);
                } else {
                    return size_t(p - start) + size_t(std::countl_zero(x) >> 3);
                }
            }
            p += 8;
            m += 8;
        }
        while (p < limit && *p == *m) {
            ++p;
            ++m;
        }
        return size_t(p - start);
    }

    // How many bytes before p equal those before m, up to `most`: eight at
    // a time from the end, the first difference found by counting the
    // leading zero bits of the XOR (the last byte the most significant)
    SGCL_INLINE_HOT size_t lz4_count_back(const uint8_t* p, const uint8_t* m, size_t most) noexcept {
        size_t n = 0;
        while (n + 8 <= most) {
            const uint64_t x = lz4_read64(p - n - 8) ^ lz4_read64(m - n - 8);
            if (x) {
                if constexpr (std::endian::native == std::endian::little) {
                    return n + size_t(std::countl_zero(x) >> 3);
                } else {
                    return n + size_t(std::countr_zero(x) >> 3);
                }
            }
            n += 8;
        }
        while (n < most && p[-1 - ptrdiff_t(n)] == m[-1 - ptrdiff_t(n)]) {
            ++n;
        }
        return n;
    }

    // What a block's decoding ended in
    enum class Lz4Decoded : uint8_t {
        ok,
        truncated,   // the input ends inside a sequence
        overflow,    // more output than the room
        offset,      // an offset before the history
        zero         // an offset of 0, which the format forbids
    };

    SGCL_INLINE_HOT const char* lz4_why(Lz4Decoded d) noexcept {
        switch (d) {
            case Lz4Decoded::offset: return "lz4: a match before the start of the data";
            case Lz4Decoded::zero: return "lz4: a match of offset 0";
            case Lz4Decoded::overflow: return "lz4: a block decodes past the frame's block size";
            case Lz4Decoded::truncated: return "lz4: a block ends inside a sequence";
            case Lz4Decoded::ok: break;
        }
        return "lz4: corrupt data";
    }

    struct Lz4DecodeResult {
        Lz4Decoded status;
        size_t written;   // the bytes made (on success)
        size_t at;        // the byte of the input where it failed
    };

    // A match of ml bytes `offset` back (offset 1 or more, the source all
    // made) copied to op, which has room for ml and 32 bytes past them: 32
    // bytes a step from far back, 16 from 16 back, 8 from 8 back; nearer,
    // the pattern of 1, 2 or 4 bytes is stored a word at a time, and of 3,
    // 5, 6 or 7 its first eight bytes are made one by one and the rest
    // copied 8 at a time from the nearest multiple of the offset eight or
    // more back, which holds the same bytes. Returns op + ml
    SGCL_INLINE_HOT uint8_t* lz4_copy_match(uint8_t* op, size_t offset, size_t ml) noexcept {
        const uint8_t* m = op - offset;
        uint8_t* const e = op + ml;
        if (offset >= 32) {
            do {
                lz4_copy16(op, m);
                lz4_copy16(op + 16, m + 16);
                op += 32;
                m += 32;
            } while (op < e);
            return e;
        }
        if (offset >= 16) {
            do {
                lz4_copy16(op, m);
                op += 16;
                m += 16;
            } while (op < e);
            return e;
        }
        if (offset < 8) {
            if ((offset & (offset - 1)) == 0) {
                uint64_t v;
                if (offset == 1) {
                    v = 0x0101010101010101ull * m[0];
                } else if (offset == 2) {
                    uint16_t h;
                    std::memcpy(&h, m, 2);
                    v = 0x0001000100010001ull * h;
                } else {
                    uint32_t w;
                    std::memcpy(&w, m, 4);
                    v = 0x0000000100000001ull * w;
                }
                do {
                    std::memcpy(op, &v, 8);
                    op += 8;
                } while (op < e);
                return e;
            }
            for (int i = 0; i < 8; ++i) {
                op[i] = m[i];
            }
            static constexpr uint8_t step[8] = {0, 8, 8, 9, 8, 10, 12, 14};
            op += 8;
            m = op - step[offset];
        }
        while (op < e) {
            lz4_copy8(op, m);
            op += 8;
            m += 8;
        }
        return e;
    }

    // Decodes the block [src, src + n) into [dst, dst_end); matches may
    // reach back to `low` (the history before dst: the dictionary, the
    // blocks before a linked one; dst itself for none). Up to Lz4OutSlack
    // bytes past dst_end are written over.
    //
    // Two loops. The fast one runs while the input has 17 bytes past a
    // token and the room 64 bytes past the output: there a run of up to 14
    // literals goes as 16 bytes and a match of up to 18 bytes from 8 or
    // more back as 18 (8, 8 and 2), each with one test, and nothing else
    // about the ends is asked; a long run or match is checked as it comes.
    // Near either end the careful loop takes over, in the middle of a
    // sequence where need be, with every length and end checked.
    inline Lz4DecodeResult lz4_decode_block(const uint8_t* src, size_t n, uint8_t* dst, uint8_t* dst_end, const uint8_t* low) noexcept {
        const uint8_t* ip = src;
        const uint8_t* const iend = src + n;
        uint8_t* op = dst;
        auto fail = [&](Lz4Decoded why) noexcept {
            return Lz4DecodeResult {why, 0, size_t(ip - src)};
        };
        if (n == 0) {
            return fail(Lz4Decoded::truncated);
        }
        unsigned token;
        size_t ll;
        size_t ml;
        size_t offset;
        if (n >= 18 && dst_end - dst > 64) {
            const uint8_t* const fast_iend = iend - 17;   // a run of 14, the offset, the next token
            uint8_t* const fast_oend = dst_end - 64;
            // here ip < iend and op < fast_oend
            for (;;) {
                token = *ip++;
                ll = token >> 4;
                if (ll < 15) {
                    if (ip > fast_iend) {
                        goto literals;
                    }
                    lz4_copy16(op, ip);
                    op += ll;
                    ip += ll;
                } else {
                    unsigned b;
                    do {
                        if (ip >= iend) {
                            return fail(Lz4Decoded::truncated);
                        }
                        b = *ip++;
                        ll += b;
                    } while (b == 255);
                    if (ll > size_t(iend - ip) || ll > size_t(fast_oend - op) || size_t(iend - ip) - ll < 32) {
                        if (ll < 15) {
                            return fail(Lz4Decoded::overflow);
                        }
                        goto copy_literals;   // near an end, or past it: the careful copy says which
                    }
                    uint8_t* const e = op + ll;
                    const uint8_t* s = ip;
                    do {
                        lz4_copy16(op, s);
                        lz4_copy16(op + 16, s + 16);
                        op += 32;
                        s += 32;
                    } while (op < e);
                    op = e;
                    ip += ll;
                }
                offset = size_t(ip[0]) | size_t(ip[1]) << 8;
                ip += 2;
                ml = token & 15;
                if (ml < 15) {
                    ml += Lz4MinMatch;
                    if (op + ml >= fast_oend) {
                        goto match;
                    }
                    if (offset >= 8 && offset <= size_t(op - low)) {
                        const uint8_t* m = op - offset;
                        lz4_copy8(op, m);
                        lz4_copy8(op + 8, m + 8);
                        std::memcpy(op + 16, m + 16, 2);
                        op += ml;
                        continue;
                    }
                } else {
                    unsigned b;
                    do {
                        if (ip >= iend) {
                            return fail(Lz4Decoded::truncated);
                        }
                        b = *ip++;
                        ml += b;
                    } while (b == 255);
                    if (ml < 15) {
                        return fail(Lz4Decoded::overflow);
                    }
                    ml += Lz4MinMatch;
                    if (ml >= size_t(fast_oend - op)) {
                        goto match;
                    }
                }
                if (offset == 0) {
                    return fail(Lz4Decoded::zero);
                }
                if (offset > size_t(op - low)) {
                    return fail(Lz4Decoded::offset);
                }
                op = lz4_copy_match(op, offset, ml);
                if (ip >= iend) {   // a block ends with literals, never after a match
                    return fail(Lz4Decoded::truncated);
                }
            }
        }
        // the careful loop
        for (;;) {
            if (ip >= iend) {   // a block ends with literals, never after a match
                return fail(Lz4Decoded::truncated);
            }
            token = *ip++;
            ll = token >> 4;
        literals:
            if (ll == 15) {
                unsigned b;
                do {
                    if (ip >= iend) {
                        return fail(Lz4Decoded::truncated);
                    }
                    b = *ip++;
                    ll += b;
                } while (b == 255);
                if (ll < 15) {   // wrapped: no input is that long
                    return fail(Lz4Decoded::overflow);
                }
            }
        copy_literals:
            if (ll > size_t(iend - ip)) {
                return fail(Lz4Decoded::truncated);
            }
            if (ll > size_t(dst_end - op)) {
                return fail(Lz4Decoded::overflow);
            }
            if (size_t(iend - ip) >= ll + 16) {
                uint8_t* d = op;
                const uint8_t* s = ip;
                uint8_t* const e = op + ll;
                do {
                    lz4_copy16(d, s);
                    d += 16;
                    s += 16;
                } while (d < e);
            } else {
                sgcl::detail::copy_bytes(op, ip, ll);
            }
            op += ll;
            ip += ll;
            if (ip == iend) {   // the last sequence: literals only
                return {Lz4Decoded::ok, size_t(op - dst), 0};
            }
            if (iend - ip < 2) {
                return fail(Lz4Decoded::truncated);
            }
            offset = size_t(ip[0]) | size_t(ip[1]) << 8;
            ip += 2;
            ml = token & 15;
            if (ml == 15) {
                unsigned b;
                do {
                    if (ip >= iend) {
                        return fail(Lz4Decoded::truncated);
                    }
                    b = *ip++;
                    ml += b;
                } while (b == 255);
                if (ml < 15) {
                    return fail(Lz4Decoded::overflow);
                }
            }
            ml += Lz4MinMatch;
        match:
            if (offset == 0) {
                return fail(Lz4Decoded::zero);
            }
            if (offset > size_t(op - low)) {
                return fail(Lz4Decoded::offset);
            }
            if (ml > size_t(dst_end - op)) {
                return fail(Lz4Decoded::overflow);
            }
            op = lz4_copy_match(op, offset, ml);
        }
    }

    // The writing side: a sequence's token, lengths, literals and offset
    SGCL_INLINE_HOT uint8_t* lz4_put_length(uint8_t* op, size_t n) noexcept {
        while (n >= 255) {
            *op++ = 255;
            n -= 255;
        }
        *op++ = uint8_t(n);
        return op;
    }

    // One sequence: the literals [anchor, ip) and a match of ml bytes
    // `offset` back (ml 0: the last sequence, literals only)
    SGCL_INLINE_HOT uint8_t* lz4_put_sequence(uint8_t* op, const uint8_t* anchor, const uint8_t* ip, size_t offset, size_t ml) noexcept {
        const size_t ll = size_t(ip - anchor);
        uint8_t* token = op++;
        unsigned t = ll >= 15 ? 15u << 4 : unsigned(ll) << 4;
        if (ll >= 15) {
            op = lz4_put_length(op, ll - 15);
        }
        if (ml) {
            // a sequence with a match leaves at least 12 bytes of the input
            // after its literals and writes more than 8 bytes after them:
            // the literals go 8 at a time, the last step over the end
            uint8_t* d = op;
            const uint8_t* s = anchor;
            uint8_t* const e = op + ll;
            while (d < e) {
                lz4_copy8(d, s);
                d += 8;
                s += 8;
            }
            op = e;
        } else {
            sgcl::detail::copy_bytes(op, anchor, ll);
            op += ll;
        }
        if (ml) {
            op[0] = uint8_t(offset);
            op[1] = uint8_t(offset >> 8);
            op += 2;
            const size_t extra = ml - Lz4MinMatch;
            if (extra >= 15) {
                t |= 15;
                op = lz4_put_length(op, extra - 15);
            } else {
                t |= unsigned(extra);
            }
        }
        *token = uint8_t(t);
        return op;
    }

    // The fast compressor's hash of the five bytes at p (the product of
    // the low 40 bits keeps them all in the top 12 of the result)
    SGCL_INLINE_HOT uint32_t lz4_hash5(const uint8_t* p) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            return uint32_t(((lz4_read64(p) << 24) * 889523592379ull) >> 52);
        } else {
            return uint32_t(((lz4_read64(p) >> 24) * 11400714785074694791ull) >> 52);
        }
    }

    // The fast compressor. Its table holds indexes from `base`; one index
    // below `low` (the start of the history) is never followed
    class Lz4Fast {
    public:
        static constexpr unsigned TableBits = 12;

        Lz4Fast() noexcept {
            reset();
        }

        void reset() noexcept {
            std::fill(std::begin(_table), std::end(_table), 0u);
        }

        // Every index lowered by `delta` (0 when below): the window's base
        // moved forward by delta
        void shift(uint32_t delta) noexcept {
            for (auto& v : _table) {
                v = v > delta ? v - delta : 0;
            }
        }

        // The positions of [from, to) (a dictionary) into the table
        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            for (const uint8_t* p = from; p + 8 <= to; ++p) {
                _table[lz4_hash5(p)] = uint32_t(p - base);
            }
        }

        // Compresses [src, src + n): the history from `low` on (indexes of
        // base); returns the end of what was written at dst, which has
        // lz4_bound(n) bytes
        uint8_t* compress(const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, uint8_t* dst, int acceleration) noexcept {
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            uint8_t* op = dst;
            if (n < Lz4MatchFindLimit + 1) {
                return lz4_put_sequence(op, anchor, iend, 0, 0);
            }
            const uint8_t* const mflimit = iend - Lz4MatchFindLimit;   // a match starts at or before
            const uint8_t* const matchlimit = iend - Lz4LastLiterals;  // a match ends at or before
            const uint32_t low_index = uint32_t(low - base);
            const unsigned skip_trigger = 6;
            const unsigned accel = unsigned(std::max(acceleration, 1));
            // the history's positions go in as they are reached, except
            // the first, which no match may precede
            _table[lz4_hash5(ip)] = uint32_t(ip - base);
            ++ip;
            uint32_t forward_hash = lz4_hash5(ip);
            for (;;) {
                const uint8_t* match;
                // find a match
                {
                    const uint8_t* forward = ip;
                    unsigned step = 1;
                    unsigned searches = accel << skip_trigger;
                    for (;;) {
                        const uint32_t h = forward_hash;
                        ip = forward;
                        forward += step;
                        step = searches++ >> skip_trigger;
                        if (forward > mflimit + 1) {
                            return lz4_put_sequence(op, anchor, iend, 0, 0);
                        }
                        const uint32_t candidate = _table[h];
                        forward_hash = lz4_hash5(forward);
                        const uint32_t here = uint32_t(ip - base);
                        _table[h] = here;
                        if (candidate >= low_index && here - candidate <= Lz4MaxDistance) {
                            match = base + candidate;
                            if (lz4_read32(match) == lz4_read32(ip)) {
                                break;
                            }
                        }
                    }
                }
                // back over the literals while the bytes before agree
                while (ip > anchor && match > low && ip[-1] == match[-1]) {
                    --ip;
                    --match;
                }
                for (;;) {
                    // the match's length, and the sequence
                    const size_t ml = Lz4MinMatch + lz4_count(ip + Lz4MinMatch, match + Lz4MinMatch, matchlimit);
                    op = lz4_put_sequence(op, anchor, ip, size_t(ip - match), ml);
                    ip += ml;
                    anchor = ip;
                    if (ip > mflimit) {
                        return lz4_put_sequence(op, anchor, iend, 0, 0);
                    }
                    // the position two back, and a match right here
                    _table[lz4_hash5(ip - 2)] = uint32_t(ip - 2 - base);
                    const uint32_t h = lz4_hash5(ip);
                    const uint32_t candidate = _table[h];
                    const uint32_t here = uint32_t(ip - base);
                    _table[h] = here;
                    if (candidate >= low_index && here - candidate <= Lz4MaxDistance &&
                        lz4_read32(base + candidate) == lz4_read32(ip)) {
                        match = base + candidate;
                        continue;
                    }
                    break;
                }
                forward_hash = lz4_hash5(++ip);
            }
        }

    private:
        uint32_t _table[size_t(1) << TableBits];
    };

    // The middle level (2): no chains, two tables of 2^17 entries, the last
    // position under a hash of eight bytes and of four, both asked at every
    // position, the longer match taken and extended backwards; of a match
    // every fifth position and the one two before its end go into the
    // tables. Between 1 and the chains: more found than by one probe, much
    // less walked than a chain
    class Lz4Mid {
    public:
        static constexpr unsigned TableBits = 17;

        Lz4Mid() noexcept
        : _long(new uint32_t[size_t(1) << TableBits])
        , _short(new uint32_t[size_t(1) << TableBits]) {
            reset();
        }

        void reset() noexcept {
            std::fill(_long.get(), _long.get() + (size_t(1) << TableBits), 0u);
            std::fill(_short.get(), _short.get() + (size_t(1) << TableBits), 0u);
        }

        void shift(uint32_t delta) noexcept {
            for (size_t i = 0; i < (size_t(1) << TableBits); ++i) {
                _long[i] = _long[i] > delta ? _long[i] - delta : 0;
                _short[i] = _short[i] > delta ? _short[i] - delta : 0;
            }
        }

        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            for (const uint8_t* p = from; p + 8 <= to; ++p) {
                _put(base, p);
            }
        }

        void copy_from(const Lz4Mid& o) noexcept {
            std::copy(o._long.get(), o._long.get() + (size_t(1) << TableBits), _long.get());
            std::copy(o._short.get(), o._short.get() + (size_t(1) << TableBits), _short.get());
        }

        uint8_t* compress(const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, uint8_t* dst) noexcept {
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            uint8_t* op = dst;
            if (n < Lz4MatchFindLimit + 1) {
                return lz4_put_sequence(op, anchor, iend, 0, 0);
            }
            const uint8_t* const mflimit = iend - Lz4MatchFindLimit;
            const uint8_t* const matchlimit = iend - Lz4LastLiterals;
            const uint32_t low_index = uint32_t(low - base);
            while (ip <= mflimit) {
                const uint32_t here = uint32_t(ip - base);
                const uint32_t hl = _hash8(ip);
                const uint32_t hs = _hash4(ip);
                const uint32_t cl = _long[hl];
                const uint32_t cs = _short[hs];
                _long[hl] = here;
                _short[hs] = here;
                const uint32_t word = lz4_read32(ip);
                size_t len = 0;
                uint32_t candidate = 0;
                if (cl >= low_index && here - cl - 1 < Lz4MaxDistance && lz4_read32(base + cl) == word) {
                    len = Lz4MinMatch + lz4_count(ip + Lz4MinMatch, base + cl + Lz4MinMatch, matchlimit);
                    candidate = cl;
                }
                if (cs >= low_index && here - cs - 1 < Lz4MaxDistance && cs != cl && lz4_read32(base + cs) == word) {
                    const size_t l = Lz4MinMatch + lz4_count(ip + Lz4MinMatch, base + cs + Lz4MinMatch, matchlimit);
                    if (l > len) {
                        len = l;
                        candidate = cs;
                    }
                }
                if (!len) {
                    ip += 1 + (size_t(ip - anchor) >> 7);   // a long run of misses steps faster
                    continue;
                }
                const uint8_t* match = base + candidate;
                while (ip > anchor && match > low && ip[-1] == match[-1]) {
                    --ip;
                    --match;
                    ++len;
                }
                op = lz4_put_sequence(op, anchor, ip, size_t(ip - match), len);
                const uint8_t* end = ip + len;
                // every fifth position of the match into the tables
                for (const uint8_t* q = ip + 1; q < end && q + 8 <= iend; q += 5) {
                    _put(base, q);
                }
                if (end - 2 <= mflimit) {
                    _put(base, end - 2);
                }
                ip = end;
                anchor = ip;
            }
            return lz4_put_sequence(op, anchor, iend, 0, 0);
        }

    private:
        SGCL_INLINE_HOT static uint32_t _hash8(const uint8_t* p) noexcept {
            return uint32_t((lz4_read64(p) * 0xCF1BBCDCB7A56463ull) >> (64 - TableBits));
        }

        SGCL_INLINE_HOT static uint32_t _hash4(const uint8_t* p) noexcept {
            return (lz4_read32(p) * 2654435761u) >> (32 - TableBits);
        }

        SGCL_INLINE_HOT void _put(const uint8_t* base, const uint8_t* p) noexcept {
            const uint32_t i = uint32_t(p - base);
            _long[_hash8(p)] = i;
            _short[_hash4(p)] = i;
        }

        std::unique_ptr<uint32_t[]> _long;
        std::unique_ptr<uint32_t[]> _short;
    };

    // The hash of four bytes for the chains
    SGCL_INLINE_HOT uint32_t lz4_hash4(const uint8_t* p) noexcept {
        return (lz4_read32(p) * 2654435761u) >> (32 - 15);
    }

    // The hash-chain compressors (3..9) and the optimal parser (10..12)
    class Lz4Hc {
    public:
        static constexpr unsigned HashBits = 15;
        static constexpr size_t OptNum = 4096;   // the optimal parser's stretch

        Lz4Hc() noexcept
        : _head(new uint32_t[size_t(1) << HashBits])
        , _chain(new uint16_t[Lz4Window]) {
            reset();
        }

        void reset() noexcept {
            std::fill(_head.get(), _head.get() + (size_t(1) << HashBits), 0u);
            std::fill(_chain.get(), _chain.get() + Lz4Window, uint16_t(0xFFFF));
            _next = 0;
        }

        // The positions of [from, to) (a dictionary) into the chains
        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            _base = base;
            _low = uint32_t(from - base);
            _next = _low;
            if (to - from >= ptrdiff_t(Lz4MinMatch)) {
                _insert(to - Lz4MinMatch + 1);
            }
        }

        void copy_from(const Lz4Hc& o) noexcept {
            std::copy(o._head.get(), o._head.get() + (size_t(1) << HashBits), _head.get());
            std::copy(o._chain.get(), o._chain.get() + Lz4Window, _chain.get());
            _next = o._next;
            _low = o._low;
        }

        void shift(uint32_t delta) noexcept {
            for (size_t i = 0; i < (size_t(1) << HashBits); ++i) {
                _head[i] = _head[i] > delta ? _head[i] - delta : 0;
            }
            _next = _next > delta ? _next - delta : 0;
        }

        // Compresses [src, src + n) at the level (2..12), the history from
        // low on; dst has lz4_bound(n) bytes
        uint8_t* compress(const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, uint8_t* dst, int level) noexcept {
            _base = base;
            _low = uint32_t(low - base);
            if (_next < _low) {
                _next = _low;
            }
            if (level >= 10) {
                static constexpr unsigned attempts[3] = {96, 512, 16384};
                static constexpr size_t sufficient[3] = {64, 128, OptNum};
                return _optimal(src, n, dst, attempts[level - 10], sufficient[level - 10], level == 12);
            }
            const unsigned attempts = level <= 2 ? 2u : 1u << (level - 1);
            return _ahead(src, n, dst, attempts);
        }

    private:
        struct Match {
            uint32_t length = 0;
            uint32_t offset = 0;
        };

        // Every position before p into the chains: each links to the last
        // position of its hash, as far back as 65534 (a farther one, or one
        // before the window, the end of the chain: the walk's floor stops it)
        SGCL_INLINE_HOT void _insert(const uint8_t* p) noexcept {
            const uint32_t target = uint32_t(p - _base);
            uint32_t* const head = _head.get();
            uint16_t* const chain = _chain.get();
            for (uint32_t idx = _next; idx < target; ++idx) {
                const uint32_t h = lz4_hash4(_base + idx);
                chain[idx & 0xFFFF] = uint16_t(std::min<uint32_t>(idx - head[h], 0xFFFF));
                head[h] = idx;
            }
            if (target > _next) {
                _next = target;
            }
        }

        // The longest match for ip within [ip, limit), better than `best`.
        // Deep (the optimal parser's levels): once a match of eight bytes is
        // found, the walk goes on along the chain of the four bytes of that
        // match whose link reaches farthest back: a longer match holds
        // those four bytes too, at the same distance from its start, so
        // only the positions of that chain can be one, and the walk skips
        // the rest; where those four bytes occur nowhere before, nothing
        // longer can be found and the walk stops
        template<bool Deep = false>
        Match _longest(const uint8_t* ip, const uint8_t* limit, unsigned attempts, Match best) noexcept {
            _insert(ip);
            const uint32_t here = uint32_t(ip - _base);
            const uint32_t floor = here > Lz4MaxDistance + _low ? here - uint32_t(Lz4MaxDistance) : _low;
            const size_t room = size_t(limit - ip);
            if (best.length >= room) {   // nothing longer fits
                return best;
            }
            const uint32_t first = lz4_read32(ip);
            const uint32_t span = here - floor;        // a candidate c is one when here - c is 1..span
            uint32_t pivot = 0;                        // the bytes of the match the walk follows, from its start
            uint32_t cursor = _head[lz4_hash4(ip)];    // a candidate's start plus pivot
            // a run of one byte at ip (spaces, zeros): its length, 0 for none
            const bool run = (first & 0xFFFF) == (first >> 16) && (first & 0xFF) == ((first >> 8) & 0xFF);
            size_t run_length = 0;
            if (run) {
                run_length = Lz4MinMatch;
                while (run_length < room && ip[run_length] == ip[0]) {
                    ++run_length;
                }
            }
            while (attempts--) {
                // one test for a candidate before the floor, at or past here,
                // or wrapped below 0 by a link that reaches past the window
                const uint32_t candidate = cursor - pivot;
                if (uint32_t(here - 1 - candidate) >= span) {
                    break;
                }
                const uint8_t* m = _base + candidate;
                if (m[best.length] == ip[best.length] && lz4_read32(m) == first) {
                    const size_t len = Lz4MinMatch + lz4_count(ip + Lz4MinMatch, m + Lz4MinMatch, limit);
                    if (len > best.length) {
                        best = {uint32_t(len), here - candidate};
                        if (len == room) {
                            break;
                        }
                        if constexpr (Deep) {
                            if (len >= 8) {
                                uint32_t farthest = 0;
                                uint32_t at = 0;
                                for (uint32_t k = 0; k + Lz4MinMatch <= len; ++k) {
                                    const uint16_t d = _chain[(candidate + k) & 0xFFFF];
                                    if (d == 0xFFFF) {
                                        return best;
                                    }
                                    if (d > farthest) {
                                        farthest = d;
                                        at = k;
                                    }
                                }
                                pivot = at;
                                cursor = candidate + at;
                            }
                        }
                    }
                }
                uint16_t delta = _chain[cursor & 0xFFFF];
                if (delta == 1 && run && pivot == 0) {
                    // the chain walks back through a run of the same byte a
                    // position at a time: the run's positions differ only in
                    // how much of it is left, and the one that leaves exactly
                    // ip's run may go on past it; that one is tried, and the
                    // walk goes on from the run's start
                    const uint8_t b = ip[0];
                    uint32_t start = candidate;
                    while (start > floor && _base[start - 1] == b) {
                        --start;
                    }
                    uint32_t end = candidate;
                    while (end < here && _base[end] == b) {
                        ++end;
                    }
                    if (end - start >= run_length && end - uint32_t(run_length) < candidate) {
                        const uint32_t t = end - uint32_t(run_length);
                        const uint8_t* tm = _base + t;
                        const size_t len = Lz4MinMatch + lz4_count(ip + Lz4MinMatch, tm + Lz4MinMatch, limit);
                        if (len > best.length) {
                            best = {uint32_t(len), here - t};
                            if (len == room) {
                                break;
                            }
                        }
                    }
                    cursor = start;
                    delta = _chain[cursor & 0xFFFF];
                }
                cursor -= delta;   // a link of 0xFFFF reaches past the floor: the test above ends the walk
            }
            return best;
        }

        struct Wide {
            const uint8_t* start = nullptr;
            uint32_t length = 0;
            uint32_t offset = 0;
        };

        // The match through q that is longest counted from as far back as
        // it agrees, but not before `lower`: a match found near the end of
        // the one before may start inside it and reach farther. Only one
        // longer than `longer` is returned (offset 0: none)
        Wide _wider(const uint8_t* q, const uint8_t* lower, const uint8_t* limit, unsigned attempts, uint32_t longer) noexcept {
            _insert(q);
            Wide best;
            best.length = longer;
            const uint32_t here = uint32_t(q - _base);
            uint32_t candidate = _head[lz4_hash4(q)];
            const uint32_t floor = here > Lz4MaxDistance + _low ? here - uint32_t(Lz4MaxDistance) : _low;
            const uint32_t first = lz4_read32(q);
            const uint8_t* const window = _base + _low;
            const size_t back_room = size_t(q - lower);
            // a longer match begins at lower or after and so covers the byte
            // at lower + longer: a candidate that does not agree there is
            // passed over at once
            const uint8_t* const must = lower + longer;
            if (must >= limit) {
                return best;
            }
            ptrdiff_t at = must - q;
            const uint32_t span = here - floor;   // a candidate c is one when here - c is 1..span
            while (attempts--) {
                if (uint32_t(here - 1 - candidate) >= span) {   // before the floor, or wrapped by a link past the window
                    break;
                }
                const uint8_t* m = _base + candidate;
                if (m[at] == q[at] && lz4_read32(m) == first) {
                    const size_t forward = Lz4MinMatch + lz4_count(q + Lz4MinMatch, m + Lz4MinMatch, limit);
                    const size_t back = lz4_count_back(q, m, std::min(back_room, size_t(m - window)));
                    if (forward + back > best.length) {
                        best = {q - back, uint32_t(forward + back), here - candidate};
                        if (lower + best.length >= limit) {
                            break;
                        }
                        at = lower + best.length - q;   // a longer one still must reach one byte further
                    }
                }
                candidate -= _chain[candidate & 0xFFFF];
            }
            return best;
        }

        // The parse of the hash-chain levels, two matches ahead. The match
        // found here (A) is not written at once: near its end a match that
        // reaches farther is looked for (B, which may begin inside A, as far
        // back as its bytes agree), and near B's end another (C). Then:
        //   - B begins less than three bytes after A: A is not worth a
        //     sequence of its own and B takes its place;
        //   - B begins within 18 bytes of A: A keeps up to 18 bytes (a
        //     length the token holds by itself) and B begins where A ends,
        //     as long as B keeps four bytes and its end;
        //   - C begins less than three bytes after A's end: B is left out
        //     when C begins at or past A's end (A is written, C goes on as
        //     the new A, B kept as a fallback should C's own search undo
        //     it); otherwise C replaces B;
        //   - else A is written, cut where B begins, and B and C move up.
        // With no B, A is written whole; with no C, A (cut) and B are.
        uint8_t* _ahead(const uint8_t* src, size_t n, uint8_t* dst, unsigned attempts) noexcept {
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            uint8_t* op = dst;
            if (n < Lz4MatchFindLimit + 1) {
                return lz4_put_sequence(op, anchor, iend, 0, 0);
            }
            const uint8_t* const mflimit = iend - Lz4MatchFindLimit;
            const uint8_t* const matchlimit = iend - Lz4LastLiterals;
            constexpr ptrdiff_t TokenLength = 18;   // the longest match the token holds alone
            auto end_of = [](const Wide& w) noexcept {
                return w.start + w.length;
            };
            auto put = [&](const Wide& w) noexcept {
                op = lz4_put_sequence(op, anchor, w.start, w.offset, w.length);
                anchor = end_of(w);
            };
            // Cuts the front of b so that it begins where a, kept up to
            // TokenLength bytes, ends (a shortened when b would lose its
            // last four bytes)
            auto share_token = [&](Wide& a, Wide& b) noexcept {
                ptrdiff_t keep = std::min<ptrdiff_t>(a.length, TokenLength);
                if (a.start + keep > end_of(b) - Lz4MinMatch) {
                    keep = (b.start - a.start) + ptrdiff_t(b.length) - ptrdiff_t(Lz4MinMatch);
                }
                const ptrdiff_t cut = keep - (b.start - a.start);
                if (cut > 0) {
                    b.start += cut;
                    b.length -= uint32_t(cut);
                }
                return keep;
            };
            const uint8_t* ip = src;
            while (ip <= mflimit) {
                const Match found = _longest(ip, matchlimit, attempts, Match {});
                if (found.length < Lz4MinMatch) {
                    ++ip;
                    continue;
                }
                Wide a {ip, found.length, found.offset};
                Wide fallback = a;   // A as first found, should it be replaced and B then not reach past it
                Wide b, c;
                bool have_b = false;
                for (;;) {
                    if (!have_b) {
                        b = end_of(a) <= mflimit ? _wider(end_of(a) - 2, a.start, matchlimit, attempts, a.length) : Wide {};
                        if (!b.offset) {
                            put(a);
                            break;
                        }
                        if (fallback.start < a.start && b.start < end_of(fallback)) {
                            a = fallback;
                        }
                        if (b.start - a.start < 3) {
                            a = b;
                            continue;
                        }
                        have_b = true;
                    }
                    if (b.start - a.start < TokenLength) {
                        share_token(a, b);
                    }
                    c = end_of(b) <= mflimit ? _wider(end_of(b) - 3, b.start, matchlimit, attempts, b.length) : Wide {};
                    if (!c.offset) {
                        if (b.start < end_of(a)) {
                            a.length = uint32_t(b.start - a.start);
                        }
                        put(a);
                        put(b);
                        break;
                    }
                    if (c.start < end_of(a) + 3) {
                        if (c.start >= end_of(a)) {
                            if (b.start < end_of(a)) {
                                const ptrdiff_t cut = end_of(a) - b.start;
                                b.start += cut;
                                b.length -= uint32_t(cut);
                                if (b.length < Lz4MinMatch) {
                                    b = c;
                                }
                            }
                            put(a);
                            a = c;
                            fallback = b;
                            have_b = false;
                            continue;
                        }
                        b = c;
                        continue;
                    }
                    if (b.start < end_of(a)) {
                        if (b.start - a.start < TokenLength) {
                            a.length = uint32_t(share_token(a, b));
                        } else {
                            a.length = uint32_t(b.start - a.start);
                        }
                    }
                    put(a);
                    a = b;
                    b = c;
                }
                ip = anchor;
            }
            return lz4_put_sequence(op, anchor, iend, 0, 0);
        }

        // The price in bytes of a run of n literals (their length bytes
        // and themselves) and of a match of ml bytes (the token, the
        // offset, the length bytes)
        SGCL_INLINE_HOT static uint32_t _literals_price(size_t n) noexcept {
            return uint32_t(n + (n >= 15 ? 1 + (n - 15) / 255 : 0));
        }

        SGCL_INLINE_HOT static uint32_t _match_price(size_t ml) noexcept {
            const size_t extra = ml - Lz4MinMatch;
            return uint32_t(1 + 2 + (extra >= 15 ? 1 + (extra - 15) / 255 : 0));
        }

        struct Node {
            uint32_t price;
            uint32_t length;     // of the match that ends here (0: a literal)
            uint32_t offset;
            uint32_t literals;   // the literals before this position since the last match
        };

        struct Step {
            uint32_t end;        // where the match ends, from the stretch's start
            uint32_t length;
            uint32_t offset;
        };

        // The optimal parser: stretches of up to OptNum positions, each the
        // cheapest path through its literals and matches. A match of
        // `sufficient` bytes or more, or one that reaches past the
        // stretch, ends it and is taken as it is.
        uint8_t* _optimal(const uint8_t* src, size_t n, uint8_t* dst, unsigned attempts, size_t sufficient, bool full) noexcept {
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            uint8_t* op = dst;
            if (n < Lz4MatchFindLimit + 1) {
                return lz4_put_sequence(op, anchor, iend, 0, 0);
            }
            if (!_nodes) {
                _nodes.reset(new Node[OptNum + 1]);
                _steps.reset(new Step[OptNum + 1]);
            }
            Node* const opt = _nodes.get();
            Step* const steps = _steps.get();
            const uint8_t* const mflimit = iend - Lz4MatchFindLimit;
            const uint8_t* const matchlimit = iend - Lz4LastLiterals;
            while (ip <= mflimit) {
                const Match first = _longest<true>(ip, matchlimit, attempts, Match {});
                if (first.length < Lz4MinMatch) {
                    ++ip;
                    continue;
                }
                if (first.length >= sufficient) {
                    op = lz4_put_sequence(op, anchor, ip, first.offset, first.length);
                    ip += first.length;
                    anchor = ip;
                    continue;
                }
                // opt[i]: the cheapest way to ip + i, priced from ip (the
                // literals pending since anchor go on as a run)
                size_t last = 0;
                opt[0] = {0, 0, 0, uint32_t(ip - anchor)};
                auto price_matches = [&](size_t at, const Match& m, size_t shortest = Lz4MinMatch) noexcept {
                    if (at + m.length > last) {
                        for (size_t k = last + 1; k <= at + m.length; ++k) {
                            opt[k] = {UINT32_MAX, 0, 0, 0};
                        }
                        last = at + m.length;
                    }
                    const uint32_t from = opt[at].price;
                    for (size_t ml = shortest; ml <= m.length; ++ml) {
                        const uint32_t p = from + _match_price(ml);
                        if (p < opt[at + ml].price) {
                            opt[at + ml] = {p, uint32_t(ml), m.offset, 0};
                        }
                    }
                };
                price_matches(0, first);
                size_t end = 0;
                Match forced;
                for (size_t cur = 1;; ++cur) {
                    if (cur > last) {
                        end = last;
                        break;
                    }
                    // a literal from cur - 1: one byte, and one more where
                    // the run's length takes another byte (at 15, and every
                    // 255 after)
                    const Node& prev = opt[cur - 1];
                    const uint32_t lits = prev.literals + 1;
                    const uint32_t lp = prev.price + 1 + (lits >= 15 && (lits - 15) % 255 == 0);
                    if (lp < opt[cur].price) {
                        opt[cur] = {lp, 0, 0, lits};
                    }
                    const uint8_t* p = ip + cur;
                    if (p > mflimit) {
                        continue;
                    }
                    // no search where the next position costs no more
                    // (inside a long match), and, but for the fullest
                    // level, only for a match reaching past the farthest
                    // position priced
                    if (cur + 1 <= last && opt[cur + 1].price <= opt[cur].price &&
                        (!full || (cur + Lz4MinMatch <= last && opt[cur + Lz4MinMatch].price < opt[cur].price + 3))) {
                        continue;
                    }
                    const uint32_t at_least = full ? 0 : uint32_t(last > cur ? last - cur : 0);
                    const Match m = _longest<true>(p, matchlimit, attempts, Match {at_least, 0});
                    if (m.length < Lz4MinMatch || !m.offset) {
                        continue;
                    }
                    if (m.length >= sufficient || cur + m.length >= OptNum) {
                        end = cur;
                        forced = m;
                        break;
                    }
                    price_matches(cur, m);
                    // the same match from as far back as it agrees: found
                    // here through another chain than the one of its start
                    size_t back = 0;
                    const uint8_t* mp = p - m.offset;
                    const size_t back_limit = std::min(cur, size_t(mp - (_base + _low)));
                    while (back < back_limit && p[-1 - ptrdiff_t(back)] == mp[-1 - ptrdiff_t(back)]) {
                        ++back;
                    }
                    if (back) {
                        price_matches(cur - back, Match {uint32_t(m.length + back), m.offset}, back + 1);
                    }
                }
                // the path back from end, then forwards
                size_t count = 0;
                for (size_t at = end; at > 0;) {
                    if (opt[at].length) {
                        steps[count++] = {uint32_t(at), opt[at].length, opt[at].offset};
                        at -= opt[at].length;
                    } else {
                        --at;
                    }
                }
                while (count) {
                    const Step& s = steps[--count];
                    const uint8_t* match_end = ip + s.end;
                    op = lz4_put_sequence(op, anchor, match_end - s.length, s.offset, s.length);
                    anchor = match_end;
                }
                if (forced.length) {
                    op = lz4_put_sequence(op, anchor, ip + end, forced.offset, forced.length);
                    anchor = ip + end + forced.length;
                    ip = anchor;
                } else {
                    ip += end;
                }
            }
            return lz4_put_sequence(op, anchor, iend, 0, 0);
        }

        const uint8_t* _base = nullptr;
        uint32_t _low = 0;
        uint32_t _next = 0;   // the first index not yet in the chains
        std::unique_ptr<uint32_t[]> _head;
        std::unique_ptr<uint16_t[]> _chain;
        std::unique_ptr<Node[]> _nodes;
        std::unique_ptr<Step[]> _steps;
    };
}
