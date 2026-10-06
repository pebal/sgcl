//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec_stream.h"
#include "lz4_block.h"
#include "zstd_entropy.h"
#include "../../core/root_ptr.h"
#include "../../core/tracked_ptr.h"
#include "../../hash/xxhash.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Zstandard's decoder (RFC 8878, section 3). A frame: the magic, a header
// (the content size, a single segment or a window, a dictionary id, a
// content checksum), blocks of up to 128 KB — raw, one byte repeated, or
// compressed — and the low 32 bits of XXH64 of the content. A compressed
// block is its literals (raw, repeated, or Huffman-coded in one stream or
// four, with a table of their own or the one before) and its sequences: a
// count, the modes of three FSE codes (predefined, one symbol, a table, the
// table before) and one backward bitstream from which every sequence takes
// its literal length, match length and offset code with their extra bits.
// An offset value of 1 to 3 names one of the three offsets used last (the
// literals' length of 0 shifting which), a larger one a new offset.
//
// The sequences are decoded and carried out in one loop: the literals
// copied from the literals' buffer and the match from the history, 16 or
// 32 bytes at a time where both buffers have the room (the output has
// ZstdOutSlack bytes past its end, the literals' buffer as many past its).
namespace sgcl::compress::detail {
    inline constexpr uint32_t ZstdMagic = 0xFD2FB528u;
    inline constexpr uint32_t ZstdDictionaryMagic = 0xEC30A437u;
    inline constexpr uint32_t ZstdSkippableMagic = 0x184D2A50u;   // to 0x184D2A5F
    inline constexpr size_t ZstdBlockMax = size_t(128) << 10;
    inline constexpr size_t ZstdOutSlack = 32;

