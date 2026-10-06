//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "brotli_dictionary.h"
#include "codec_stream.h"
#include "lz4_block.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Brotli's decoder (RFC 7932), written from the RFC.
//
// A stream is a header (the window: 2^WBITS - 16 bytes) and meta-blocks:
// metadata to skip, stored bytes, or compressed: per category (literals,
// insert-and-copy commands, distances) a number of block types with the
// prefix codes that switch them, the context modes of the literals' types
// and two context maps, then the prefix codes themselves and the commands.
// A command inserts literals (each coded by the tree its block type and the
// context of the two bytes before choose) and copies from a distance: one
// of the four last, near them, or coded; past what the window holds, a word
// of the static dictionary, transformed.
//
// The decoder can stop between any two commands, between two literals and
// inside a copy, where the input runs out or the room is full: a
// meta-block's header is read whole (or again from its start when more
// input comes), and every step of a command from a saved point it goes back
// to when the input ends inside it. In memory the room is the output, which
// grows; a stream's is a window of its own that slides.
namespace sgcl::compress::detail {
    // ---- the bit reader ------------------------------------------------------

    // Bits least significant first over bytes in memory; a read past the end
    // gives zeros and counts what was missing (the caller goes back to a
    // saved point, or fails at the end of the data)
    struct BrotliBits {
        const uint8_t* p = nullptr;     // the next byte to load
        const uint8_t* end = nullptr;
        uint64_t word = 0;              // the bits loaded, the next at bit 0
        unsigned count = 0;             // how many
        unsigned missing = 0;           // bits read past the end

        SGCL_INLINE_HOT void refill() noexcept {
            if (end - p >= 8) {
                uint64_t v;
                std::memcpy(&v, p, 8);
                if constexpr (std::endian::native == std::endian::big) {
                    v = __builtin_bswap64(v);
                }
                word |= v << count;
                p += (63 - count) >> 3;
                count |= 56;
            } else {
                while (count <= 56 && p < end) {
                    word |= uint64_t(*p++) << count;
                    count += 8;
                }
            }
        }

        // n <= 32 bits, after a refill
        SGCL_INLINE_HOT uint32_t peek(unsigned n) const noexcept {
            return uint32_t(word & ((uint64_t(1) << n) - 1));
        }

        SGCL_INLINE_HOT void skip(unsigned n) noexcept {
            if (n > count) {
                missing += n - count;
                word = 0;
                count = 0;
                return;
            }
            word >>= n;
            count -= n;
        }

        SGCL_INLINE_HOT uint32_t read(unsigned n) noexcept {
            if (count < n) {
                refill();
            }
            const uint32_t v = peek(n);
            skip(n);
            return v;
        }

        // The bits read from data
        SGCL_INLINE_HOT uint64_t position(const uint8_t* data) const noexcept {
            return uint64_t(p - data) * 8 - count + missing;
        }

        void start(const uint8_t* data, size_t n, uint64_t bit) noexcept {
            p = data + bit / 8;
            end = data + n;
            word = 0;
            count = 0;
            missing = 0;
            refill();
            skip(unsigned(bit % 8));
        }
    };

    // ---- prefix codes ----------------------------------------------------------

    // A decoding table: 2^8 entries for the first 8 bits, and for a longer
    // code a second table its root entry names (bits > 8: value is the
    // second table's start from the root, bits - 8 its index bits)
    struct BrotliEntry {
        uint16_t value;
        uint8_t bits;
    };

    inline constexpr unsigned BrotliRootBits = 8;

    SGCL_INLINE_HOT unsigned brotli_reverse(unsigned code, unsigned bits) noexcept {
        return unsigned(__builtin_bitreverse32(code) >> (32 - bits));
    }

    // The table of a complete code of the given lengths appended to pool,
    // or of the one symbol `single` of no bits when used is 1; its start
    inline uint32_t brotli_build_table(std::vector<BrotliEntry>& pool, const uint8_t* lengths, unsigned alphabet, unsigned single = 0,
                                       bool one = false) noexcept {
        const uint32_t start = uint32_t(pool.size());
        pool.resize(start + (size_t(1) << BrotliRootBits));
        if (one) {
            for (unsigned i = 0; i < (1u << BrotliRootBits); ++i) {
                pool[start + i] = {uint16_t(single), 0};
            }
            return start;
        }
        unsigned counts[16] = {};
        for (unsigned s = 0; s < alphabet; ++s) {
            ++counts[lengths[s]];
        }
        counts[0] = 0;
        unsigned first[16];
        unsigned code = 0;
        for (unsigned l = 1; l < 16; ++l) {
            code = (code + counts[l - 1]) << 1;
            first[l] = code;
        }
        // the second tables: for each 8-bit prefix of a longer code, as deep
        // as its longest code
        uint8_t sub_bits[256] = {};
        unsigned next[16];
        std::copy(first, first + 16, next);
        for (unsigned s = 0; s < alphabet; ++s) {
            const unsigned l = lengths[s];
            if (l > BrotliRootBits) {
                const unsigned rev = brotli_reverse(next[l]++, l);
                const unsigned low = rev & ((1u << BrotliRootBits) - 1);
                sub_bits[low] = uint8_t(std::max<unsigned>(sub_bits[low], l - BrotliRootBits));
            }
        }
        for (unsigned i = 0; i < 256; ++i) {
            if (sub_bits[i]) {
                const size_t at = pool.size() - start;
                pool.resize(pool.size() + (size_t(1) << sub_bits[i]));
                pool[start + i] = {uint16_t(at), uint8_t(BrotliRootBits + sub_bits[i])};
            }
        }
        BrotliEntry* root = pool.data() + start;
        std::copy(first, first + 16, next);
        for (unsigned s = 0; s < alphabet; ++s) {
            const unsigned l = lengths[s];
            if (!l) {
                continue;
            }
            const unsigned rev = brotli_reverse(next[l]++, l);
            if (l <= BrotliRootBits) {
                for (unsigned i = rev; i < (1u << BrotliRootBits); i += 1u << l) {
                    root[i] = {uint16_t(s), uint8_t(l)};
                }
            } else {
                const BrotliEntry link = root[rev & ((1u << BrotliRootBits) - 1)];
                const unsigned sb = link.bits - BrotliRootBits;
                const unsigned rl = l - BrotliRootBits;
                BrotliEntry* sub = root + link.value;
                for (unsigned i = rev >> BrotliRootBits; i < (1u << sb); i += 1u << rl) {
                    sub[i] = {uint16_t(s), uint8_t(rl)};
                }
            }
        }
        return start;
    }

