//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "copy.h"
#include "../error.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::compress::detail {
    // The decoder of DEFLATE (RFC 1951), resumable at any byte of its
    // input: a state that decodes into a caller's buffer whose front is the
    // history the back references reach (a whole output in memory, or the
    // window of a stream), given its input in pieces of any size.
    //
    // Deflate64 (PKWARE's "enhanced deflate", zip method 9, 7z's 040109)
    // is the same format with three changes: the window is 64 KB, the
    // length symbol 285 takes 16 extra bits over a base of 3 (lengths up to
    // 65 538), and the distance symbols 30 and 31, reserved in DEFLATE,
    // reach 32 769 and 49 153 with 14 extra bits. inflate64() decodes it:
    // the same code, instantiated a second time, so that DEFLATE's is not
    // slowed by what only Deflate64 needs.
    //
    // The Huffman codes are decoded through tables indexed by the next
    // bits of the input, read least significant bit first as the format
    // packs them: a root table of RootBits bits, and for a code longer than
    // that a subtable the root entry links to. An entry is one word:
    //   bits 0..4    the bits it consumes: a code's length, with a length's or a
    //                distance's extra bits added in; a link's root bits
    //   bits 5..7    its kind
    //   bits 8..12   the extra bits of a length or a distance, a link's subtable bits
    //   bits 16..31  the literal, the base length or distance, the subtable's start
    // A literal/length entry carries the base and the extra bits of its
    // length already, so that a length is one lookup and one read of bits.
    enum : uint32_t {
        EntryLiteral = 0,
        EntryLength = 1,
        EntryEnd = 2,
        EntryLink = 3,
        EntryInvalid = 4,
        EntryDistance = 5
    };

    SGCL_INLINE_HOT constexpr uint32_t make_entry(uint32_t bits, uint32_t kind, uint32_t extra, uint32_t value) noexcept {
        return bits | (kind << 5) | (extra << 8) | (value << 16);
    }

    SGCL_INLINE_HOT constexpr uint32_t entry_bits(uint32_t e) noexcept {
        return e & 31;
    }

    SGCL_INLINE_HOT constexpr uint32_t entry_kind(uint32_t e) noexcept {
        return (e >> 5) & 7;
    }

    SGCL_INLINE_HOT constexpr uint32_t entry_extra(uint32_t e) noexcept {
        return (e >> 8) & 31;
    }

    SGCL_INLINE_HOT constexpr uint32_t entry_value(uint32_t e) noexcept {
        return e >> 16;
    }

    // The lengths and the distances of the format: base and extra bits of
    // each symbol (section 3.2.5)
    inline constexpr uint16_t LengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    inline constexpr uint8_t LengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    inline constexpr uint16_t DistanceBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    inline constexpr uint8_t DistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    // Deflate64's: the distances 30 and 31, the length 285
    inline constexpr uint16_t Distance64Base[2] = {32769, 49153};
    inline constexpr uint8_t Distance64Extra = 14;
    inline constexpr uint16_t Length64Base = 3;
    inline constexpr uint8_t Length64Extra = 16;

    // The order the code length code's lengths come in (section 3.2.7)
    inline constexpr uint8_t CodeLengthOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    inline constexpr unsigned LitLenRootBits = 10;
    inline constexpr unsigned DistRootBits = 8;
    inline constexpr unsigned CodeLengthRootBits = 7;
    // A complete code of at most 286 symbols and 15 bits needs no more than
    // this with a root of 10 bits: a subtable of 2^d entries holds at least
    // d + 1 codes, and 2^d / (d + 1) is at most 32 / 6 for d <= 5; the
    // distances likewise with a root of 8 (d <= 7, at most 30 codes)
    inline constexpr unsigned LitLenTableSize = (1u << LitLenRootBits) + 1536;
    inline constexpr unsigned DistTableSize = (1u << DistRootBits) + 512;
    inline constexpr unsigned MaxRootBits = 11;

    inline constexpr uint32_t MaxMatch = 258;
    inline constexpr uint32_t WindowSize = 32768;
    inline constexpr uint32_t Window64Size = 65536;

    constexpr uint32_t reverse_bits(uint32_t code, unsigned length) noexcept {
        uint32_t r = 0;
        for (unsigned i = 0; i < length; ++i) {
            r = (r << 1) | (code & 1);
            code >>= 1;
        }
        return r;
    }

    // What a symbol of a code means, as a table entry of `bits` bits
    enum class CodeKind : uint8_t {
        litlen,
        distance,
        code_length
    };

    constexpr uint32_t symbol_entry(CodeKind kind, unsigned symbol, unsigned bits, bool wide) noexcept {
        switch (kind) {
            case CodeKind::litlen:
                if (symbol < 256) {
                    return make_entry(bits, EntryLiteral, 0, symbol);
                }
                if (symbol == 256) {
                    return make_entry(bits, EntryEnd, 0, 0);
                }
                if (symbol == 285 && wide) {
                    return make_entry(bits + Length64Extra, EntryLength, Length64Extra, Length64Base);
                }
                if (symbol < 286) {
                    return make_entry(bits + LengthExtra[symbol - 257], EntryLength, LengthExtra[symbol - 257], LengthBase[symbol - 257]);
                }
                return make_entry(bits, EntryInvalid, 0, 0);
            case CodeKind::distance:
                if (symbol < 30) {
                    return make_entry(bits + DistanceExtra[symbol], EntryDistance, DistanceExtra[symbol], DistanceBase[symbol]);
                }
                if (symbol < 32 && wide) {
                    return make_entry(bits + Distance64Extra, EntryDistance, Distance64Extra, Distance64Base[symbol - 30]);
                }
                return make_entry(bits, EntryInvalid, 0, 0);
            case CodeKind::code_length:
                return make_entry(bits, EntryLiteral, 0, symbol);
        }
        return make_entry(bits, EntryInvalid, 0, 0);
    }

    // The canonical code of these lengths (section 3.2.2) as a table:
    // false for an over-subscribed set, or an incomplete one other than a
    // single code of one bit (which the format allows: a block with one
    // distance) or no code at all (a block with no distances, whose table
    // answers every lookup with an invalid entry). The table has room for
    // `capacity` entries; `wide`, the symbols as Deflate64 has them.
    inline bool build_table(uint32_t* table, unsigned capacity, unsigned root, const uint8_t* lengths, unsigned count, CodeKind kind, bool wide = false) noexcept {
        unsigned bl_count[16] = {};
        for (unsigned i = 0; i < count; ++i) {
            ++bl_count[lengths[i]];
        }
        bl_count[0] = 0;
        int left = 1;
        unsigned used = 0;
        unsigned max_length = 0;
        for (unsigned len = 1; len <= 15; ++len) {
            left <<= 1;
            left -= int(bl_count[len]);
            if (left < 0) {
                return false;
            }
            used += bl_count[len];
            if (bl_count[len]) {
                max_length = len;
            }
        }
        unsigned root_size = 1u << root;
        if (used == 0) {
            std::fill(table, table + root_size, make_entry(1, EntryInvalid, 0, 0));
            return true;
        }
        if (left > 0 && !(used == 1 && max_length == 1)) {
            return false;
        }
        uint32_t next_code[16] = {};
        uint32_t code = 0;
        for (unsigned len = 1; len <= 15; ++len) {
            code = (code + bl_count[len - 1]) << 1;
            next_code[len] = code;
        }
        // an incomplete code of one symbol: the other half of the table is invalid
        std::fill(table, table + root_size, make_entry(1, EntryInvalid, 0, 0));
        // the longest code under each root prefix, which sizes its subtable
        uint8_t prefix_max[1u << MaxRootBits] = {};
        if (max_length > root) {
            uint32_t probe[16];
            std::memcpy(probe, next_code, sizeof(probe));
            for (unsigned s = 0; s < count; ++s) {
                unsigned len = lengths[s];
                if (len > root) {
                    uint32_t c = probe[len]++;
                    uint32_t prefix = reverse_bits(c >> (len - root), root);
                    prefix_max[prefix] = std::max<uint8_t>(prefix_max[prefix], uint8_t(len));
                }
            }
        }
        unsigned next_sub = root_size;
        uint16_t sub_start[1u << MaxRootBits];
        if (max_length > root) {
            for (unsigned p = 0; p < root_size; ++p) {
                if (prefix_max[p]) {
                    unsigned sub_bits = prefix_max[p] - root;
                    if (next_sub + (1u << sub_bits) > capacity) {
                        return false;
                    }
                    sub_start[p] = uint16_t(next_sub);
                    table[p] = make_entry(root, EntryLink, sub_bits, next_sub);
                    std::fill(table + next_sub, table + next_sub + (1u << sub_bits), make_entry(1, EntryInvalid, 0, 0));
                    next_sub += 1u << sub_bits;
                }
            }
        }
        for (unsigned s = 0; s < count; ++s) {
            unsigned len = lengths[s];
            if (!len) {
                continue;
            }
            uint32_t c = next_code[len]++;
            if (len <= root) {
                uint32_t r = reverse_bits(c, len);
                uint32_t e = symbol_entry(kind, s, len, wide);
                for (uint32_t i = r; i < root_size; i += 1u << len) {
                    table[i] = e;
                }
            } else {
                uint32_t prefix = reverse_bits(c >> (len - root), root);
                unsigned sub_bits = prefix_max[prefix] - root;
                unsigned rest = len - root;
                uint32_t r = reverse_bits(c & ((1u << rest) - 1), rest);
                uint32_t e = symbol_entry(kind, s, rest, wide);
                uint32_t* sub = table + sub_start[prefix];
                for (uint32_t i = r; i < (1u << sub_bits); i += 1u << rest) {
                    sub[i] = e;
                }
            }
        }
        return true;
    }

    // The fixed codes of section 3.2.6, built once (DEFLATE's, Deflate64's)
    struct FixedTables {
        uint32_t litlen[LitLenTableSize];
        uint32_t dist[DistTableSize];

        explicit FixedTables(bool wide) noexcept {
            uint8_t lengths[288];
            for (unsigned i = 0; i < 144; ++i) lengths[i] = 8;
            for (unsigned i = 144; i < 256; ++i) lengths[i] = 9;
            for (unsigned i = 256; i < 280; ++i) lengths[i] = 7;
            for (unsigned i = 280; i < 288; ++i) lengths[i] = 8;
            build_table(litlen, LitLenTableSize, LitLenRootBits, lengths, 288, CodeKind::litlen, wide);
            // 32 codes of five bits, 30 and 31 valid in Deflate64's data only
            uint8_t d[32];
            std::fill(d, d + 32, uint8_t(5));
            build_table(dist, DistTableSize, DistRootBits, d, 32, CodeKind::distance, wide);
        }
    };

    template<bool Wide>
    inline const FixedTables& fixed_tables() noexcept {
        static const FixedTables t(Wide);
        return t;
    }

    enum class InflateStatus : uint8_t {
        need_input,   // every byte given was taken; more is needed
        need_room,    // the output has no room for the next piece
        done,         // the last block ended: the bytes after it were given back
        failed
    };

    enum class InflateStage : uint8_t {
        header,
        stored_header,
        stored_copy,
        dynamic_header,
        code_length_codes,
        code_lengths,
        codes,
        distance,
        copy,
        done
    };

    // The state of one DEFLATE stream, plain memory (a member of a reader,
    // or a local); reset() before use.
    struct InflateState {
        uint64_t bits;                // the input bits not yet used, the next at bit 0
        uint32_t count;               // how many; fewer than 8 between two calls
        InflateStage stage;
        bool final_block;
        bool fixed;                   // the current block uses the fixed codes
        uint16_t hlit, hdist, hclen, index;
        uint32_t stored_left;
        uint32_t copy_length, copy_distance;
        errc error;
        const char* error_text;
        uint8_t lengths[320];         // the code lengths of a dynamic header being read
        uint32_t code_length_table[1u << CodeLengthRootBits];
        uint32_t litlen[LitLenTableSize];
        uint32_t dist[DistTableSize];

        void reset() noexcept {
            bits = 0;
            count = 0;
            stage = InflateStage::header;
            final_block = false;
            fixed = false;
            stored_left = 0;
            copy_length = 0;
            copy_distance = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }
    };

    // Decodes from [in, in_end) into out[pos, capacity), the bytes before
    // pos being the history a back reference may reach (the whole output
    // so far, or a stream's window with a dictionary or the last 32 KB in
    // front). Returns when the input runs out (every byte given taken, the
    // bits of a symbol cut short kept in the state), when the room does
    // (fewer than MaxMatch + 8 bytes left for the fast loop and none for
    // the next byte), when the last block ends (the whole bytes read ahead
    // given back: `in` points at the first byte past the stream) or on
    // corrupt data (the state's error and error_text say what).
    //
    // The slow path takes a byte only when the next piece needs it, so
    // that the bits held between two calls belong to a piece not yet
    // decoded; the fast loop reads ahead eight bytes at a time and gives
    // back what it did not use when it stops.
    //
    // Wide: Deflate64. Its history reaches 64 KB back; a match may be
    // longer than the room the fast loop keeps, and then goes to the copy
    // stage; a length and a distance with their extra bits may take 60
    // bits, one refill more.
    template<bool Wide>
    inline InflateStatus inflate_blocks(InflateState& s, const uint8_t*& in, const uint8_t* in_end, uint8_t* out, size_t& pos, size_t capacity) noexcept {
        const uint8_t* const in_start = in;
        uint64_t bitbuf = s.bits;
        uint32_t bitcnt = s.count;
        size_t p = pos;
        const uint32_t* lit_table = s.fixed ? fixed_tables<Wide>().litlen : s.litlen;
        const uint32_t* dist_table = s.fixed ? fixed_tables<Wide>().dist : s.dist;
        InflateStatus status = InflateStatus::need_input;

        auto more = [&]() noexcept -> bool {
            if (in == in_end) {
                return false;
            }
            bitbuf |= uint64_t(*in++) << bitcnt;
            bitcnt += 8;
            return true;
        };
        auto need = [&](uint32_t n) noexcept -> bool {
            while (bitcnt < n) {
                if (!more()) {
                    return false;
                }
            }
            return true;
        };
        // The entry of the next symbol of a code and the bits it takes, a
        // link's root bits included; false when the input runs out first.
        // A lookup over missing bits (zeros) is only trusted when the entry
        // it finds is no longer than the bits there are.
        auto symbol = [&](const uint32_t* table, unsigned root, uint32_t& entry, uint32_t& bits) noexcept -> bool {
            for (;;) {
                uint32_t e = table[bitbuf & ((1u << root) - 1)];
                uint32_t n = entry_bits(e);
                if (entry_kind(e) == EntryLink) {
                    e = table[entry_value(e) + ((bitbuf >> root) & ((1u << entry_extra(e)) - 1))];
                    n = root + entry_bits(e);
                }
                if (n <= bitcnt) {
                    entry = e;
                    bits = n;
                    return true;
                }
                if (!more()) {
                    return false;
                }
            }
        };
        auto fail = [&](const char* what) noexcept {
            s.error = errc::corrupt;
            s.error_text = what;
            status = InflateStatus::failed;
        };
        auto take = [&](uint32_t n) noexcept {
            bitbuf >>= n;
            bitcnt -= n;
        };

        for (;;) {
            switch (s.stage) {
                case InflateStage::header: {
                    if (!need(3)) {
                        goto out;
                    }
                    s.final_block = bitbuf & 1;
                    unsigned type = unsigned(bitbuf >> 1) & 3;
                    take(3);
                    if (type == 0) {
                        s.stage = InflateStage::stored_header;
                    } else if (type == 1) {
                        s.fixed = true;
                        lit_table = fixed_tables<Wide>().litlen;
                        dist_table = fixed_tables<Wide>().dist;
                        s.stage = InflateStage::codes;
                    } else if (type == 2) {
                        s.fixed = false;
                        s.stage = InflateStage::dynamic_header;
                    } else {
                        fail("invalid block type 3");
                        goto out;
                    }
                    break;
                }
                case InflateStage::stored_header: {
                    // to the byte boundary, then LEN and NLEN
                    take(bitcnt & 7);
                    if (!need(32)) {
                        goto out;
                    }
                    uint32_t len = uint32_t(bitbuf & 0xFFFF);
                    uint32_t nlen = uint32_t((bitbuf >> 16) & 0xFFFF);
                    take(32);
                    if (len != (~nlen & 0xFFFF)) {
                        fail("stored block length does not match its complement");
                        goto out;
                    }
                    s.stored_left = len;
                    s.stage = InflateStage::stored_copy;
                    break;
                }
                case InflateStage::stored_copy: {
                    // the whole bytes in the bit buffer first, then the input
                    while (s.stored_left && bitcnt >= 8) {
                        if (p == capacity) {
                            status = InflateStatus::need_room;
                            goto out;
                        }
                        out[p++] = uint8_t(bitbuf);
                        take(8);
                        --s.stored_left;
                    }
                    if (s.stored_left) {
                        // The bytes now come from the input past the bit buffer:
                        // what the fast loop's refill read ahead above the bits
                        // counted is of bytes this copy takes, and would be ORed
                        // into the next header. Only the counted bits stay.
                        bitbuf = bitcnt ? bitbuf & ((uint64_t(1) << bitcnt) - 1) : 0;
                        size_t n = std::min<size_t>({size_t(s.stored_left), size_t(in_end - in), capacity - p});
                        // A stored block, up to 64 KB, into a result perhaps not in
                        // the cache: libc's copy, as copy.h says why (one-shot
                        // inflate of stored blocks 19.1 GB/s, 15.0 by copy_bytes).
                        // Not copy_out: its branch here, in the decoder's loop,
                        // made that case 0.4-1.0x from run to run.
                        std::memcpy(out + p, in, n);
                        p += n;
                        in += n;
                        s.stored_left -= uint32_t(n);
                        if (s.stored_left) {
                            status = p == capacity ? InflateStatus::need_room : InflateStatus::need_input;
                            goto out;
                        }
                    }
                    s.stage = s.final_block ? InflateStage::done : InflateStage::header;
                    break;
                }
                case InflateStage::dynamic_header: {
                    if (!need(14)) {
                        goto out;
                    }
                    s.hlit = uint16_t((bitbuf & 31) + 257);
                    s.hdist = uint16_t(((bitbuf >> 5) & 31) + 1);
                    s.hclen = uint16_t(((bitbuf >> 10) & 15) + 4);
                    take(14);
                    if (s.hlit > 286 || s.hdist > (Wide ? 32 : 30)) {
                        fail("too many length or distance codes");
                        goto out;
                    }
                    s.index = 0;
                    std::memset(s.lengths, 0, 19);
                    s.stage = InflateStage::code_length_codes;
                    break;
                }
                case InflateStage::code_length_codes: {
                    while (s.index < s.hclen) {
                        if (!need(3)) {
                            goto out;
                        }
                        s.lengths[CodeLengthOrder[s.index++]] = uint8_t(bitbuf & 7);
                        take(3);
                    }
                    if (!build_table(s.code_length_table, 1u << CodeLengthRootBits, CodeLengthRootBits, s.lengths, 19, CodeKind::code_length)) {
                        fail("invalid code length code");
                        goto out;
                    }
                    s.index = 0;
                    s.stage = InflateStage::code_lengths;
                    break;
                }
                case InflateStage::code_lengths: {
                    unsigned total = s.hlit + s.hdist;
                    while (s.index < total) {
                        uint32_t e, n;
                        if (!symbol(s.code_length_table, CodeLengthRootBits, e, n)) {
                            goto out;
                        }
                        if (entry_kind(e) == EntryInvalid) {
                            fail("invalid code length symbol");
                            goto out;
                        }
                        uint32_t sym = entry_value(e);
                        if (sym < 16) {
                            take(n);
                            s.lengths[s.index++] = uint8_t(sym);
                            continue;
                        }
                        uint32_t extra = sym == 16 ? 2 : sym == 17 ? 3 : 7;
                        if (!need(n + extra)) {
                            goto out;
                        }
                        take(n);
                        uint32_t repeat = uint32_t(bitbuf & ((1u << extra) - 1));
                        take(extra);
                        uint8_t value = 0;
                        if (sym == 16) {
                            if (s.index == 0) {
                                fail("repeat of a code length with none before it");
                                goto out;
                            }
                            value = s.lengths[s.index - 1];
                            repeat += 3;
                        } else {
                            repeat += sym == 17 ? 3 : 11;
                        }
                        if (s.index + repeat > total) {
                            fail("code lengths past the number of codes");
                            goto out;
                        }
                        std::memset(s.lengths + s.index, value, repeat);
                        s.index = uint16_t(s.index + repeat);
                    }
                    if (s.lengths[256] == 0) {
                        fail("no code for the end of a block");
                        goto out;
                    }
                    if (!build_table(s.litlen, LitLenTableSize, LitLenRootBits, s.lengths, s.hlit, CodeKind::litlen, Wide)) {
                        fail("invalid literal/length code");
                        goto out;
                    }
                    if (!build_table(s.dist, DistTableSize, DistRootBits, s.lengths + s.hlit, s.hdist, CodeKind::distance, Wide)) {
                        fail("invalid distance code");
                        goto out;
                    }
                    lit_table = s.litlen;
                    dist_table = s.dist;
                    s.stage = InflateStage::codes;
                    break;
                }
                case InflateStage::codes: {
                    // The fast loop: at least 16 bytes of input (two refills
                    // of eight) and room for a match and a 16-byte overrun, so
                    // that neither is checked inside. A refill gives 56 bits:
                    // three literals (45), or a length and a distance with
                    // their extra bits (48). A refill adds bits above the
                    // ones there, so an entry looked up before it stays good.
                    auto refill = [&]() noexcept {
                        uint64_t word;
                        std::memcpy(&word, in, 8);
                        bitbuf |= word << bitcnt;
                        in += (63 - bitcnt) >> 3;
                        bitcnt |= 56;
                    };
                    constexpr uint32_t LitMask = (1u << LitLenRootBits) - 1;
                    // the bounds of the fast loop, once: the last input byte a
                    // refill may start at twice (Deflate64: three times), the
                    // last output position a match may start at
                    constexpr ptrdiff_t Slack = Wide ? 24 : 16;
                    const uint8_t* in_limit = in_end - in >= Slack ? in_end - Slack : in;
                    size_t out_limit = capacity >= MaxMatch + 16 ? capacity - MaxMatch - 16 : 0;
                    while (in < in_limit && p <= out_limit && capacity >= MaxMatch + 16) {
                        refill();
                        uint32_t e = lit_table[bitbuf & LitMask];
                        if (entry_kind(e) == EntryLiteral) {
                            take(entry_bits(e));
                            out[p++] = uint8_t(entry_value(e));
                            e = lit_table[bitbuf & LitMask];
                            if (entry_kind(e) == EntryLiteral) {
                                take(entry_bits(e));
                                out[p++] = uint8_t(entry_value(e));
                                e = lit_table[bitbuf & LitMask];
                                if (entry_kind(e) == EntryLiteral) {
                                    take(entry_bits(e));
                                    out[p++] = uint8_t(entry_value(e));
                                    continue;
                                }
                            }
                            refill();
                        }
                        if (entry_kind(e) == EntryLink) {
                            take(LitLenRootBits);
                            e = lit_table[entry_value(e) + (bitbuf & ((1u << entry_extra(e)) - 1))];
                        }
                        uint32_t kind = entry_kind(e);
                        if (kind == EntryLiteral) {
                            take(entry_bits(e));
                            out[p++] = uint8_t(entry_value(e));
                            continue;
                        }
                        if (kind != EntryLength) {
                            if (kind == EntryEnd) {
                                take(entry_bits(e));
                                s.stage = s.final_block ? InflateStage::done : InflateStage::header;
                                goto next_stage;
                            }
                            fail("invalid literal/length symbol");
                            goto out;
                        }
                        // the code and its extra bits in one shift; the extra bits read from before it
                        uint64_t saved = bitbuf;
                        uint32_t total = entry_bits(e);
                        uint32_t extra = entry_extra(e);
                        take(total);
                        uint32_t length = entry_value(e) + uint32_t((saved >> (total - extra)) & ((1u << extra) - 1));
                        if constexpr (Wide) {
                            // a length of 31 bits may leave fewer than a distance's 29
                            if (bitcnt < 29) {
                                refill();
                            }
                        }

                        uint32_t d = dist_table[bitbuf & ((1u << DistRootBits) - 1)];
                        if (entry_kind(d) == EntryLink) {
                            take(DistRootBits);
                            d = dist_table[entry_value(d) + (bitbuf & ((1u << entry_extra(d)) - 1))];
                        }
                        if (entry_kind(d) != EntryDistance) {
                            fail("invalid distance symbol");
                            goto out;
                        }
                        saved = bitbuf;
                        uint32_t dtotal = entry_bits(d);
                        uint32_t dextra = entry_extra(d);
                        take(dtotal);
                        uint32_t distance = entry_value(d) + uint32_t((saved >> (dtotal - dextra)) & ((1u << dextra) - 1));
                        if (distance > p) {
                            fail("distance before the start of the output");
                            goto out;
                        }
                        if constexpr (Wide) {
                            // longer than the room kept (p is at most capacity - 274)
                            if (length > capacity - p - 16) {
                                s.copy_length = length;
                                s.copy_distance = distance;
                                s.stage = InflateStage::copy;
                                goto next_stage;
                            }
                        }
                        uint8_t* dst = out + p;
                        const uint8_t* src = dst - distance;
                        uint8_t* end = dst + length;
                        p += length;
                        if (distance >= 16) {
                            // sixteen bytes at a time (one vector register); the room checked covers the overrun
                            do {
                                std::memcpy(dst, src, 16);
                                dst += 16;
                                src += 16;
                            } while (dst < end);
                        } else if (distance >= 8) {
                            do {
                                std::memcpy(dst, src, 8);
                                dst += 8;
                                src += 8;
                            } while (dst < end);
                        } else if (distance == 1) {
                            sgcl::detail::fill_bytes(dst, *src, length);
                        } else {
                            // a period shorter than a word (2 to 7): the first
                            // step bytes byte by byte, step the smallest
                            // multiple of the period of 8 bytes or more, then
                            // eight at a time from step bytes back, which is
                            // the same pattern and never the bytes being
                            // written (the overrun is under the room checked)
                            const uint32_t step = (8 + distance - 1) / distance * distance;
                            for (uint32_t i = 0; i < step; ++i) {
                                dst[i] = src[i];
                            }
                            uint8_t* w = dst + step;
                            while (w < end) {
                                std::memcpy(w, w - step, 8);
                                w += 8;
                            }
                        }
                    }
                    // the slow path: one symbol, every bound checked
                    {
                        uint32_t e, n;
                        if (!symbol(lit_table, LitLenRootBits, e, n)) {
                            goto out;
                        }
                        uint32_t kind = entry_kind(e);
                        if (kind == EntryLiteral) {
                            if (p == capacity) {
                                status = InflateStatus::need_room;
                                goto out;
                            }
                            out[p++] = uint8_t(entry_value(e));
                            take(n);
                            break;
                        }
                        if (kind == EntryEnd) {
                            take(n);
                            s.stage = s.final_block ? InflateStage::done : InflateStage::header;
                            break;
                        }
                        if (kind != EntryLength) {
                            fail("invalid literal/length symbol");
                            goto out;
                        }
                        // n has the extra bits in it, and symbol() waited for them
                        uint32_t extra = entry_extra(e);
                        uint64_t saved = bitbuf;
                        take(n);
                        s.copy_length = entry_value(e) + uint32_t((saved >> (n - extra)) & ((1u << extra) - 1));
                        s.stage = InflateStage::distance;
                    }
                    break;
                }
                case InflateStage::distance: {
                    uint32_t d, n;
                    if (!symbol(dist_table, DistRootBits, d, n)) {
                        goto out;
                    }
                    if (entry_kind(d) != EntryDistance) {
                        fail("invalid distance symbol");
                        goto out;
                    }
                    uint32_t extra = entry_extra(d);
                    uint64_t saved = bitbuf;
                    take(n);
                    uint32_t distance = entry_value(d) + uint32_t((saved >> (n - extra)) & ((1u << extra) - 1));
                    if (distance > p) {
                        fail("distance before the start of the output");
                        goto out;
                    }
                    s.copy_distance = distance;
                    s.stage = InflateStage::copy;
                    break;
                }
                case InflateStage::copy: {
                    size_t n = std::min<size_t>(s.copy_length, capacity - p);
                    const uint8_t* src = out + p - s.copy_distance;
                    if constexpr (Wide) {
                        // a match of up to 64 KB: in pieces no longer than its distance
                        for (size_t i = 0; i < n;) {
                            size_t k = std::min<size_t>(n - i, s.copy_distance);
                            sgcl::detail::copy_bytes(out + p + i, src + i, k);
                            i += k;
                        }
                    } else {
                        for (size_t i = 0; i < n; ++i) {
                            out[p + i] = src[i];
                        }
                    }
                    p += n;
                    s.copy_length -= uint32_t(n);
                    if (s.copy_length) {
                        status = InflateStatus::need_room;
                        goto out;
                    }
                    s.stage = InflateStage::codes;
                    break;
                }
                case InflateStage::done:
                    status = InflateStatus::done;
                    goto out;
            }
        next_stage:;
        }
    out:
        if (status == InflateStatus::done || status == InflateStatus::need_room) {
            // Between two pieces every whole byte held was read ahead, by
            // the fast loop of this call: back to the input. At the end the
            // bits of the last byte are padding.
            uint32_t back = std::min<uint32_t>(bitcnt >> 3, uint32_t(in - in_start));
            in -= back;
            bitcnt -= back * 8;
            bitbuf = bitcnt ? bitbuf & ((uint64_t(1) << bitcnt) - 1) : 0;
            if (status == InflateStatus::done) {
                bitbuf = 0;
                bitcnt = 0;
            }
        }
        s.bits = bitbuf;
        s.count = bitcnt;
        pos = p;
        return status;
    }

    SGCL_INLINE_HOT InflateStatus inflate(InflateState& s, const uint8_t*& in, const uint8_t* in_end, uint8_t* out, size_t& pos, size_t capacity) noexcept {
        return inflate_blocks<false>(s, in, in_end, out, pos, capacity);
    }

    // Deflate64: the history in front of pos must reach 64 KB
    SGCL_INLINE_HOT InflateStatus inflate64(InflateState& s, const uint8_t*& in, const uint8_t* in_end, uint8_t* out, size_t& pos, size_t capacity) noexcept {
        return inflate_blocks<true>(s, in, in_end, out, pos, capacity);
    }
}