    SGCL_INLINE_HOT uint32_t zstd_le32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    // The codes of the literal lengths and the match lengths: a base and
    // the extra bits read after it (RFC 8878, 3.1.1.3.2.1.1)
    inline constexpr uint32_t ZstdLiteralBase[36] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 128, 256, 512, 1024,
        2048, 4096, 8192, 16384, 32768, 65536};
    inline constexpr uint8_t ZstdLiteralBits[36] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    inline constexpr uint32_t ZstdMatchBase[53] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 37, 39, 41, 43, 47, 51, 59, 67, 83, 99, 131, 259, 515, 1027, 2051, 4099, 8195, 16387, 32771, 65539};
    inline constexpr uint8_t ZstdMatchBits[53] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    // The predefined distributions (RFC 8878, 3.1.1.3.2.2)
    inline constexpr int16_t ZstdLiteralDefault[36] = {
        4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1};
    inline constexpr int16_t ZstdMatchDefault[53] = {
        1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1};
    inline constexpr int16_t ZstdOffsetDefault[29] = {
        1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1};

    enum class ZstdCode : uint8_t {
        literal,
        offset,
        match
    };

    // An entry of a sequence code's decoding table, eight bytes: the code's
    // base value and extra bits, the new state's base and the bits read to it
    struct ZstdSeqEntry {
        uint32_t value;   // the code's base value
        uint16_t next;    // the new state's base
        uint8_t bits;     // the bits read to it
        uint8_t extra;    // the code's extra bits
    };

    static_assert(sizeof(ZstdSeqEntry) == 8);

    // An entry as one word, little-endian: the value in the low 32 bits,
    // the next state's base in the 16 above, then the bits, the extra bits
    SGCL_INLINE_HOT uint64_t zstd_entry_word(const ZstdSeqEntry* e) noexcept {
        uint64_t w;
        std::memcpy(&w, e, 8);
        if constexpr (std::endian::native == std::endian::big) {
            w = uint64_t(e->value) | uint64_t(e->next) << 32 | uint64_t(e->bits) << 48 | uint64_t(e->extra) << 56;
        }
        return w;
    }

    // A decoding table of one of the three codes: the FSE table (its symbols
    // for the encoder's use of a dictionary's tables) and the decoder's
    // entries with the base and the extra bits of each code
    struct ZstdSeqTable {
        FseEntry entry[512];
        ZstdSeqEntry seq[512];
        unsigned log = 0;
        bool valid = false;
        bool predefined = false;   // the predefined table in use, entry left as it was (no copy)
    };

    // The decoder's entries of a table built from counts
    inline void zstd_fill_values(ZstdSeqTable& t, ZstdCode kind) noexcept {
        const unsigned size = 1u << t.log;
        for (unsigned u = 0; u < size; ++u) {
            const FseEntry& f = t.entry[u];
            const unsigned s = f.symbol;
            ZstdSeqEntry& e = t.seq[u];
            e.next = f.next;
            e.bits = f.bits;
            switch (kind) {
                case ZstdCode::literal:
                    e.extra = ZstdLiteralBits[s];
                    e.value = ZstdLiteralBase[s];
                    break;
                case ZstdCode::match:
                    e.extra = ZstdMatchBits[s];
                    e.value = ZstdMatchBase[s];
                    break;
                case ZstdCode::offset:
                    e.extra = uint8_t(s);
                    e.value = uint32_t(1) << s;   // offset values: 2^code plus the code's bits
                    break;
            }
        }
    }

    inline constexpr unsigned zstd_max_symbol(ZstdCode kind) noexcept {
        return kind == ZstdCode::literal ? 35 : kind == ZstdCode::match ? 52 : 31;
    }

    inline constexpr unsigned zstd_max_log(ZstdCode kind) noexcept {
        return kind == ZstdCode::offset ? 8 : 9;
    }

    // The offsets' codes as the other two have theirs: the base 2^code, the
    // code's own number of extra bits
    inline constexpr auto ZstdOffsetTables = [] {
        struct {
            uint32_t base[32];
            uint8_t bits[32];
        } t {};
        for (unsigned s = 0; s < 32; ++s) {
            t.base[s] = uint32_t(1) << s;
            t.bits[s] = uint8_t(s);
        }
        return t;
    }();

    // The decoder's entries straight from counts (the FSE spread of the
    // symbols, then each state's bits and next base with its code's value
    // and extra bits, in one pass); the FSE entries are left as they were
    inline bool zstd_build_seq(ZstdSeqTable& t, const FseCounts& c, ZstdCode kind) noexcept {
        const unsigned log = c.log;
        const unsigned size = 1u << log;
        const unsigned mask = size - 1;
        unsigned high = size - 1;
        uint8_t spread[512];
        uint16_t next[64];
        const unsigned symbols = c.symbols;
        if (symbols > 64) {
            return false;
        }
        for (unsigned s = 0; s < symbols; ++s) {
            if (c.count[s] == -1) {
                spread[high--] = uint8_t(s);
                next[s] = 1;
            } else {
                next[s] = uint16_t(c.count[s]);
            }
        }
        const unsigned step = (size >> 1) + (size >> 3) + 3;
        unsigned position = 0;
        if (high == size - 1) {
            // no "less than one": every step lands in the table
            for (unsigned s = 0; s < symbols; ++s) {
                for (int i = 0; i < c.count[s]; ++i) {
                    spread[position] = uint8_t(s);
                    position = (position + step) & mask;
                }
            }
        } else {
            for (unsigned s = 0; s < symbols; ++s) {
                for (int i = 0; i < c.count[s]; ++i) {
                    spread[position] = uint8_t(s);
                    do {
                        position = (position + step) & mask;
                    } while (position > high);
                }
            }
        }
        if (position != 0) {
            return false;   // the counts do not fill the table
        }
        const uint32_t* const base = kind == ZstdCode::literal ? ZstdLiteralBase : kind == ZstdCode::match ? ZstdMatchBase : ZstdOffsetTables.base;
        const uint8_t* const extra = kind == ZstdCode::literal ? ZstdLiteralBits : kind == ZstdCode::match ? ZstdMatchBits : ZstdOffsetTables.bits;
        for (unsigned u = 0; u < size; ++u) {
            const unsigned s = spread[u];
            const unsigned x = next[s]++;
            const unsigned bits = log - (31 - unsigned(std::countl_zero(uint32_t(x))));
            t.seq[u] = ZstdSeqEntry {base[s], uint16_t((x << bits) - size), uint8_t(bits), extra[s]};
        }
        return true;
    }

    // A table from counts for the decoder alone (a block's)
    inline bool zstd_build_seq_from_counts(ZstdSeqTable& t, const FseCounts& c, ZstdCode kind) noexcept {
        t.log = c.log;
        t.predefined = false;
        t.valid = zstd_build_seq(t, c, kind);
        return t.valid;
    }

    // A table from counts with its FSE entries too (the predefined ones and
    // a dictionary's, which the encoder reads)
    inline bool zstd_build_from_counts(ZstdSeqTable& t, const FseCounts& c, ZstdCode kind) noexcept {
        t.log = c.log;
        t.predefined = false;
        t.valid = fse_build_decoder(c, t.entry) && zstd_build_seq(t, c, kind);
        return t.valid;
    }

    inline void zstd_build_default(ZstdSeqTable& t, ZstdCode kind) noexcept {
        FseCounts c;
        const int16_t* d = kind == ZstdCode::literal ? ZstdLiteralDefault : kind == ZstdCode::match ? ZstdMatchDefault : ZstdOffsetDefault;
        const unsigned n = kind == ZstdCode::literal ? 36 : kind == ZstdCode::match ? 53 : 29;
        for (unsigned i = 0; i < n; ++i) {
            c.count[i] = d[i];
        }
        c.symbols = n;
        c.log = kind == ZstdCode::offset ? 5 : 6;
        zstd_build_from_counts(t, c, kind);
    }

    // The three predefined tables, built once
    struct ZstdDefaults {
        ZstdSeqTable literal, offset, match;

        ZstdDefaults() noexcept {
            zstd_build_default(literal, ZstdCode::literal);
            zstd_build_default(offset, ZstdCode::offset);
            zstd_build_default(match, ZstdCode::match);
        }

        static const ZstdDefaults& get() noexcept {
            static const ZstdDefaults d;
            return d;
        }
    };

    // What one block leaves to the next, and a dictionary to the first: the
    // last Huffman table and the three last sequence tables (for the modes
    // that say "the one before"), and the three offsets used last
    struct ZstdEntropy {
        HufTable huf;
        ZstdSeqTable literal, offset, match;
        uint32_t rep[3] = {1, 4, 8};

        void reset() noexcept {
            huf.valid = false;
            literal.valid = offset.valid = match.valid = false;
            rep[0] = 1;
            rep[1] = 4;
            rep[2] = 8;
        }
    };

    enum class ZstdFail : uint8_t {
        none,
        corrupt,
        truncated,
        overflow,   // a block that makes more than the block's room
        offset      // a match before the data the frame may reach
    };

    struct ZstdBlockResult {
        ZstdFail status = ZstdFail::none;
        size_t written = 0;
        size_t at = 0;             // within the block's bytes
        const char* text = nullptr;
    };

    // Decodes the Huffman-coded literals of `regenerated` bytes from
    // [p, p + n) (one stream or four) into out
    inline bool zstd_huf_decode(const HufTable& t, const uint8_t* p, size_t n, bool four, uint8_t* out, size_t regenerated) noexcept {
        const unsigned max_bits = t.max_bits;
        auto stream = [&](const uint8_t* b, const uint8_t* e, uint8_t* o, size_t count) noexcept -> bool {
            ZstdBitReader r;
            if (!r.open(b, e)) {
                return false;
            }
            uint8_t* const end = o + count;
            // four symbols a refill while there are four to make
            while (end - o >= 4) {
                r.refill();
                for (int k = 0; k < 4; ++k) {
                    const HufEntry en = t.entry[r.peek(int(max_bits))];
                    r.skip(en.bits);
                    *o++ = en.symbol;
                }
            }
            while (o < end) {
                r.refill();
                const HufEntry en = t.entry[r.peek(int(max_bits))];
                r.skip(en.bits);
                *o++ = en.symbol;
            }
            return r.finished();
        };
        if (!four) {
            return stream(p, p + n, out, regenerated);
        }
        if (n < 10) {
            return false;
        }
        const size_t s1 = size_t(p[0]) | size_t(p[1]) << 8;
        const size_t s2 = size_t(p[2]) | size_t(p[3]) << 8;
        const size_t s3 = size_t(p[4]) | size_t(p[5]) << 8;
        if (6 + s1 + s2 + s3 > n) {
            return false;
        }
        const size_t s4 = n - 6 - s1 - s2 - s3;
        const size_t q = (regenerated + 3) / 4;
        if (3 * q > regenerated) {
            return false;
        }
        const uint8_t* b = p + 6;
        // the four streams side by side: four bit readers, one symbol from
        // each in turn, while each has four to make
        ZstdBitReader r[4];
        const uint8_t* starts[5] = {b, b + s1, b + s1 + s2, b + s1 + s2 + s3, b + s1 + s2 + s3 + s4};
        uint8_t* o[4] = {out, out + q, out + 2 * q, out + 3 * q};
        uint8_t* const ends[4] = {out + q, out + 2 * q, out + 3 * q, out + regenerated};
        for (int i = 0; i < 4; ++i) {
            if (!r[i].open(starts[i], starts[i + 1])) {
                return false;
            }
        }
        const size_t last = regenerated - 3 * q;
        size_t common = std::min(q, last);   // the symbols every stream has
        // while every stream has 8 bytes before its word: refills without
        // tests, and 4 symbols of 11 bits at most from each (44 bits, of
        // the 57 a refill leaves)
        while (common >= 4 && r[0].roomy8() && r[1].roomy8() && r[2].roomy8() && r[3].roomy8()) {
            for (int i = 0; i < 4; ++i) {
                r[i].refill_fast();
            }
            for (int k = 0; k < 4; ++k) {
                for (int i = 0; i < 4; ++i) {
                    const HufEntry en = t.entry[r[i].peek_fast(int(max_bits))];
                    r[i].skip(en.bits);
                    *o[i]++ = en.symbol;
                }
            }
            common -= 4;
        }
        while (common >= 4) {
            for (int i = 0; i < 4; ++i) {
                r[i].refill();
            }
            for (int k = 0; k < 4; ++k) {
                for (int i = 0; i < 4; ++i) {
                    const HufEntry en = t.entry[r[i].peek(int(max_bits))];
                    r[i].skip(en.bits);
                    *o[i]++ = en.symbol;
                }
            }
            common -= 4;
        }
        for (int i = 0; i < 4; ++i) {
            while (o[i] < ends[i]) {
                r[i].refill();
                const HufEntry en = t.entry[r[i].peek(int(max_bits))];
                r[i].skip(en.bits);
                *o[i]++ = en.symbol;
            }
            if (!r[i].finished()) {
                return false;
            }
        }
        return true;
    }

    // The literals section at p (n bytes of the block): decoded into lit
    // (ZstdBlockMax + ZstdOutSlack bytes), the table kept in e. Returns the
    // section's bytes, 0 on a failure (r says which)
    inline size_t zstd_literals(const uint8_t* p, size_t n, ZstdEntropy& e, uint8_t* lit, size_t& count, ZstdBlockResult& r) noexcept {
        if (n == 0) {
            r = {ZstdFail::truncated, 0, 0, "zstd: a block with no literals section"};
            return 0;
        }
        const unsigned type = p[0] & 3;
        const unsigned format = (p[0] >> 2) & 3;
        if (type < 2) {
            // raw or one byte repeated: a header of 1, 2 or 3 bytes
            size_t head, size;
            if ((format & 1) == 0) {
                head = 1;
                size = p[0] >> 3;
            } else if (format == 1) {
                if (n < 2) {
                    r = {ZstdFail::truncated, 0, n, "zstd: a literals header cut short"};
                    return 0;
                }
                head = 2;
                size = (p[0] >> 4) | size_t(p[1]) << 4;
            } else {
                if (n < 3) {
                    r = {ZstdFail::truncated, 0, n, "zstd: a literals header cut short"};
                    return 0;
                }
                head = 3;
                size = (p[0] >> 4) | size_t(p[1]) << 4 | size_t(p[2]) << 12;
            }
            if (size > ZstdBlockMax) {
                r = {ZstdFail::corrupt, 0, 0, "zstd: more literals than a block holds"};
                return 0;
            }
            if (type == 0) {
                if (n - head < size) {
                    r = {ZstdFail::truncated, 0, n, "zstd: raw literals cut short"};
                    return 0;
                }
                sgcl::detail::copy_bytes(lit, p + head, size);
                count = size;
                return head + size;
            }
            if (n - head < 1) {
                r = {ZstdFail::truncated, 0, n, "zstd: repeated literals cut short"};
                return 0;
            }
            sgcl::detail::fill_bytes(lit, p[head], size);
            count = size;
            return head + 1;
        }
        // compressed or treeless: 3, 4 or 5 bytes of header
        size_t head, regenerated, compressed;
        bool four = format != 0;
        if (format < 2) {
            if (n < 3) {
                r = {ZstdFail::truncated, 0, n, "zstd: a literals header cut short"};
                return 0;
            }
            head = 3;
            const uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16;
            regenerated = (v >> 4) & 0x3FF;
            compressed = (v >> 14) & 0x3FF;
        } else if (format == 2) {
            if (n < 4) {
                r = {ZstdFail::truncated, 0, n, "zstd: a literals header cut short"};
                return 0;
            }
            head = 4;
            const uint32_t v = zstd_le32(p);
            regenerated = (v >> 4) & 0x3FFF;
            compressed = (v >> 18) & 0x3FFF;
        } else {
            if (n < 5) {
                r = {ZstdFail::truncated, 0, n, "zstd: a literals header cut short"};
                return 0;
            }
            head = 5;
            const uint64_t v = uint64_t(zstd_le32(p)) | uint64_t(p[4]) << 32;
            regenerated = size_t((v >> 4) & 0x3FFFF);
            compressed = size_t((v >> 22) & 0x3FFFF);
        }
        if (regenerated > ZstdBlockMax) {
            r = {ZstdFail::corrupt, 0, 0, "zstd: more literals than a block holds"};
            return 0;
        }
        if (n - head < compressed) {
            r = {ZstdFail::truncated, 0, n, "zstd: compressed literals cut short"};
            return 0;
        }
        const uint8_t* q = p + head;
        size_t streams = compressed;
        if (type == 2) {
            const size_t t = huf_read_table(q, compressed, e.huf);
            if (!t) {
                e.huf.valid = false;
                r = {ZstdFail::corrupt, 0, head, "zstd: a Huffman table that is not valid"};
                return 0;
            }
            q += t;
            streams -= t;
        } else if (!e.huf.valid) {
            r = {ZstdFail::corrupt, 0, 0, "zstd: treeless literals with no table before"};
            return 0;
        }
        if (!zstd_huf_decode(e.huf, q, streams, four, lit, regenerated)) {
            r = {ZstdFail::corrupt, 0, head, "zstd: Huffman-coded literals that do not decode"};
            return 0;
        }
        count = regenerated;
        return head + compressed;
    }

    // A sequence table of the block from its mode at p (n bytes); the bytes
    // read, or SIZE_MAX on a failure
    inline size_t zstd_seq_table(unsigned mode, const uint8_t* p, size_t n, ZstdSeqTable& t, ZstdCode kind, const ZstdSeqTable& predefined) noexcept {
        switch (mode) {
            case 0:
                (void)predefined;
                t.valid = true;
                t.predefined = true;
                return 0;
            case 1: {
                t.predefined = false;
                if (n < 1 || p[0] > zstd_max_symbol(kind)) {
                    return SIZE_MAX;
                }
                t.log = 0;
                fse_build_rle(t.entry, p[0]);
                zstd_fill_values(t, kind);
                t.valid = true;
                return 1;
            }
            case 2: {
                t.predefined = false;
                FseCounts c;
                const size_t used = fse_read_counts(p, n, c, zstd_max_symbol(kind) + 1, zstd_max_log(kind));
                if (!used || !zstd_build_seq_from_counts(t, c, kind)) {
                    t.valid = false;
                    return SIZE_MAX;
                }
                return used;
            }
            default:
                return t.valid ? 0 : SIZE_MAX;   // the table before
        }
    }

    // A sequence as decoded: its literals' length, its match's length and
    // its offset (SIZE_MAX for a repeated offset of 0, which the check of
    // the offset then refuses)
    struct ZstdSeq {
        size_t ll;
        size_t ml;
        size_t offset;
    };

    // The decoding of the sequences of a block: the bitstream, the three
    // tables and their states, the three offsets used last. On the stack
    // and used through inlined members only, so that all of it may live in
    // registers
    struct ZstdSeqDecoder {
        ZstdBitReader bits;
        const ZstdSeqEntry* ll_t = nullptr;
        const ZstdSeqEntry* of_t = nullptr;
        const ZstdSeqEntry* ml_t = nullptr;
        unsigned ll_s = 0;
        unsigned of_s = 0;
        unsigned ml_s = 0;
        size_t rep0 = 0;
        size_t rep1 = 0;
        size_t rep2 = 0;

        template<bool Fast>
        SGCL_INLINE_HOT void refill() noexcept {
            if constexpr (Fast) {
                bits.refill_fast();
            } else {
                bits.refill();
            }
        }

        // The next sequence. Fast: 16 bytes of the stream left before the
        // word, so that the refills (three at most: the first moves back 8
        // bytes at most, the other two 4 each) need no tests. Last: no
        // states after it
        template<bool Fast, bool Last>
        SGCL_INLINE_HOT ZstdSeq next() noexcept {
            refill<Fast>();
            // each entry as one word (fewer registers than its four fields):
            // the value in the low 32 bits, then next, bits, extra
            const uint64_t le = zstd_entry_word(ll_t + ll_s);
            const uint64_t oe = zstd_entry_word(of_t + of_s);
            const uint64_t me = zstd_entry_word(ml_t + ml_s);
            const unsigned le_extra = unsigned(le >> 56);
            const unsigned oe_extra = unsigned(oe >> 56);
            const unsigned me_extra = unsigned(me >> 56);
            const unsigned le_bits = unsigned(le >> 48) & 0xFF;
            const unsigned oe_bits = unsigned(oe >> 48) & 0xFF;
            const unsigned me_bits = unsigned(me >> 48) & 0xFF;
            // a refill leaves 56 bits at least: a sequence's bits mostly
            // fit, and need no other (else one before the lengths and one
            // before the states)
            const unsigned state_bits = Last ? 0u : le_bits + me_bits + oe_bits;
            const bool one = oe_extra + me_extra + le_extra + state_bits <= 56;
            // the offset, its extra bits first
            size_t offset;
            const uint32_t v = uint32_t(oe) + uint32_t(bits.read(int(oe_extra)));
            if (oe_extra > 1) {
                offset = v - 3;
                rep2 = rep1;
                rep1 = rep0;
                rep0 = offset;
            } else {
                // a repeat: the value 1..3 (a code of 0 or 1), shifted by one
                // when the literals' length is 0 (only its code 0 has the
                // base 0)
                const unsigned index = v - 1 + unsigned(uint32_t(le) == 0);
                if (index == 0) {
                    offset = rep0;
                } else {
                    // values, not the members (a conditional of two
                    // members would select between their addresses)
                    const size_t r0 = rep0;
                    const size_t r1 = rep1;
                    const size_t r2 = rep2;
                    offset = index == 1 ? r1 : index == 2 ? r2 : r0 - 1;
                    offset -= offset == 0;   // no offset of 0: SIZE_MAX, which the check refuses
                    rep2 = index == 1 ? r2 : r1;
                    rep1 = r0;
                    rep0 = offset;
                }
            }
            if (!one) {
                refill<Fast>();
            }
            // the two lengths' extra bits in one read (32 at most), the
            // match's first; the three states' bits in one read likewise
            // (26 at most): fewer steps through the reader's position
            const uint32_t lengths = uint32_t(bits.read(int(me_extra + le_extra)));
            const size_t ml = uint32_t(me) + size_t(lengths >> le_extra);
            const size_t ll = uint32_t(le) + size_t(lengths & ((uint32_t(1) << le_extra) - 1));
            if constexpr (!Last) {
                if (!one) {
                    refill<Fast>();
                }
                const uint32_t states = uint32_t(bits.read(int(state_bits)));
                ll_s = (unsigned(le >> 32) & 0xFFFF) + (states >> (me_bits + oe_bits));
                ml_s = (unsigned(me >> 32) & 0xFFFF) + ((states >> oe_bits) & ((uint32_t(1) << me_bits) - 1));
                of_s = (unsigned(oe >> 32) & 0xFFFF) + (states & ((uint32_t(1) << oe_bits) - 1));
                // the next states made here, before the copies and their
                // branches (a compiler would rather make them after): a
                // mispredicted branch of the copies then leaves the next
                // sequence's start done
                __asm__ volatile("" : "+r"(ll_s), "+r"(ml_s), "+r"(of_s));
            }
            return {ll, ml, offset};
        }
    };

    // Carries out a sequence: its literals from lp, its match from the
    // history (from low on), both to op, 16 or 32 bytes at a time (both
    // buffers have ZstdOutSlack bytes past their ends). False, with nothing
    // done, when the literals run past lend, the output past dst_end or the
    // match before low
    SGCL_INLINE_HOT bool zstd_execute(const ZstdSeq& s, uint8_t*& op, const uint8_t*& lp, const uint8_t* lend, const uint8_t* dst_end,
                                      const uint8_t* low) noexcept {
        const size_t ll = s.ll;
        const size_t ml = s.ml;
        const size_t offset = s.offset;
        // the match's source asked for while the literals are copied (a
        // prefetch of any address is harmless: the checks come after)
        __builtin_prefetch(reinterpret_cast<const uint8_t*>(uintptr_t(op) + ll - offset));
        // the three checks in one test
        if ((ll > size_t(lend - lp)) | (ll + ml > size_t(dst_end - op)) | (offset > size_t(op - low) + ll)) {
            return false;
        }
        // the literals: 32 bytes whatever their length (most are shorter:
        // one branch less to mispredict), the rest 32 at a time
        lz4_copy16(op, lp);
        lz4_copy16(op + 16, lp + 16);
        if (ll > 32) [[unlikely]] {
            uint8_t* o = op + 32;
            const uint8_t* q = lp + 32;
            uint8_t* const e = op + ll;
            do {
                lz4_copy16(o, q);
                lz4_copy16(o + 16, q + 16);
                o += 32;
                q += 32;
            } while (o < e);
        }
        uint8_t* o = op + ll;
        lp += ll;
        // the match: from 16 back or further 32 bytes (16 and 16, the second
        // from bytes the first may have made), the rest 32 or 16 at a time;
        // nearer, as LZ4's
        if (offset >= 16) [[likely]] {
            const uint8_t* m = o - offset;
            lz4_copy16(o, m);
            lz4_copy16(o + 16, m + 16);
            if (ml > 32) [[unlikely]] {
                uint8_t* const e = o + ml;
                o += 32;
                m += 32;
                if (offset >= 32) {
                    do {
                        lz4_copy16(o, m);
                        lz4_copy16(o + 16, m + 16);
                        o += 32;
                        m += 32;
                    } while (o < e);
                } else {
                    do {
                        lz4_copy16(o, m);
                        o += 16;
                        m += 16;
                    } while (o < e);
                }
            }
            op += ll + ml;
        } else {
            op = lz4_copy_match(o, offset, ml);
        }
        return true;
    }

    // Decodes one compressed block [src, src + n) into [dst, dst_end), the
    // history from `low` on (the frame's data before the block, a
    // dictionary's content before that): no offset reaches before it
    inline ZstdBlockResult zstd_decode_block(const uint8_t* src, size_t n, uint8_t* dst, uint8_t* dst_end, const uint8_t* low, ZstdEntropy& e,
                                             uint8_t* lit) noexcept {
        ZstdBlockResult r;
        size_t lit_count = 0;
        const size_t lit_size = zstd_literals(src, n, e, lit, lit_count, r);
        if (!lit_size) {
            return r;
        }
        const uint8_t* p = src + lit_size;
        const uint8_t* const end = src + n;
        auto fail = [&](ZstdFail why, const uint8_t* at, const char* text) noexcept {
            return ZstdBlockResult {why, 0, size_t(at - src), text};
        };
        if (p >= end) {
            return fail(ZstdFail::truncated, p, "zstd: a block with no sequences section");
        }
        // the number of sequences
        size_t count = *p++;
        if (count >= 128) {
            if (count < 255) {
                if (p >= end) {
                    return fail(ZstdFail::truncated, p, "zstd: a sequences header cut short");
                }
                count = ((count - 128) << 8) + *p++;
            } else {
                if (end - p < 2) {
                    return fail(ZstdFail::truncated, p, "zstd: a sequences header cut short");
                }
                count = size_t(p[0]) + (size_t(p[1]) << 8) + 0x7F00;
                p += 2;
            }
        }
        uint8_t* op = dst;
        const uint8_t* lp = lit;
        const uint8_t* const lend = lit + lit_count;
        if (count == 0) {
            if (p != end) {
                return fail(ZstdFail::corrupt, p, "zstd: bytes after a block with no sequences");
            }
            if (lit_count > size_t(dst_end - dst)) {
                return fail(ZstdFail::overflow, src, "zstd: a block makes more than its room");
            }
            sgcl::detail::copy_bytes(op, lit, lit_count);
            return {ZstdFail::none, lit_count, 0, nullptr};
        }
        if (p >= end) {
            return fail(ZstdFail::truncated, p, "zstd: a sequences header cut short");
        }
        const unsigned modes = *p++;
        if (modes & 3) {
            return fail(ZstdFail::corrupt, p - 1, "zstd: reserved bits of the sequences' modes set");
        }
        const ZstdDefaults& defaults = ZstdDefaults::get();
        size_t used = zstd_seq_table(modes >> 6, p, size_t(end - p), e.literal, ZstdCode::literal, defaults.literal);
        if (used == SIZE_MAX) {
            return fail(ZstdFail::corrupt, p, "zstd: a literal lengths table that is not valid");
        }
        p += used;
        used = zstd_seq_table((modes >> 4) & 3, p, size_t(end - p), e.offset, ZstdCode::offset, defaults.offset);
        if (used == SIZE_MAX) {
            return fail(ZstdFail::corrupt, p, "zstd: an offsets table that is not valid");
        }
        p += used;
        used = zstd_seq_table((modes >> 2) & 3, p, size_t(end - p), e.match, ZstdCode::match, defaults.match);
        if (used == SIZE_MAX) {
            return fail(ZstdFail::corrupt, p, "zstd: a match lengths table that is not valid");
        }
        p += used;
        ZstdSeqDecoder d;
        if (!d.bits.open(p, end)) {
            return fail(ZstdFail::corrupt, p, "zstd: a sequences bitstream with no end mark");
        }
        const ZstdSeqTable& lt = e.literal.predefined ? defaults.literal : e.literal;
        const ZstdSeqTable& ot = e.offset.predefined ? defaults.offset : e.offset;
        const ZstdSeqTable& mt = e.match.predefined ? defaults.match : e.match;
        d.ll_t = lt.seq;
        d.of_t = ot.seq;
        d.ml_t = mt.seq;
        d.ll_s = unsigned(d.bits.read(int(lt.log)));
        d.of_s = unsigned(d.bits.read(int(ot.log)));
        d.ml_s = unsigned(d.bits.read(int(mt.log)));
        d.rep0 = e.rep[0];
        d.rep1 = e.rep[1];
        d.rep2 = e.rep[2];
        // which check refused a sequence (from values: nothing of the loop's
        // taken by reference)
        const size_t at = size_t(p - src);
        auto refused = [at](const ZstdSeq& s, size_t literals, size_t room) noexcept {
            if (s.ll > literals) {
                return ZstdBlockResult {ZstdFail::corrupt, 0, at, "zstd: a sequence takes more literals than there are"};
            }
            if (s.ll + s.ml > room) {
                return ZstdBlockResult {ZstdFail::overflow, 0, at, "zstd: a block makes more than its room"};
            }
            if (s.offset == SIZE_MAX) {
                return ZstdBlockResult {ZstdFail::corrupt, 0, at, "zstd: a repeated offset of 0"};
            }
            return ZstdBlockResult {ZstdFail::offset, 0, at, "zstd: a match before the data"};
        };
        // every sequence but the last, first while the stream has the room
        // for refills without tests, then with them; the last with no states
        const size_t body = count - 1;
        size_t k = 0;
        while (k < body && d.bits.roomy()) {
            const ZstdSeq s = d.next<true, false>();
            if (!zstd_execute(s, op, lp, lend, dst_end, low)) [[unlikely]] {
                return refused(s, size_t(lend - lp), size_t(dst_end - op));
            }
            ++k;
        }
        for (; k < body; ++k) {
            const ZstdSeq s = d.next<false, false>();
            if (!zstd_execute(s, op, lp, lend, dst_end, low)) [[unlikely]] {
                return refused(s, size_t(lend - lp), size_t(dst_end - op));
            }
        }
        const ZstdSeq s = d.next<false, true>();
        if (!zstd_execute(s, op, lp, lend, dst_end, low)) [[unlikely]] {
            return refused(s, size_t(lend - lp), size_t(dst_end - op));
        }
        if (d.bits.overflowed() || !d.bits.finished()) {
            return fail(ZstdFail::corrupt, p, "zstd: a sequences bitstream that does not end where it should");
        }
        // the literals after the last sequence
        const size_t tail = size_t(lend - lp);
        if (tail > size_t(dst_end - op)) {
            return fail(ZstdFail::overflow, p, "zstd: a block makes more than its room");
        }
        sgcl::detail::copy_bytes(op, lp, tail);
        op += tail;
        e.rep[0] = uint32_t(d.rep0);
        e.rep[1] = uint32_t(d.rep1);
        e.rep[2] = uint32_t(d.rep2);
        return {ZstdFail::none, size_t(op - dst), 0, nullptr};
    }

    // A frame's header, read
    struct ZstdFrameHeader {
        uint64_t content_size = 0;
        bool has_size = false;
        bool single_segment = false;
        bool checksum = false;
        uint32_t dictionary_id = 0;
        uint64_t window = 0;
        size_t size = 0;   // the header's bytes, the magic included

        // nullptr: read (size set), or need more (size 0 and *more true);
        // else what is wrong
        const char* read(const uint8_t* p, size_t n, bool& more) noexcept {
            more = false;
            size = 0;
            if (n < 5) {
                more = true;
                return nullptr;
            }
            const uint8_t fhd = p[4];
            const unsigned fcs_flag = fhd >> 6;
            single_segment = fhd & 0x20;
            if (fhd & 0x08) {
                return "zstd: a reserved bit of the frame header set";
            }
            checksum = fhd & 0x04;
            const unsigned did_flag = fhd & 3;
            const size_t did_size = did_flag == 0 ? 0 : did_flag == 1 ? 1 : did_flag == 2 ? 2 : 4;
            const size_t fcs_size = fcs_flag == 0 ? (single_segment ? 1 : 0) : fcs_flag == 1 ? 2 : fcs_flag == 2 ? 4 : 8;
            const size_t need = 5 + (single_segment ? 0 : 1) + did_size + fcs_size;
            if (n < need) {
                more = true;
                return nullptr;
            }
            size_t at = 5;
            window = 0;
            if (!single_segment) {
                const unsigned wd = p[at++];
                const unsigned exponent = wd >> 3;
                const unsigned mantissa = wd & 7;
                const uint64_t base = uint64_t(1) << (10 + exponent);
                window = base + (base / 8) * mantissa;
            }
            dictionary_id = 0;
            for (size_t i = 0; i < did_size; ++i) {
                dictionary_id |= uint32_t(p[at++]) << (8 * i);
            }
            content_size = 0;
            for (size_t i = 0; i < fcs_size; ++i) {
                content_size |= uint64_t(p[at++]) << (8 * i);
            }
            if (fcs_size == 2) {
                content_size += 256;
            }
            has_size = fcs_size != 0;
            if (single_segment) {
                window = content_size;
            }
            size = need;
            return nullptr;
        }
    };

    // A dictionary as the decoder and the encoder take it: its id, its
    // content, and, for one of zstd's format, the entropy tables and the
    // offsets the first block starts from
    struct ZstdDictionaryData {
        uint32_t id = 0;
        std::vector<uint8_t> content;
        bool has_entropy = false;
        ZstdEntropy entropy;
        // what the encoder needs of the tables: the counts as written
        FseCounts ll_counts, of_counts, ml_counts;
        uint8_t huf_weights[256];
        unsigned huf_symbols = 0;

        // nullptr: parsed; else what is wrong
        const char* parse(const uint8_t* p, size_t n) noexcept {
            entropy.reset();
            has_entropy = false;
            id = 0;
            if (n < 8 || zstd_le32(p) != ZstdDictionaryMagic) {
                content.assign(p, p + n);   // raw content
                return nullptr;
            }
            id = zstd_le32(p + 4);
            size_t at = 8;
            const size_t h = huf_read_table(p + at, n - at, entropy.huf);
            if (!h) {
                return "zstd: a dictionary's Huffman table that is not valid";
            }
            at += h;
            const ZstdDefaults& d = ZstdDefaults::get();
            (void)d;
            size_t u = fse_read_counts(p + at, n - at, of_counts, 32, 8);
            if (!u || !zstd_build_from_counts(entropy.offset, of_counts, ZstdCode::offset)) {
                return "zstd: a dictionary's offsets table that is not valid";
            }
            at += u;
            u = fse_read_counts(p + at, n - at, ml_counts, 53, 9);
            if (!u || !zstd_build_from_counts(entropy.match, ml_counts, ZstdCode::match)) {
                return "zstd: a dictionary's match lengths table that is not valid";
            }
            at += u;
            u = fse_read_counts(p + at, n - at, ll_counts, 36, 9);
            if (!u || !zstd_build_from_counts(entropy.literal, ll_counts, ZstdCode::literal)) {
                return "zstd: a dictionary's literal lengths table that is not valid";
            }
            at += u;
            if (n - at < 12) {
                return "zstd: a dictionary cut short in its offsets";
            }
            for (int i = 0; i < 3; ++i) {
                entropy.rep[i] = zstd_le32(p + at + 4 * i);
            }
            at += 12;
            content.assign(p + at, p + n);
            for (int i = 0; i < 3; ++i) {
                if (entropy.rep[i] == 0 || entropy.rep[i] > content.size()) {
                    return "zstd: a dictionary's offset past its content";
                }
            }
            has_entropy = true;
            return nullptr;
        }
    };

    // The door to the state a public zstd::dictionary holds
    struct ZstdDictionaryAccess {
        template<class D>
        SGCL_INLINE_HOT static const tracked_ptr<ZstdDictionaryData>& get(const D& d) noexcept {
            return d._state;
        }
    };

    // The work of a decode: the literals' buffer and the tables, made once
    struct ZstdWork {
        uint8_t literals[ZstdBlockMax + ZstdOutSlack];
        ZstdEntropy entropy;
    };

    // A whole decompress in memory with no dictionary: every frame, each
    // block decoded straight into the result, the history the result
    // itself; skippable frames passed over
    inline expected<vector<byte>, error> zstd_decompress_all(const uint8_t* p, size_t n, const limits& l) noexcept {
        auto fail = [](errc code, uint64_t at, const char* text) noexcept {
            return unexpected<error>(error(code, at, string(text)));
        };
        const uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
        vector<byte> result;
        size_t capacity = 0;
        size_t total = 0;
        auto out = [&]() noexcept {
            return reinterpret_cast<uint8_t*>(result.data());
        };
        auto grow = [&](uint64_t end) noexcept {
            const uint64_t target = std::min(end, ceiling);
            if (target <= capacity) {
                return;
            }
            const uint64_t c = std::min<uint64_t>(std::max<uint64_t>({target, uint64_t(capacity) * 2, uint64_t(1) << 16}), ceiling);
            vector<byte> grown;
            sgcl::detail::VectorOverwrite::resize(grown, size_t(c) + ZstdOutSlack);   // every byte written before it is returned: resize(total) below
            copy_out(grown.data(), result.data(), total);
            result = std::move(grown);
            capacity = size_t(c);
        };
        std::unique_ptr<ZstdWork> work(new ZstdWork);
        size_t at = 0;
        bool any = false;
        while (at < n || !any) {
            if (n - at < 4) {
                return fail(errc::unexpected_end, n, any ? "zstd: data after the last frame is not a frame" : "zstd: unexpected end before a frame");
            }
            any = true;
            const uint32_t magic = zstd_le32(p + at);
            if ((magic & 0xFFFFFFF0u) == ZstdSkippableMagic) {
                if (n - at < 8) {
                    return fail(errc::unexpected_end, n, "zstd: unexpected end in a skippable frame");
                }
                const uint64_t size = zstd_le32(p + at + 4);
                if (size > n - at - 8) {
                    return fail(errc::unexpected_end, n, "zstd: unexpected end in a skippable frame");
                }
                at += 8 + size_t(size);
                continue;
            }
            if (magic != ZstdMagic) {
                return fail(errc::invalid_header, at, at ? "zstd: data after the last frame is not a frame" : "zstd: not a zstd frame");
            }
            ZstdFrameHeader h;
            bool more = false;
            if (const char* wrong = h.read(p + at, n - at, more)) {
                return fail(errc::invalid_header, at, wrong);
            }
            if (more) {
                return fail(errc::unexpected_end, n, "zstd: unexpected end in a frame header");
            }
            if (h.dictionary_id) {
                return fail(errc::dictionary_required, at, "zstd: the frame was made with a dictionary");
            }
            if (h.window > l.max_memory) {
                return fail(errc::too_large, at, "zstd: the frame's window needs more memory than the limit allows");
            }
            if (h.has_size) {
                if (h.content_size > l.max_size || total + h.content_size > l.max_size) {
                    return fail(errc::too_large, at, "zstd: decompressed data past the limit");
                }
                grow(total + h.content_size);
            }
            const size_t block_max = size_t(std::min<uint64_t>(h.window, ZstdBlockMax));
            at += h.size;
            const size_t frame_start = total;
            work->entropy.reset();
            for (;;) {
                if (n - at < 3) {
                    return fail(errc::unexpected_end, n, "zstd: unexpected end at a block's header");
                }
                const uint32_t bh = uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16;
                const bool last = bh & 1;
                const unsigned type = (bh >> 1) & 3;
                const size_t size = bh >> 3;
                at += 3;
                if (type == 3) {
                    return fail(errc::corrupt, at - 3, "zstd: a block of the reserved type");
                }
                if (size > block_max) {
                    return fail(errc::corrupt, at - 3, "zstd: a block larger than the frame allows");
                }
                if (type == 0) {
                    if (size > n - at) {
                        return fail(errc::unexpected_end, n, "zstd: unexpected end in a block");
                    }
                    grow(uint64_t(total) + size);
                    if (uint64_t(total) + size > l.max_size) {
                        return fail(errc::too_large, at, "zstd: decompressed data past the limit");
                    }
                    copy_out(out() + total, p + at, size);
                    total += size;
                    at += size;
                } else if (type == 1) {
                    if (n - at < 1) {
                        return fail(errc::unexpected_end, n, "zstd: unexpected end in a block");
                    }
                    grow(uint64_t(total) + size);
                    if (uint64_t(total) + size > l.max_size) {
                        return fail(errc::too_large, at, "zstd: decompressed data past the limit");
                    }
                    sgcl::detail::fill_bytes(out() + total, p[at], size);
                    total += size;
                    at += 1;
                } else {
                    if (size > n - at) {
                        return fail(errc::unexpected_end, n, "zstd: unexpected end in a block");
                    }
                    // the room: a block's most, no further than the size the
                    // header gives (no growth past it at the frame's end)
                    uint64_t room_end = uint64_t(total) + block_max;
                    const bool sized = h.has_size && frame_start + h.content_size < room_end;
                    if (sized) {
                        room_end = frame_start + h.content_size;
                    }
                    grow(room_end);
                    const bool limited = capacity < room_end;
                    uint8_t* dst = out() + total;
                    auto r = zstd_decode_block(p + at, size, dst, out() + (limited ? capacity : size_t(room_end)), out() + frame_start, work->entropy,
                                               work->literals);
                    if (r.status != ZstdFail::none) {
                        if (r.status == ZstdFail::overflow && limited) {
                            return fail(errc::too_large, at, "zstd: decompressed data past the limit");
                        }
                        if (r.status == ZstdFail::overflow && sized) {
                            return fail(errc::corrupt, at, "zstd: the content differs in size from the frame's header");
                        }
                        return fail(r.status == ZstdFail::truncated ? errc::corrupt : errc::corrupt, at + r.at, r.text);
                    }
                    total += r.written;
                    if (total > l.max_size) {
                        return fail(errc::too_large, at, "zstd: decompressed data past the limit");
                    }
                    at += size;
                }
                if (last) {
                    break;
                }
            }
            if (h.checksum) {
                if (n - at < 4) {
                    return fail(errc::unexpected_end, n, "zstd: unexpected end at the content checksum");
                }
                if (uint32_t(hash::detail::xxh64(out() + frame_start, total - frame_start, 0)) != zstd_le32(p + at)) {
                    return fail(errc::checksum, at, "zstd: the content checksum does not match");
                }
                at += 4;
            }
            if (h.has_size && total - frame_start != h.content_size) {
                return fail(errc::corrupt, at, "zstd: the content differs in size from the frame's header");
            }
        }
        result.resize(total);
        return result;
    }

    // The streaming decoder: frames one after another, each block decoded
    // into a buffer of the dictionary, twice the window and a block, and
    // handed out from there; when the buffer fills, its last window (and
    // what of the dictionary the frame may still reach) moves to the front
    class ZstdFrameDecoder {
    public:
        static constexpr const char* name = "zstd";

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        template<class O>
        ZstdFrameDecoder(const O& o, const limits& l) noexcept
        : _max_memory(l.max_memory)
        , _dictionary(ZstdDictionaryAccess::get(o.dictionary))
        , _work(new ZstdWork) {
            reset();
        }

        void reset() noexcept {
            _stage = Stage::magic;
            _have = 0;
            _frames = 0;
            _taken = 0;
            _out_begin = _out_end = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _taken;
        }

        CodecStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept {
            for (;;) {
                if (_out_begin < _out_end) {
                    const size_t k = std::min(_out_end - _out_begin, cap - pos);
                    copy_out(out + pos, _buffer.get() + _out_begin, k);
                    pos += k;
                    _out_begin += k;
                    if (_out_begin < _out_end) {
                        return CodecStatus::need_room;
                    }
                }
                switch (_stage) {
                    case Stage::magic: {
                        if (!_gather(in, end, 4)) {
                            if (final) {
                                if (_have == 0 && _frames > 0) {
                                    return CodecStatus::done;
                                }
                                return _fail(errc::unexpected_end, "zstd: unexpected end before a frame");
                            }
                            return CodecStatus::need_input;
                        }
                        const uint32_t magic = zstd_le32(_head);
                        if ((magic & 0xFFFFFFF0u) == ZstdSkippableMagic) {
                            _have = 0;
                            _stage = Stage::skip_size;
                        } else if (magic == ZstdMagic) {
                            _stage = Stage::header;   // the header is gathered after the magic, in _head
                        } else {
                            return _fail(errc::invalid_header, _frames ? "zstd: data after the last frame is not a frame" : "zstd: not a zstd frame");
                        }
                        break;
                    }
                    case Stage::skip_size: {
                        if (!_gather(in, end, 4)) {
                            return _more(final, "zstd: unexpected end in a skippable frame");
                        }
                        _skip = zstd_le32(_head);
                        _have = 0;
                        _stage = Stage::skip;
                        break;
                    }
                    case Stage::skip: {
                        const size_t k = size_t(std::min<uint64_t>(_skip, uint64_t(end - in)));
                        in += k;
                        _taken += k;
                        _skip -= k;
                        if (_skip) {
                            return _more(final, "zstd: unexpected end in a skippable frame");
                        }
                        ++_frames;
                        _stage = Stage::magic;
                        break;
                    }
                    case Stage::header: {
                        if (!_gather(in, end, 5)) {
                            return _more(final, "zstd: unexpected end in a frame header");
                        }
                        bool more = false;
                        const char* wrong = _h.read(_head, _have, more);
                        if (wrong) {
                            return _fail(errc::invalid_header, wrong);
                        }
                        if (more) {
                            const uint8_t fhd = _head[4];
                            const unsigned did = fhd & 3;
                            const unsigned fcs = fhd >> 6;
                            const size_t need = 5 + ((fhd & 0x20) ? 0 : 1) + (did == 0 ? 0 : did == 1 ? 1 : did == 2 ? 2 : 4) +
                                                (fcs == 0 ? ((fhd & 0x20) ? 1 : 0) : fcs == 1 ? 2 : fcs == 2 ? 4 : 8);
                            if (!_gather(in, end, need)) {
                                return _more(final, "zstd: unexpected end in a frame header");
                            }
                            wrong = _h.read(_head, _have, more);
                            if (wrong) {
                                return _fail(errc::invalid_header, wrong);
                            }
                        }
                        _have = 0;
                        if (auto e = _start_frame()) {
                            return *e;
                        }
                        _stage = Stage::block_header;
                        break;
                    }
                    case Stage::block_header: {
                        if (!_gather(in, end, 3)) {
                            return _more(final, "zstd: unexpected end at a block's header");
                        }
                        const uint32_t bh = uint32_t(_head[0]) | uint32_t(_head[1]) << 8 | uint32_t(_head[2]) << 16;
                        _have = 0;
                        _last = bh & 1;
                        _type = (bh >> 1) & 3;
                        _size = bh >> 3;
                        if (_type == 3) {
                            return _fail(errc::corrupt, "zstd: a block of the reserved type");
                        }
                        if (_size > _block_max) {
                            return _fail(errc::corrupt, "zstd: a block larger than the frame allows");
                        }
                        _block_have = 0;
                        _block_at = _taken;
                        _stage = Stage::block_data;
                        break;
                    }
                    case Stage::block_data: {
                        const size_t want = _type == 1 ? 1 : _size;
                        const uint8_t* data;
                        if (_block_have == 0 && size_t(end - in) >= want) {
                            data = in;
                            in += want;
                            _taken += want;
                        } else {
                            const size_t k = std::min(want - _block_have, size_t(end - in));
                            sgcl::detail::copy_bytes(_block.get() + _block_have, in, k);
                            in += k;
                            _taken += k;
                            _block_have += k;
                            if (_block_have < want) {
                                return _more(final, "zstd: unexpected end in a block");
                            }
                            data = _block.get();
                        }
                        if (auto e = _decode_block(data)) {
                            return *e;
                        }
                        _stage = _last ? (_h.checksum ? Stage::checksum : Stage::frame_end) : Stage::block_header;
                        break;
                    }
                    case Stage::checksum: {
                        if (!_gather(in, end, 4)) {
                            return _more(final, "zstd: unexpected end at the content checksum");
                        }
                        _have = 0;
                        if (uint32_t(_hasher.value()) != zstd_le32(_head)) {
                            return _fail(errc::checksum, "zstd: the content checksum does not match");
                        }
                        _stage = Stage::frame_end;
                        break;
                    }
                    case Stage::frame_end: {
                        if (_h.has_size && _produced != _h.content_size) {
                            return _fail(errc::corrupt, "zstd: the content differs in size from the frame's header");
                        }
                        ++_frames;
                        _stage = Stage::magic;
                        break;
                    }
                }
            }
        }

    private:
        enum class Stage : uint8_t {
            magic,
            skip_size,
            skip,
            header,
            block_header,
            block_data,
            checksum,
            frame_end
        };

        SGCL_INLINE_HOT bool _gather(const uint8_t*& in, const uint8_t* end, size_t n) noexcept {
            const size_t k = std::min(n > _have ? n - _have : 0, size_t(end - in));
            sgcl::detail::copy_bytes(_head + _have, in, k);
            _have += k;
            in += k;
            _taken += k;
            return _have >= n;
        }

        SGCL_INLINE_HOT CodecStatus _more(bool final, const char* text) noexcept {
            if (final) {
                return _fail(errc::unexpected_end, text);
            }
            return CodecStatus::need_input;
        }

        CodecStatus _fail(errc code, const char* text) noexcept {
            error = code;
            error_text = text;
            return CodecStatus::failed;
        }

        // A frame begins: its dictionary checked, its window made
        optional<CodecStatus> _start_frame() noexcept {
            const ZstdDictionaryData* d = _dictionary.get();
            if (_h.dictionary_id) {
                if (!d) {
                    return _fail(errc::dictionary_required, "zstd: the frame was made with a dictionary");
                }
                if (d->id && d->id != _h.dictionary_id) {
                    return _fail(errc::dictionary_required, "zstd: the frame was made with another dictionary");
                }
            }
            if (_h.window > _max_memory) {
                return _fail(errc::too_large, "zstd: the frame's window needs more memory than the limit allows");
            }
            _window = size_t(_h.window);
            _block_max = std::min(_window, ZstdBlockMax);
            const size_t dict = d ? d->content.size() : 0;
            const size_t need = dict + 2 * _window + _block_max + ZstdOutSlack;
            if (_capacity < need) {
                _buffer.reset(new uint8_t[need]);
                _capacity = need;
            }
            if (!_block) {
                _block.reset(new uint8_t[ZstdBlockMax]);
            }
            if (dict) {
                sgcl::detail::copy_bytes(_buffer.get(), d->content.data(), dict);
            }
            _low = 0;
            _pos = dict;
            _produced = 0;
            _hasher.reset();
            if (d && d->has_entropy) {
                _work->entropy = d->entropy;
            } else {
                _work->entropy.reset();
            }
            return nullopt;
        }

        optional<CodecStatus> _decode_block(const uint8_t* data) noexcept {
            // room for a block: slide the history to the front when the end is near
            if (_pos + _block_max + ZstdOutSlack > _capacity) {
                // past twice the window: what a match may still reach is the last window
                const size_t keep = std::min(_pos - _low, _window);
                std::memmove(_buffer.get(), _buffer.get() + _pos - keep, keep);
                _pos = keep;
                _low = 0;
            }
            uint8_t* const dst = _buffer.get() + _pos;
            size_t made;
            if (_type == 0) {
                sgcl::detail::copy_bytes(dst, data, _size);
                made = _size;
            } else if (_type == 1) {
                sgcl::detail::fill_bytes(dst, data[0], _size);
                made = _size;
            } else {
                auto r = zstd_decode_block(data, _size, dst, dst + _block_max, _buffer.get() + _low, _work->entropy, _work->literals);
                if (r.status != ZstdFail::none) {
                    _taken = _block_at + r.at;
                    return _fail(errc::corrupt, r.text);
                }
                made = r.written;
            }
            if (_h.checksum) {
                _hasher.update(slice<const byte>(reinterpret_cast<const byte*>(dst), made));
            }
            _produced += made;
            if (_h.has_size && _produced > _h.content_size) {
                return _fail(errc::corrupt, "zstd: the content is longer than the frame's header says");
            }
            _out_begin = _pos;
            _out_end = _pos + made;
            _pos += made;
            return nullopt;
        }

        uint64_t _max_memory;
        root_ptr<ZstdDictionaryData> _dictionary;   // a root: the decoder lives outside the managed heap
        std::unique_ptr<ZstdWork> _work;
        Stage _stage = Stage::magic;
        uint8_t _head[24];
        size_t _have = 0;
        ZstdFrameHeader _h;
        bool _last = false;
        unsigned _type = 0;
        size_t _size = 0;
        size_t _block_have = 0;
        uint64_t _block_at = 0;
        uint64_t _skip = 0;
        uint64_t _frames = 0;
        uint64_t _taken = 0;
        uint64_t _produced = 0;
        size_t _window = 0;
        size_t _block_max = 0;
        std::unique_ptr<uint8_t[]> _buffer;
        size_t _capacity = 0;
        std::unique_ptr<uint8_t[]> _block;
        size_t _low = 0;     // where the history the frame may reach begins in the buffer
        size_t _pos = 0;     // where the next block goes
        size_t _out_begin = 0;
        size_t _out_end = 0;
        hash::xxh64 _hasher;
    };
}