    // One symbol by a table
    SGCL_INLINE_HOT unsigned brotli_symbol(const BrotliEntry* t, BrotliBits& b) noexcept {
        if (b.count < 15) {
            b.refill();
        }
        BrotliEntry e = t[b.peek(BrotliRootBits)];
        if (e.bits > BrotliRootBits) {
            const unsigned idx = unsigned(b.word >> BrotliRootBits) & ((1u << (e.bits - BrotliRootBits)) - 1);
            b.skip(BrotliRootBits);
            e = t[e.value + idx];
        }
        b.skip(e.bits);
        return e.value;
    }

    // The order the code lengths of the code-length code come in
    inline constexpr uint8_t BrotliCodeLengthOrder[18] = {1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    // Reads a prefix code for an alphabet (RFC 7932, 3.4 and 3.5; symbols
    // from max_symbol on are not valid) and appends its table to pool;
    // false when it is not a valid code (or the input ran short)
    inline bool brotli_read_code(BrotliBits& b, unsigned alphabet, unsigned max_symbol, std::vector<BrotliEntry>& pool, uint32_t& table) noexcept {
        uint8_t lengths[704];
        std::fill(lengths, lengths + alphabet, uint8_t(0));
        const unsigned hskip = b.read(2);
        if (hskip == 1) {
            // a simple code: 1 to 4 symbols of ALPHABET_BITS each
            unsigned bits = 0;
            while ((1u << bits) < alphabet) {
                ++bits;
            }
            const unsigned nsym = b.read(2) + 1;
            unsigned sym[4];
            for (unsigned i = 0; i < nsym; ++i) {
                sym[i] = b.read(bits);
                if (sym[i] >= max_symbol) {
                    return false;
                }
                for (unsigned j = 0; j < i; ++j) {
                    if (sym[j] == sym[i]) {
                        return false;
                    }
                }
            }
            if (nsym == 1) {
                table = brotli_build_table(pool, lengths, alphabet, sym[0], true);
                return true;
            }
            if (nsym == 2) {
                lengths[sym[0]] = 1;
                lengths[sym[1]] = 1;
            } else if (nsym == 3) {
                lengths[sym[0]] = 1;
                lengths[sym[1]] = 2;
                lengths[sym[2]] = 2;
            } else if (b.read(1)) {
                lengths[sym[0]] = 1;
                lengths[sym[1]] = 2;
                lengths[sym[2]] = 3;
                lengths[sym[3]] = 3;
            } else {
                for (unsigned i = 0; i < 4; ++i) {
                    lengths[sym[i]] = 2;
                }
            }
            table = brotli_build_table(pool, lengths, alphabet);
            return true;
        }
        // a complex code: the code-length code's lengths, then the lengths
        uint8_t cl[18] = {};
        unsigned space = 32;
        unsigned nonzero = 0;
        unsigned last_nonzero = 0;
        for (unsigned i = hskip; i < 18; ++i) {
            if (b.count < 4) {
                b.refill();
            }
            // the code of the code-length code lengths, canonical of the
            // lengths 0:2 1:4 2:3 3:2 4:2 5:4 (by four bits read: their length and value)
            static constexpr uint8_t len_of[16] = {2, 2, 2, 3, 2, 2, 2, 4, 2, 2, 2, 3, 2, 2, 2, 4};
            static constexpr uint8_t value_of[16] = {0, 4, 3, 2, 0, 4, 3, 1, 0, 4, 3, 2, 0, 4, 3, 5};
            const unsigned v = b.peek(4);
            b.skip(len_of[v]);
            const unsigned l = value_of[v];
            cl[BrotliCodeLengthOrder[i]] = uint8_t(l);
            if (l) {
                space -= 32u >> l;
                ++nonzero;
                last_nonzero = BrotliCodeLengthOrder[i];
                if (space - 1u >= 32u) {   // 0, or wrapped past it
                    break;
                }
            }
        }
        if (!(nonzero == 1 || space == 0)) {
            return false;
        }
        BrotliEntry cl_table[1 << BrotliRootBits];
        {
            std::vector<BrotliEntry> tmp;
            tmp.reserve(1 << BrotliRootBits);
            const uint32_t t = nonzero == 1 ? brotli_build_table(tmp, cl, 18, last_nonzero, true) : brotli_build_table(tmp, cl, 18);
            std::copy(tmp.begin() + t, tmp.begin() + t + (1 << BrotliRootBits), cl_table);
        }
        unsigned s = 0;
        unsigned prev = 8;               // the last non-zero length
        unsigned repeat = 0;             // the count of the repeat codes before
        unsigned repeat_len = 0;         // what they repeat
        unsigned left = 32768;           // the code space left, in 1/32768
        unsigned used = 0;
        while (s < alphabet && left) {
            const unsigned c = brotli_symbol(cl_table, b);
            if (b.missing) {
                return false;
            }
            if (c < 16) {
                repeat = 0;
                lengths[s++] = uint8_t(c);
                if (c) {
                    prev = c;
                    ++used;
                    if ((32768u >> c) > left) {
                        return false;
                    }
                    left -= 32768u >> c;
                }
                continue;
            }
            const unsigned extra = c == 16 ? 2 : 3;
            const unsigned new_len = c == 16 ? prev : 0;
            if (repeat_len != new_len) {
                repeat = 0;
                repeat_len = new_len;
            }
            const unsigned old = repeat;
            if (repeat > 0) {
                repeat = (repeat - 2) << extra;
            }
            repeat += b.read(extra) + 3;
            const unsigned delta = repeat - old;
            if (s + delta > alphabet) {
                return false;
            }
            for (unsigned i = 0; i < delta; ++i) {
                lengths[s++] = uint8_t(repeat_len);
            }
            if (repeat_len) {
                used += delta;
                const unsigned take = delta * (32768u >> repeat_len);
                if (take > left) {
                    return false;
                }
                left -= take;
            }
        }
        if (left != 0 || used < 2) {
            return false;
        }
        for (unsigned i = max_symbol; i < alphabet; ++i) {
            if (lengths[i]) {
                return false;
            }
        }
        table = brotli_build_table(pool, lengths, alphabet);
        return true;
    }

    // ---- the tables of the RFC ---------------------------------------------------

    // The block counts' codes: base and extra bits (RFC 7932, 6)
    inline constexpr uint16_t BrotliCountBase[26] = {1,   5,   9,   13,  17,  25,  33,  41,  49,   65,   81,   97,   113,
                                                     145, 177, 209, 241, 305, 369, 497, 753, 1265, 2289, 4337, 8433, 16625};
    inline constexpr uint8_t BrotliCountExtra[26] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 8, 9, 10, 11, 12, 13, 24};

    // The insert and copy lengths' codes (RFC 7932, 5)
    inline constexpr uint32_t BrotliInsertBase[24] = {0,  1,  2,   3,   4,   5,   6,   8,    10,   14,   18,   26,
                                                      34, 50, 66, 98, 130, 194, 322, 578, 1090, 2114, 6210, 22594};
    inline constexpr uint8_t BrotliInsertExtra[24] = {0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 12, 14, 24};
    inline constexpr uint32_t BrotliCopyBase[24] = {2,  3,  4,  5,  6,  7,   8,   9,   10,  12,  14,   18,
                                                    22, 30, 38, 54, 70, 102, 134, 198, 326, 582, 1094, 2118};
    inline constexpr uint8_t BrotliCopyExtra[24] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9, 10, 24};

    // A command's cell (its code >> 6): the first insert code and copy code
    // (cells 0 and 1 copy from the last distance, read no distance code)
    inline constexpr uint8_t BrotliCellInsert[11] = {0, 0, 0, 0, 8, 8, 0, 16, 8, 16, 16};
    inline constexpr uint8_t BrotliCellCopy[11] = {0, 8, 0, 8, 0, 8, 16, 0, 16, 8, 16};

    // The distance codes 0..15: which of the last four, and the change
    inline constexpr uint8_t BrotliDistanceIndex[16] = {0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1};
    inline constexpr int8_t BrotliDistanceDelta[16] = {0, 0, 0, 0, -1, 1, -2, 2, -3, 3, -1, 1, -2, 2, -3, 3};

    // A byte of the static dictionary
    SGCL_INLINE_HOT uint8_t brotli_dictionary_byte(uint32_t i) noexcept {
        return uint8_t(BrotliDictionaryWords[i >> 3] >> (8 * (i & 7)));
    }

    // A dictionary word of len bytes at offset, transformed into dst (64
    // bytes of room); its length (RFC 7932, 8)
    inline unsigned brotli_transform(uint8_t* dst, uint32_t word, unsigned len, unsigned transform) noexcept {
        const BrotliTransform& t = BrotliTransforms[transform];
        unsigned at = 0;
        for (const char* q = t.prefix; *q; ++q) {
            dst[at++] = uint8_t(*q);
        }
        const unsigned type = t.type;
        unsigned skip = 0;
        unsigned n = len;
        if (type >= 1 && type <= 9) {
            n = len > type ? len - type : 0;
        } else if (type >= 12 && type <= 20) {
            skip = std::min(type - 11, len);
            n = len - skip;
        }
        const unsigned word_at = at;
        for (unsigned i = 0; i < n; ++i) {
            dst[at++] = brotli_dictionary_byte(word + skip + i);
        }
        // the upper case of the first letter, or of every one: an ASCII
        // byte, the second byte of a 2-byte UTF-8 sequence, the third of a
        // 3-byte one
        auto upper = [](uint8_t* p) noexcept -> unsigned {
            if (p[0] < 0xC0) {
                if (p[0] >= 'a' && p[0] <= 'z') {
                    p[0] ^= 32;
                }
                return 1;
            }
            if (p[0] < 0xE0) {
                p[1] ^= 32;
                return 2;
            }
            p[2] ^= 5;
            return 3;
        };
        if (type == 10 && n) {
            upper(dst + word_at);
        } else if (type == 11) {
            int left = int(n);
            uint8_t* p = dst + word_at;
            while (left > 0) {
                const unsigned step = upper(p);
                p += step;
                left -= int(step);
            }
        }
        for (const char* q = t.suffix; *q; ++q) {
            dst[at++] = uint8_t(*q);
        }
        return at;
    }

    // ---- the decoder -----------------------------------------------------------

    // A category's block types: the codes that switch them, the type and
    // the one before, the blocks left of the current type
    struct BrotliBlocks {
        unsigned types = 1;
        uint32_t type_code = 0;
        uint32_t count_code = 0;
        unsigned type = 0;
        unsigned last = 1;
        uint32_t left = 0;
    };

    class BrotliDecoder {
    public:
        static constexpr const char* name = "brotli";

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        template<class O>
        BrotliDecoder(const O&, const limits& l) noexcept
        : _max_memory(l.max_memory) {
        }

        explicit BrotliDecoder(const limits& l) noexcept
        : _max_memory(l.max_memory) {
        }

        void reset() noexcept {
            _state = State::header;
            _in.clear();
            _bit = 0;
            _dropped = 0;
            _total = 0;
            _pos = 0;
            _handed = 0;
            _window = 0;
            _phase = Phase::command;
            _last = false;
            _dist[0] = 4;    // the last distances 4, 11, 15, 16 (the last first)
            _dist[1] = 16;
            _dist[2] = 15;
            _dist[3] = 11;
            _dist_at = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return (_dropped * 8 + _bit) / 8;
        }

        // The pushed form (codec_stream.h)
        CodecStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept {
            if (in != end) {
                const size_t used = size_t(_bit / 8);
                if (used > (size_t(1) << 16) && used * 2 > _in.size()) {
                    _in.erase(_in.begin(), _in.begin() + ptrdiff_t(used));
                    _bit -= uint64_t(used) * 8;
                    _dropped += used;
                }
                _in.insert(_in.end(), in, end);
                in = end;
            }
            for (;;) {
                if (_handed < _pos) {
                    const size_t n = std::min(_pos - _handed, cap - pos);
                    sgcl::detail::copy_bytes(out + pos, _out + _handed, n);
                    pos += n;
                    _handed += n;
                    if (_handed < _pos) {
                        return CodecStatus::need_room;
                    }
                }
                if (_state == State::done) {
                    return CodecStatus::done;
                }
                if (_state == State::failed) {
                    return CodecStatus::failed;
                }
                // the window slides: its last window's worth stays
                if (_out && _cap - _pos < Portion / 2) {
                    const size_t keep = std::min(_pos, _window);
                    sgcl::detail::move_bytes(_out, _out + _pos - keep, keep);
                    _handed -= _pos - keep;
                    _pos = keep;
                }
                const size_t before = _pos;
                const State state_before = _state;
                const Step st = _run(_in.data(), _in.size());
                if (st == Step::failed) {
                    _state = State::failed;
                    return CodecStatus::failed;
                }
                if (st == Step::need_input && _pos == before && _state == state_before) {
                    if (final) {
                        error = errc::unexpected_end;
                        error_text = "brotli: unexpected end of data";
                        _state = State::failed;
                        return CodecStatus::failed;
                    }
                    return CodecStatus::need_input;
                }
            }
        }

        // In memory: the whole stream into a vector that grows (from a
        // guess, doubling) up to one byte past max_size
        expected<vector<byte>, compress::error> decompress_all(const uint8_t* p, size_t n, const limits& l) noexcept {
            reset();
            _memory = true;
            const uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
            vector<byte> result;
            size_t capacity = size_t(std::min<uint64_t>(std::max<uint64_t>(uint64_t(n) * 4, 1 << 16), ceiling));
            sgcl::detail::VectorOverwrite::resize(result, capacity + Slack);   // every byte written before it is returned: resize(total) below
            _out = reinterpret_cast<uint8_t*>(result.data());
            _cap = capacity;
            for (;;) {
                const Step st = _run(p, n);
                if (st == Step::failed) {
                    return unexpected<compress::error>(compress::error(error, offset(), string(error_text)));
                }
                if (_state == State::done) {
                    break;
                }
                if (st == Step::need_input) {
                    return unexpected<compress::error>(compress::error(errc::unexpected_end, n, string("brotli: unexpected end of data")));
                }
                if (_cap - _pos < Portion / 2 || _cap == _pos) {
                    // the room: doubled, up to one byte past the limit
                    if (_cap >= ceiling) {
                        return unexpected<compress::error>(compress::error(errc::too_large, offset(), string("brotli: decompressed data past the limit")));
                    }
                    const size_t grown = size_t(std::min<uint64_t>(uint64_t(_cap) * 2, ceiling));
                    vector<byte> bigger;
                    sgcl::detail::VectorOverwrite::resize(bigger, grown + Slack);
                    copy_out(bigger.data(), result.data(), _pos);
                    result = std::move(bigger);
                    _out = reinterpret_cast<uint8_t*>(result.data());
                    _cap = grown;
                }
            }
            if (_total > l.max_size) {
                return unexpected<compress::error>(compress::error(errc::too_large, offset(), string("brotli: decompressed data past the limit")));
            }
            if (_bit / 8 != n) {
                return unexpected<compress::error>(compress::error(errc::corrupt, offset(), string("brotli: bytes after the end of the stream")));
            }
            result.resize(_pos);
            return result;
        }

    private:
        enum class State : uint8_t {
            header,
            meta_header,
            metadata,
            stored,
            commands,
            done,
            failed
        };

        enum class Step : uint8_t {
            progress,     // something done: output made, or a state passed
            need_input,
            failed
        };

        enum class Phase : uint8_t {
            command,
            literals,
            distance,
            copy
        };

        static constexpr size_t Slack = 64;                   // room past the end for wide stores
        static constexpr size_t Portion = size_t(1) << 18;    // the stream's window: room past the history

        Step _fail(const char* text, errc code = errc::corrupt) noexcept {
            error = code;
            error_text = text;
            return Step::failed;
        }

        // the meta-block's tables
        struct Meta {
            uint32_t left = 0;            // of MLEN
            BrotliBlocks blocks[3];       // literals, commands, distances
            unsigned npostfix = 0;
            unsigned ndirect = 0;
            uint8_t modes[256];
            std::vector<uint8_t> lit_map;    // 64 per literal type
            std::vector<uint8_t> single;     // a literal type's 64 contexts all of one tree
            std::vector<uint8_t> dist_map;   // 4 per distance type
            std::vector<uint32_t> lit_trees, cmd_trees, dist_trees;
            std::vector<BrotliEntry> pool;
        };

        // What a step of a command changes, saved before it
        struct Mark {
            uint64_t bit;
            unsigned type[3], last[3];
            uint32_t left[3];
            uint32_t dist[4];
            unsigned dist_at;
            uint32_t meta_left;
            size_t pos;
            uint64_t total;
        };

        SGCL_INLINE_HOT void _save(Mark& m, const BrotliBits& b, const uint8_t* data) const noexcept {
            m.bit = b.position(data);
            for (int i = 0; i < 3; ++i) {
                m.type[i] = _m.blocks[i].type;
                m.last[i] = _m.blocks[i].last;
                m.left[i] = _m.blocks[i].left;
            }
            std::copy(_dist, _dist + 4, m.dist);
            m.dist_at = _dist_at;
            m.meta_left = _m.left;
            m.pos = _pos;
            m.total = _total;
        }

        SGCL_INLINE_HOT void _restore(const Mark& m, BrotliBits& b, const uint8_t* data, size_t n) noexcept {
            for (int i = 0; i < 3; ++i) {
                _m.blocks[i].type = m.type[i];
                _m.blocks[i].last = m.last[i];
                _m.blocks[i].left = m.left[i];
            }
            std::copy(m.dist, m.dist + 4, _dist);
            _dist_at = m.dist_at;
            _m.left = m.meta_left;
            _pos = m.pos;
            _total = m.total;
            b.start(data, n, m.bit);
        }

        bool _make_window(unsigned wbits) noexcept {
            _window = (size_t(1) << wbits) - 16;
            if (_window > _max_memory) {
                return false;
            }
            if (_memory) {
                return true;
            }
            const size_t cap = _window + Portion;
            if (_own_cap < cap) {
                _own.reset(new uint8_t[cap + Slack]);
                _own_cap = cap;
            }
            _out = _own.get();
            _cap = cap;
            _pos = 0;
            _handed = 0;
            return true;
        }

        static uint32_t _var8(BrotliBits& b) noexcept {
            if (!b.read(1)) {
                return 0;
            }
            const unsigned n = b.read(3);
            return n == 0 ? 1 : b.read(n) + (1u << n);
        }

        bool _read_blocks(BrotliBits& b, unsigned c) noexcept {
            BrotliBlocks& k = _m.blocks[c];
            k = BrotliBlocks {};
            k.types = _var8(b) + 1;
            if (k.types < 2) {
                k.left = 0xFFFFFFFFu;
                return true;
            }
            if (!brotli_read_code(b, k.types + 2, k.types + 2, _m.pool, k.type_code) || !brotli_read_code(b, 26, 26, _m.pool, k.count_code)) {
                return false;
            }
            const unsigned sym = brotli_symbol(_m.pool.data() + k.count_code, b);
            k.left = BrotliCountBase[sym] + b.read(BrotliCountExtra[sym]);
            return true;
        }

        void _switch(BrotliBits& b, unsigned c) noexcept {
            BrotliBlocks& k = _m.blocks[c];
            const unsigned t = brotli_symbol(_m.pool.data() + k.type_code, b);
            unsigned next = t == 0 ? k.last : t == 1 ? k.type + 1 : t - 2;
            if (next >= k.types) {
                next -= k.types;
            }
            k.last = k.type;
            k.type = next;
            const unsigned sym = brotli_symbol(_m.pool.data() + k.count_code, b);
            k.left = BrotliCountBase[sym] + b.read(BrotliCountExtra[sym]);
        }

        bool _read_map(BrotliBits& b, std::vector<uint8_t>& map, size_t size, unsigned& trees) noexcept {
            trees = _var8(b) + 1;
            map.assign(size, 0);
            if (trees < 2) {
                return true;
            }
            unsigned rle = 0;
            if (b.read(1)) {
                rle = b.read(4) + 1;
            }
            uint32_t t = 0;
            if (!brotli_read_code(b, trees + rle, trees + rle, _m.pool, t)) {
                return false;
            }
            const BrotliEntry* table = _m.pool.data() + t;
            size_t i = 0;
            while (i < size) {
                if (b.missing) {
                    return false;
                }
                const unsigned sym = brotli_symbol(table, b);
                if (sym == 0) {
                    map[i++] = 0;
                } else if (sym <= rle) {
                    const size_t run = (size_t(1) << sym) + b.read(sym);
                    if (i + run > size) {
                        return false;
                    }
                    i += run;   // zeros already
                } else {
                    map[i++] = uint8_t(sym - rle);
                }
            }
            if (b.read(1)) {
                // the inverse move-to-front transform
                uint8_t mtf[256];
                for (unsigned k = 0; k < 256; ++k) {
                    mtf[k] = uint8_t(k);
                }
                for (auto& v : map) {
                    const unsigned idx = v;
                    const uint8_t value = mtf[idx];
                    v = value;
                    for (unsigned k = idx; k > 0; --k) {
                        mtf[k] = mtf[k - 1];
                    }
                    mtf[0] = value;
                }
            }
            for (auto v : map) {
                if (v >= trees) {
                    return false;
                }
            }
            return true;
        }

        // A compressed meta-block's header after MLEN: false when not valid
        bool _read_compressed(BrotliBits& b) noexcept {
            _m.pool.clear();
            for (unsigned c = 0; c < 3; ++c) {
                if (!_read_blocks(b, c)) {
                    return false;
                }
            }
            _m.npostfix = b.read(2);
            _m.ndirect = b.read(4) << _m.npostfix;
            for (unsigned i = 0; i < _m.blocks[0].types; ++i) {
                _m.modes[i] = uint8_t(b.read(2));
            }
            unsigned lit_trees = 0, dist_trees = 0;
            if (!_read_map(b, _m.lit_map, size_t(64) * _m.blocks[0].types, lit_trees) ||
                !_read_map(b, _m.dist_map, size_t(4) * _m.blocks[2].types, dist_trees)) {
                return false;
            }
            _m.single.resize(_m.blocks[0].types);
            for (unsigned t = 0; t < _m.blocks[0].types; ++t) {
                const uint8_t* row = _m.lit_map.data() + size_t(t) * 64;
                _m.single[t] = std::all_of(row, row + 64, [&](uint8_t v) noexcept { return v == row[0]; });
            }
            _m.lit_trees.resize(lit_trees);
            for (auto& t : _m.lit_trees) {
                if (!brotli_read_code(b, 256, 256, _m.pool, t)) {
                    return false;
                }
            }
            _m.cmd_trees.resize(_m.blocks[1].types);
            for (auto& t : _m.cmd_trees) {
                if (!brotli_read_code(b, 704, 704, _m.pool, t)) {
                    return false;
                }
            }
            const unsigned dist_alphabet = 16 + _m.ndirect + (48u << _m.npostfix);
            _m.dist_trees.resize(dist_trees);
            for (auto& t : _m.dist_trees) {
                if (!brotli_read_code(b, dist_alphabet, dist_alphabet, _m.pool, t)) {
                    return false;
                }
            }
            return !b.missing;
        }

        // A meta-block's header, whole
        Step _meta_header(BrotliBits& b, const uint8_t* data, size_t n) noexcept {
            const uint64_t at = b.position(data);
            auto again = [&]() noexcept {
                b.start(data, n, at);
                return Step::need_input;
            };
            _last = b.read(1);
            if (_last && b.read(1)) {
                // ISLASTEMPTY: the end, padded to a byte with zeros
                if (b.read(unsigned((8 - b.position(data) % 8) % 8)) != 0) {
                    return b.missing ? again() : _fail("brotli: padding bits that are not zero");
                }
                if (b.missing) {
                    return again();
                }
                _state = State::done;
                return Step::progress;
            }
            const unsigned nibbles = b.read(2);
            if (nibbles == 3) {
                // metadata: skipped
                if (b.read(1) != 0) {
                    return b.missing ? again() : _fail("brotli: a reserved bit set");
                }
                const unsigned skip_bytes = b.read(2);
                uint32_t skip = 0;
                for (unsigned i = 0; i < skip_bytes; ++i) {
                    const uint32_t v = b.read(8);
                    if (i + 1 == skip_bytes && skip_bytes > 1 && v == 0) {
                        return b.missing ? again() : _fail("brotli: a metadata length with a last byte of zero");
                    }
                    skip |= v << (8 * i);
                }
                if (skip_bytes) {
                    ++skip;
                }
                if (b.read(unsigned((8 - b.position(data) % 8) % 8)) != 0) {
                    return b.missing ? again() : _fail("brotli: padding bits that are not zero");
                }
                if (b.missing) {
                    return again();
                }
                _m.left = skip;
                _state = State::metadata;
                return Step::progress;
            }
            const unsigned count = nibbles + 4;
            uint32_t mlen = 0;
            for (unsigned i = 0; i < count; ++i) {
                const uint32_t v = b.read(4);
                if (i + 1 == count && count > 4 && v == 0) {
                    return b.missing ? again() : _fail("brotli: a meta-block length with a last nibble of zero");
                }
                mlen |= v << (4 * i);
            }
            _m.left = mlen + 1;
            if (!_last && b.read(1)) {
                // stored: padded to a byte with zeros, then the bytes
                if (b.read(unsigned((8 - b.position(data) % 8) % 8)) != 0) {
                    return b.missing ? again() : _fail("brotli: padding bits that are not zero");
                }
                if (b.missing) {
                    return again();
                }
                _state = State::stored;
                return Step::progress;
            }
            if (!_read_compressed(b)) {
                return b.missing ? again() : _fail("brotli: a meta-block header that is not valid");
            }
            _state = State::commands;
            _phase = Phase::command;
            return Step::progress;
        }

        // The room left, for the in-memory form (the stream's window slides
        // before it fills)
        SGCL_INLINE_HOT size_t _room() const noexcept {
            return _cap - _pos;
        }

        Step _commands(BrotliBits& b, const uint8_t* data, size_t n) noexcept;

        Step _run(const uint8_t* data, size_t n) noexcept {
            BrotliBits b;
            b.start(data, n, _bit);
            Step result = Step::progress;
            bool any = false;
            for (;;) {
                Step st = Step::progress;
                switch (_state) {
                    case State::header: {
                        // the window bits (RFC 7932, 9.1)
                        const uint64_t at = b.position(data);
                        unsigned wbits;
                        if (!b.read(1)) {
                            wbits = 16;
                        } else if (const unsigned v = b.read(3); v) {
                            wbits = 17 + v;
                        } else if (const unsigned w = b.read(3); w == 1) {
                            if (!b.missing) {
                                return _fail("brotli: a window of the large-window extension (not RFC 7932)", errc::invalid_header);
                            }
                            wbits = 0;
                        } else {
                            wbits = w ? 8 + w : 17;
                        }
                        if (b.missing) {
                            b.start(data, n, at);
                            st = Step::need_input;
                            break;
                        }
                        if (!_make_window(wbits)) {
                            return _fail("brotli: the window needs more memory than the limit allows", errc::too_large);
                        }
                        _state = State::meta_header;
                        break;
                    }
                    case State::meta_header:
                        st = _meta_header(b, data, n);
                        break;
                    case State::metadata: {
                        // skipped bytes, at a byte boundary
                        const uint64_t at = b.position(data) / 8;
                        const size_t take = size_t(std::min<uint64_t>(_m.left, n - at));
                        b.start(data, n, (at + take) * 8);
                        _m.left -= uint32_t(take);
                        if (_m.left) {
                            st = Step::need_input;
                        } else {
                            _state = _last ? State::done : State::meta_header;
                        }
                        break;
                    }
                    case State::stored: {
                        const uint64_t at = b.position(data) / 8;
                        const size_t take = size_t(std::min<uint64_t>({uint64_t(_m.left), n - at, uint64_t(_room())}));
                        sgcl::detail::copy_bytes(_out + _pos, data + at, take);
                        _pos += take;
                        _total += take;
                        b.start(data, n, (at + take) * 8);
                        _m.left -= uint32_t(take);
                        if (_m.left == 0) {
                            _state = State::meta_header;
                        } else if (_room() == 0) {
                            st = Step::progress;
                            _bit = b.position(data);
                            return Step::progress;   // the room is full
                        } else {
                            st = Step::need_input;
                        }
                        break;
                    }
                    case State::commands:
                        st = _commands(b, data, n);
                        if (st == Step::progress && _m.left == 0 && _phase == Phase::command) {
                            if (_last) {
                                // the end, padded to a byte with zeros
                                const uint64_t at = b.position(data);
                                if (b.read(unsigned((8 - at % 8) % 8)) != 0) {
                                    return _fail("brotli: padding bits that are not zero");
                                }
                                if (b.missing) {
                                    b.start(data, n, at);
                                    st = Step::need_input;
                                    break;
                                }
                                _state = State::done;
                            } else {
                                _state = State::meta_header;
                            }
                        } else if (st == Step::progress) {
                            _bit = b.position(data);
                            return Step::progress;   // the room is full
                        }
                        break;
                    case State::done:
                    case State::failed:
                        _bit = b.position(data);
                        return any ? Step::progress : result;
                }
                if (st == Step::failed) {
                    return Step::failed;
                }
                if (st == Step::need_input) {
                    _bit = b.position(data);
                    return Step::need_input;
                }
                any = true;
            }
        }

        std::vector<uint8_t> _in;
        uint64_t _bit = 0;          // the bits of _in (or of the data in memory) read
        uint64_t _dropped = 0;      // the bytes of input dropped from _in's front
        uint64_t _max_memory = uint64_t(1) << 30;
        State _state = State::header;
        bool _memory = false;
        std::unique_ptr<uint8_t[]> _own;
        size_t _own_cap = 0;
        uint8_t* _out = nullptr;    // the room: the window of a stream, the output in memory
        size_t _cap = 0;
        size_t _window = 0;
        size_t _pos = 0;
        size_t _handed = 0;
        uint64_t _total = 0;
        bool _last = false;
        Meta _m;
        uint32_t _dist[4] = {4, 16, 15, 11};   // the last at _dist_at, the ones before at _dist_at - 1, - 2, - 3
        unsigned _dist_at = 0;   // the last distance: _dist[_dist_at & 3]
        // a command part way
        Phase _phase = Phase::command;
        uint32_t _insert_left = 0;
        uint32_t _copy_len = 0;
        uint32_t _copy_left = 0;
        uint32_t _distance = 0;
        bool _implicit = false;
        bool _from_word = false;
        uint8_t _word[64];
        unsigned _word_at = 0;
    };

    // The commands of a compressed meta-block, as far as the input and the
    // room go: progress when the meta-block ended or the room filled
    inline BrotliDecoder::Step BrotliDecoder::_commands(BrotliBits& b, const uint8_t* data, size_t n) noexcept {
        Mark mark;
        uint8_t* const out = _out;
        for (;;) {
            switch (_phase) {
                case Phase::command: {
                    if (_m.left == 0) {
                        return Step::progress;
                    }
                    const bool careful = b.end - b.p < 32;   // else a command's bits (117 at most) are there
                    if (careful) {
                        _save(mark, b, data);
                    }
                    BrotliBlocks& k = _m.blocks[1];
                    if (k.left == 0) {
                        _switch(b, 1);
                    }
                    --k.left;
                    const unsigned cmd = brotli_symbol(_m.pool.data() + _m.cmd_trees[k.type], b);
                    const unsigned cell = cmd >> 6;
                    const unsigned ic = BrotliCellInsert[cell] + ((cmd >> 3) & 7);
                    const unsigned cc = BrotliCellCopy[cell] + (cmd & 7);
                    if (b.count < 48) {
                        b.refill();
                    }
                    const uint32_t insert = BrotliInsertBase[ic] + b.read(BrotliInsertExtra[ic]);
                    const uint32_t copy = BrotliCopyBase[cc] + b.read(BrotliCopyExtra[cc]);
                    if (careful && b.missing) {
                        _restore(mark, b, data, n);
                        return Step::need_input;
                    }
                    if (insert > _m.left) {
                        return _fail("brotli: a command inserts more than its meta-block holds");
                    }
                    _insert_left = insert;
                    _copy_len = copy;
                    _implicit = cell < 2;
                    _phase = Phase::literals;
                    [[fallthrough]];
                }
                case Phase::literals: {
                    BrotliBlocks& k = _m.blocks[0];
                    const BrotliEntry* const pool = _m.pool.data();
                    while (_insert_left) {
                        if (_pos == _cap) {
                            return Step::progress;
                        }
                        if (k.left == 0 || b.end - b.p < 32) {
                            // one literal with a saved point: a switch, or the
                            // input's end near
                            _save(mark, b, data);
                            if (k.left == 0) {
                                _switch(b, 0);
                            }
                            --k.left;
                            const uint8_t p1 = _total >= 1 ? out[_pos - 1] : 0;
                            const uint8_t p2 = _total >= 2 ? out[_pos - 2] : 0;
                            const uint8_t* lut = BrotliContextLut + 512 * _m.modes[k.type];
                            const unsigned tree = _m.lit_map[size_t(k.type) * 64 + (lut[p1] | lut[256 + p2])];
                            const unsigned lit = brotli_symbol(pool + _m.lit_trees[tree], b);
                            if (b.missing) {
                                _restore(mark, b, data, n);
                                return Step::need_input;
                            }
                            out[_pos++] = uint8_t(lit);
                            ++_total;
                            --_m.left;
                            --_insert_left;
                            continue;
                        }
                        // a run within the block, the room and the input
                        // (each literal 15 bits at most: 32 bytes left suffice)
                        const uint32_t run = uint32_t(std::min<uint64_t>({_insert_left, k.left, _cap - _pos}));
                        const uint8_t* const row = _m.lit_map.data() + size_t(k.type) * 64;
                        size_t pos = _pos;
                        uint32_t done = 0;
                        if (_m.single[k.type]) {
                            const BrotliEntry* t = pool + _m.lit_trees[row[0]];
                            for (; done < run && b.end - b.p >= 32; ++done) {
                                out[pos++] = uint8_t(brotli_symbol(t, b));
                            }
                        } else {
                            const uint8_t* lut = BrotliContextLut + 512 * _m.modes[k.type];
                            uint8_t p1 = _total >= 1 ? out[pos - 1] : 0;
                            uint8_t p2 = _total >= 2 ? out[pos - 2] : 0;
                            for (; done < run && b.end - b.p >= 32; ++done) {
                                const unsigned tree = row[lut[p1] | lut[256 + p2]];
                                const uint8_t lit = uint8_t(brotli_symbol(pool + _m.lit_trees[tree], b));
                                out[pos++] = lit;
                                p2 = p1;
                                p1 = lit;
                            }
                        }
                        _pos = pos;
                        _total += done;
                        _m.left -= done;
                        _insert_left -= done;
                        k.left -= done;
                    }
                    if (_m.left == 0) {
                        _phase = Phase::command;   // the copy left out: the meta-block ends with the literals
                        return Step::progress;
                    }
                    _phase = Phase::distance;
                    [[fallthrough]];
                }
                case Phase::distance: {
                    const bool careful = b.end - b.p < 32;
                    if (careful) {
                        _save(mark, b, data);
                    }
                    uint32_t dcode = 0;
                    uint32_t d;
                    if (!_implicit) {
                        BrotliBlocks& k = _m.blocks[2];
                        if (k.left == 0) {
                            _switch(b, 2);
                        }
                        --k.left;
                        const unsigned ctx = _copy_len > 4 ? 3 : _copy_len - 2;
                        dcode = brotli_symbol(_m.pool.data() + _m.dist_trees[_m.dist_map[size_t(k.type) * 4 + ctx]], b);
                    }
                    if (dcode < 16) {
                        const int64_t v = int64_t(_dist[(_dist_at - BrotliDistanceIndex[dcode]) & 3]) + BrotliDistanceDelta[dcode];
                        if (v <= 0) {
                            if (b.missing) {
                                _restore(mark, b, data, n);
                                return Step::need_input;
                            }
                            return _fail("brotli: a distance of 0 or less");
                        }
                        d = uint32_t(v);
                    } else if (dcode < 16 + _m.ndirect) {
                        d = dcode - 15;
                    } else {
                        const unsigned x = dcode - _m.ndirect - 16;
                        const unsigned nbits = 1 + (x >> (_m.npostfix + 1));
                        const unsigned hcode = x >> _m.npostfix;
                        const unsigned lcode = x & ((1u << _m.npostfix) - 1);
                        const uint32_t base = ((2u + (hcode & 1)) << nbits) - 4;
                        d = ((base + b.read(nbits)) << _m.npostfix) + lcode + _m.ndirect + 1;
                    }
                    if (b.missing) {
                        _restore(mark, b, data, n);
                        return Step::need_input;
                    }
                    const uint64_t max_distance = std::min<uint64_t>(_window, _total);
                    if (d > max_distance) {
                        // a word of the static dictionary, transformed
                        const unsigned len = _copy_len;
                        if (len < 4 || len > 24) {
                            return _fail("brotli: a distance before the data");
                        }
                        const uint64_t id = d - max_distance - 1;
                        const unsigned bits = BrotliDictionaryBits[len];
                        const uint64_t index = id & ((uint64_t(1) << bits) - 1);
                        const uint64_t transform = id >> bits;
                        if (transform >= 121) {
                            return _fail("brotli: a dictionary reference past the transforms");
                        }
                        const unsigned made = brotli_transform(_word, BrotliDictionaryOffsets[len] + uint32_t(index) * len, len, unsigned(transform));
                        if (made > _m.left) {
                            return _fail("brotli: a copy past the end of its meta-block");
                        }
                        _from_word = true;
                        _word_at = 0;
                        _copy_left = made;
                    } else {
                        if (dcode != 0) {
                            _dist_at = (_dist_at + 1) & 3;
                            _dist[_dist_at] = d;
                        }
                        if (_copy_len > _m.left) {
                            return _fail("brotli: a copy past the end of its meta-block");
                        }
                        _from_word = false;
                        _distance = d;
                        _copy_left = _copy_len;
                    }
                    _phase = Phase::copy;
                    [[fallthrough]];
                }
                case Phase::copy: {
                    const size_t take = std::min<size_t>(_copy_left, _cap - _pos);
                    if (_from_word) {
                        sgcl::detail::copy_bytes(out + _pos, _word + _word_at, take);
                        _word_at += unsigned(take);
                        _pos += take;
                    } else if (take) {
                        _pos = size_t(lz4_copy_match(out + _pos, _distance, take) - out);
                    }
                    _total += take;
                    _m.left -= uint32_t(take);
                    _copy_left -= uint32_t(take);
                    if (_copy_left) {
                        return Step::progress;   // the room is full
                    }
                    _phase = Phase::command;
                    break;
                }
            }
        }
    }
}
