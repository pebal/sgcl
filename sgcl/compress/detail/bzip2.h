//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sgcl::compress::detail {
    // The CRC-32 of bzip2: the polynomial 0x04C11DB7 of zlib's, but taken
    // most significant bit first (zlib's is reflected), started at all
    // ones and inverted at the end. The tables are those of the method
    // that takes eight bytes a step: table k is what a byte does to the
    // register when k zero bytes follow it.
    struct Bzip2CrcTables {
        uint32_t t[8][256];
    };

    constexpr Bzip2CrcTables make_bzip2_crc_tables() noexcept {
        Bzip2CrcTables r{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i << 24;
            for (int k = 0; k < 8; ++k) {
                c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : c << 1;
            }
            r.t[0][i] = c;
        }
        for (int k = 1; k < 8; ++k) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = r.t[k - 1][i];
                r.t[k][i] = (c << 8) ^ r.t[0][c >> 24];
            }
        }
        return r;
    }

    inline constexpr Bzip2CrcTables Bzip2Crc = make_bzip2_crc_tables();

    // The register after the bytes: crc is the register as it stands (all
    // ones at the start of a block), not the inverted value the format
    // stores
    inline uint32_t bzip2_crc_update(uint32_t crc, const uint8_t* p, size_t n) noexcept {
        auto& t = Bzip2Crc.t;
        while (n >= 8) {
            uint32_t x = crc ^ (uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]));
            uint32_t y = uint32_t(p[4]) << 24 | uint32_t(p[5]) << 16 | uint32_t(p[6]) << 8 | uint32_t(p[7]);
            crc = t[7][x >> 24] ^ t[6][(x >> 16) & 255] ^ t[5][(x >> 8) & 255] ^ t[4][x & 255]
                ^ t[3][y >> 24] ^ t[2][(y >> 16) & 255] ^ t[1][(y >> 8) & 255] ^ t[0][y & 255];
            p += 8;
            n -= 8;
        }
        while (n--) {
            crc = (crc << 8) ^ t[0][(crc >> 24) ^ *p++];
        }
        return crc;
    }

    enum class Bzip2Status : uint8_t {
        need_input,   // every byte given was taken; more is needed
        need_room,    // the output is full and the block has more
        done,         // the last stream ended with the input
        failed
    };

    // The decoder of bzip2 1.0 streams, resumable at any byte of its input
    // and at any byte of its output. A stream is "BZh" and a digit n, the
    // blocks, and an end; the bits are read most significant first and the
    // blocks are not aligned to bytes. A block holds up to n * 100000
    // symbols of the Burrows-Wheeler transform of the data, which is
    // itself the data with runs of four to 259 equal bytes shortened to
    // four and a count (RLE1). The transform's output is coded as the
    // positions of its bytes in a move-to-front list, the runs of the
    // front byte as numbers in bijective base 2 of two symbols, RUNA and
    // RUNB (RLE2), and the whole with up to six Huffman codes, a code
    // chosen for each group of 50 symbols by a selector (itself coded
    // move-to-front and in unary). Each block carries the CRC of its
    // data, the stream the CRCs of its blocks combined, and streams may
    // follow one another (pbzip2 writes one per block): the decoder goes
    // on into the next when the input has one.
    //
    // Nothing of the input may make it take memory or time without
    // bounds: the block holds no more than its header allows, a run of
    // RUNA and RUNB that would pass that fails at once, the code lengths
    // are 1..20, and the transform's chain is a permutation, so the
    // inverse walks it exactly once.
    class Bzip2Decoder {
    public:
        static constexpr unsigned MaxGroups = 6;
        static constexpr unsigned MaxAlphabet = 258;   // RUNA, RUNB, 255 positions past the front, the end of the block
        static constexpr unsigned MaxCodeBits = 20;
        static constexpr unsigned GroupSize = 50;
        static constexpr unsigned MaxSelectors = 32767;   // what 15 bits hold
        static constexpr unsigned RootBits = 10;

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        void reset() noexcept {
            _stage = Stage::stream;
            _bits = 0;
            _count = 0;
            _pulled = 0;
            _streams = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        // The byte of the input where the decoder stands (for an error,
        // where it was found)
        uint64_t offset() const noexcept {
            return _pulled - _count / 8;
        }

        // Decodes from [in, end) into out[pos, cap). final says that no
        // input follows end: running out of it is then unexpected_end, and
        // the end of a stream with nothing after it is done.
        Bzip2Status decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) {
            for (;;) {
                switch (_stage) {
                    case Stage::stream: {
                        if (_streams && _count == 0 && in == end) {
                            if (final) {
                                _stage = Stage::done;
                                return Bzip2Status::done;
                            }
                            return Bzip2Status::need_input;
                        }
                        if (!_have(32, in, end)) {
                            return _starve(final);
                        }
                        uint32_t v = _get(32);
                        if ((v >> 8) != 0x425A68) {   // "BZh"
                            return _fail(errc::invalid_header, _streams ? "bzip2: data after the end of the stream that is not another stream" : "bzip2: not a bzip2 stream");
                        }
                        uint32_t level = (v & 255) - '0';
                        if (level < 1 || level > 9) {
                            return _fail(errc::invalid_header, "bzip2: block size outside 1..9");
                        }
                        _block_max = level * 100000;
                        if (_tt_size < _block_max) {
                            // uninitialized: a block writes each word it reads
                            _tt.reset(new uint32_t[_block_max]);
                            _tt_size = _block_max;
                        }
                        _stream_crc = 0;
                        ++_streams;
                        _stage = Stage::block_magic;
                        break;
                    }
                    case Stage::block_magic: {
                        if (!_have(48, in, end)) {
                            return _starve(final);
                        }
                        uint64_t magic = uint64_t(_get(24)) << 24;
                        magic |= _get(24);
                        if (magic == 0x314159265359) {   // the digits of pi
                            _stage = Stage::block_crc;
                        } else if (magic == 0x177245385090) {   // the square root of pi
                            _stage = Stage::stream_crc;
                        } else {
                            return _fail(errc::corrupt, "bzip2: bad block magic");
                        }
                        break;
                    }
                    case Stage::block_crc: {
                        if (!_have(32 + 1 + 24, in, end)) {
                            return _starve(final);
                        }
                        _want_crc = _get(32);
                        if (_get(1)) {
                            // bzip2 0.9.5 stopped writing them; Go refuses them too
                            return _fail(errc::unsupported, "bzip2: randomised blocks (deprecated since bzip2 0.9.5) are not supported");
                        }
                        _orig_ptr = _get(24);
                        _stage = Stage::used_map;
                        break;
                    }
                    case Stage::used_map: {
                        if (!_have(16, in, end)) {
                            return _starve(final);
                        }
                        _used_map = _get(16);
                        _used_index = 0;
                        _in_use = 0;
                        _stage = Stage::used_bits;
                        break;
                    }
                    case Stage::used_bits: {
                        // the bytes the block uses: 16 ranges of 16, a map of the ranges present
                        while (_used_index < 16) {
                            if (_used_map & (0x8000u >> _used_index)) {
                                if (!_have(16, in, end)) {
                                    return _starve(final);
                                }
                                uint32_t v = _get(16);
                                for (unsigned j = 0; j < 16; ++j) {
                                    if (v & (0x8000u >> j)) {
                                        _mtf[_in_use++] = uint8_t(_used_index * 16 + j);
                                    }
                                }
                            }
                            ++_used_index;
                        }
                        if (_in_use == 0) {
                            return _fail(errc::corrupt, "bzip2: a block that uses no byte");
                        }
                        _alphabet = _in_use + 2;
                        _stage = Stage::groups;
                        break;
                    }
                    case Stage::groups: {
                        if (!_have(3 + 15, in, end)) {
                            return _starve(final);
                        }
                        _groups = _get(3);
                        _selector_count = _get(15);
                        if (_groups < 2 || _groups > MaxGroups) {
                            return _fail(errc::corrupt, "bzip2: number of Huffman tables outside 2..6");
                        }
                        if (_selector_count == 0) {
                            return _fail(errc::corrupt, "bzip2: no selectors");
                        }
                        _index = 0;
                        _unary = 0;
                        _stage = Stage::selectors;
                        break;
                    }
                    case Stage::selectors: {
                        // each a table's position in a move-to-front list, in unary
                        while (_index < _selector_count) {
                            if (!_have(1, in, end)) {
                                return _starve(final);
                            }
                            if (_get(1)) {
                                if (++_unary >= _groups) {
                                    return _fail(errc::corrupt, "bzip2: selector past the tables");
                                }
                            } else {
                                _selectors[_index++] = uint8_t(_unary);
                                _unary = 0;
                            }
                        }
                        uint8_t order[MaxGroups];
                        for (unsigned g = 0; g < MaxGroups; ++g) {
                            order[g] = uint8_t(g);
                        }
                        for (unsigned i = 0; i < _selector_count; ++i) {
                            unsigned k = _selectors[i];
                            uint8_t v = order[k];
                            for (; k > 0; --k) {
                                order[k] = order[k - 1];
                            }
                            order[0] = v;
                            _selectors[i] = v;
                        }
                        _table = 0;
                        _stage = Stage::table_start;
                        break;
                    }
                    case Stage::table_start: {
                        if (!_have(5, in, end)) {
                            return _starve(final);
                        }
                        _length = _get(5);
                        _index = 0;
                        _step = false;
                        _stage = Stage::table_lengths;
                        break;
                    }
                    case Stage::table_lengths: {
                        // each symbol's length is the previous one's, changed by
                        // pairs of bits: 1 then 0 adds one, 1 then 1 takes one
                        // away, 0 ends the symbol; it stays within 1..20
                        while (_index < _alphabet) {
                            if (!_step) {
                                if (_length < 1 || _length > int32_t(MaxCodeBits)) {
                                    return _fail(errc::corrupt, "bzip2: Huffman code length outside 1..20");
                                }
                                if (!_have(1, in, end)) {
                                    return _starve(final);
                                }
                                if (_get(1)) {
                                    _step = true;
                                } else {
                                    _lengths[_index++] = uint8_t(_length);
                                }
                            } else {
                                if (!_have(1, in, end)) {
                                    return _starve(final);
                                }
                                _length += _get(1) ? -1 : 1;
                                _step = false;
                            }
                        }
                        _build(_tables[_table]);
                        if (++_table < _groups) {
                            _stage = Stage::table_start;
                            break;
                        }
                        std::memset(_counts, 0, sizeof(_counts));
                        _size = 0;
                        _group_left = 0;
                        _group = 0;
                        _run = 0;
                        _weight = 1;
                        _stage = Stage::symbols;
                        break;
                    }
                    case Stage::symbols: {
                        auto st = _symbols(in, end, final);
                        if (st) {
                            return *st;
                        }
                        _inverse();
                        _stage = Stage::output;
                        break;
                    }
                    case Stage::output: {
                        if (!_output(out, pos, cap)) {
                            return Bzip2Status::need_room;
                        }
                        uint32_t crc = ~_crc;
                        if (crc != _want_crc) {
                            return _fail(errc::checksum, "bzip2: block CRC mismatch");
                        }
                        _stream_crc = ((_stream_crc << 1) | (_stream_crc >> 31)) ^ crc;
                        _stage = Stage::block_magic;
                        break;
                    }
                    case Stage::stream_crc: {
                        if (!_have(32, in, end)) {
                            return _starve(final);
                        }
                        if (_get(32) != _stream_crc) {
                            return _fail(errc::checksum, "bzip2: stream CRC mismatch");
                        }
                        // the stream ends at a byte; another may follow
                        _count -= _count % 8;
                        _stage = Stage::stream;
                        break;
                    }
                    case Stage::done:
                        return Bzip2Status::done;
                    case Stage::failed:
                        return Bzip2Status::failed;
                }
            }
        }

    private:
        enum class Stage : uint8_t {
            stream,
            block_magic,
            block_crc,
            used_map,
            used_bits,
            groups,
            selectors,
            table_start,
            table_lengths,
            symbols,
            output,
            stream_crc,
            done,
            failed
        };

        // A Huffman code, canonical: the codes of a length are consecutive
        // and in the order of their symbols, the shorter codes first. A code
        // of RootBits bits or fewer is one lookup of the next RootBits bits
        // (an entry is the symbol << 5 | the length; 0 is a longer code);
        // a longer one is found by its length, trying each from RootBits + 1.
        struct Table {
            uint16_t root[1u << RootBits];
            uint32_t first[MaxCodeBits + 1];   // the first code of each length
            uint16_t count[MaxCodeBits + 1];
            uint16_t index[MaxCodeBits + 1];   // where the length's symbols start in symbols
            uint16_t symbols[MaxAlphabet];
            uint32_t max_length;
        };

        // The table of _lengths[0, _alphabet). Lengths of no prefix code
        // are taken as libbz2 takes them: an incomplete code fails when a
        // code it lacks is met, and in one oversubscribed the codes that
        // pass their length's bits (a canonical value of 2^l or more) are
        // never met, the others decoded as numbered.
        void _build(Table& t) noexcept {
            std::memset(t.count, 0, sizeof(t.count));
            t.max_length = 0;
            for (unsigned i = 0; i < _alphabet; ++i) {
                ++t.count[_lengths[i]];
                t.max_length = std::max<uint32_t>(t.max_length, _lengths[i]);
            }
            uint32_t code = 0;
            uint16_t index = 0;
            uint16_t next[MaxCodeBits + 1];
            for (unsigned l = 1; l <= MaxCodeBits; ++l) {
                t.first[l] = code;
                t.index[l] = index;
                next[l] = index;
                index = uint16_t(index + t.count[l]);
                code = (code + t.count[l]) << 1;
            }
            for (unsigned i = 0; i < _alphabet; ++i) {
                t.symbols[next[_lengths[i]]++] = uint16_t(i);
            }
            std::memset(t.root, 0, sizeof(t.root));
            for (unsigned l = 1; l <= RootBits; ++l) {
                for (unsigned k = 0; k < t.count[l] && t.first[l] + k < (1u << l); ++k) {
                    uint32_t c = t.first[l] + k;
                    uint16_t entry = uint16_t(t.symbols[t.index[l] + k] << 5 | l);
                    uint32_t from = c << (RootBits - l);
                    uint32_t to = (c + 1) << (RootBits - l);
                    for (uint32_t j = from; j < to; ++j) {
                        t.root[j] = entry;
                    }
                }
            }
        }

        // The symbols of the block, to its end symbol; nullopt when the
        // block is whole, the status to return when not
        optional<Bzip2Status> _symbols(const uint8_t*& in, const uint8_t* end, bool final) {
            // the hot loop keeps the state in locals, written back on the way out
            uint64_t bits = _bits;
            uint32_t count = _count;
            const uint8_t* p = in;
            uint32_t size = _size;
            uint32_t run = _run;
            uint32_t weight = _weight;
            uint32_t group_left = _group_left;
            const uint32_t block_max = _block_max;
            const uint32_t eob = _alphabet - 1;
            uint32_t* tt = _tt.get();
            const Table* table = _group_left ? &_tables[_selectors[_group - 1]] : nullptr;
            auto save = [&] {
                _pulled += uint64_t(p - in);
                in = p;
                _bits = bits;
                _count = count;
                _size = size;
                _run = run;
                _weight = weight;
                _group_left = group_left;
            };
            for (;;) {
                if (count < MaxCodeBits) {
                    if (end - p >= 8) {
                        uint64_t w = 0;
                        for (int k = 0; k < 8; ++k) {
                            w = w << 8 | p[k];
                        }
                        uint32_t take = (63 - count) >> 3;
                        bits = bits << (8 * take) | w >> (64 - 8 * take);
                        count += 8 * take;
                        p += take;
                    } else {
                        while (count <= 56 && p < end) {
                            bits = bits << 8 | *p++;
                            count += 8;
                        }
                        if (count < MaxCodeBits) {
                            // a valid stream has its end (80 bits) after the last symbol
                            save();
                            if (final) {
                                return _fail(errc::unexpected_end, "bzip2: unexpected end of the compressed data");
                            }
                            return Bzip2Status::need_input;
                        }
                    }
                }
                if (group_left == 0) {
                    if (_group >= _selector_count) {
                        save();
                        return _fail(errc::corrupt, "bzip2: more groups of symbols than selectors");
                    }
                    table = &_tables[_selectors[_group++]];
                    group_left = GroupSize;
                }
                uint32_t peek = uint32_t(bits >> (count - MaxCodeBits)) & ((1u << MaxCodeBits) - 1);
                uint32_t entry = table->root[peek >> (MaxCodeBits - RootBits)];
                uint32_t symbol, length;
                if (entry) {
                    symbol = entry >> 5;
                    length = entry & 31;
                } else {
                    length = RootBits + 1;
                    for (;; ++length) {
                        if (length > table->max_length) {
                            save();
                            return _fail(errc::corrupt, "bzip2: invalid Huffman code");
                        }
                        uint32_t c = peek >> (MaxCodeBits - length);
                        uint32_t k = c - table->first[length];
                        if (k < table->count[length]) {
                            symbol = table->symbols[table->index[length] + k];
                            break;
                        }
                    }
                }
                count -= length;
                --group_left;
                if (symbol <= 1) {
                    // RUNA adds the weight, RUNB twice the weight; the weight doubles
                    run += weight << symbol;
                    weight <<= 1;
                    if (run > block_max) {
                        save();
                        return _fail(errc::corrupt, "bzip2: a run past the block size");
                    }
                    continue;
                }
                if (run) {
                    if (run > block_max - size) {
                        save();
                        return _fail(errc::corrupt, "bzip2: a run past the block size");
                    }
                    uint8_t b = _mtf[0];
                    _counts[b] += run;
                    for (uint32_t i = 0; i < run; ++i) {
                        tt[size + i] = b;
                    }
                    size += run;
                    run = 0;
                    weight = 1;
                }
                if (symbol == eob) {
                    save();
                    if (_orig_ptr >= size) {
                        return _fail(errc::corrupt, "bzip2: original pointer outside the block");
                    }
                    return nullopt;
                }
                if (size >= block_max) {
                    save();
                    return _fail(errc::corrupt, "bzip2: block larger than its header allows");
                }
                // the byte at the position, moved to the front
                uint32_t k = symbol - 1;
                uint8_t b = _mtf[k];
                // the two runs overlap: move_bytes reads every block
                // before it writes one (memmove past 32 bytes)
                sgcl::detail::move_bytes(_mtf + 1, _mtf, k);
                _mtf[0] = b;
                ++_counts[b];
                tt[size++] = b;
            }
        }

        // The inverse transform: tt[j] gets, above its byte, the row whose
        // rotation starts one byte later than row j's. Row j's first byte
        // is the j-th of the sorted bytes; counting the rows before each
        // byte and taking the last column in order, the i-th occurrence of
        // a byte in the last column is the i-th in the first, and the row
        // it ends precedes (by one byte of the text) the row it starts.
        void _inverse() noexcept {
            uint32_t* tt = _tt.get();
            uint32_t start[256];
            uint32_t sum = 0;
            for (unsigned b = 0; b < 256; ++b) {
                start[b] = sum;
                sum += _counts[b];
            }
            for (uint32_t i = 0; i < _size; ++i) {
                uint8_t b = uint8_t(tt[i]);
                tt[start[b]++] |= i << 8;
            }
            _position = tt[_orig_ptr] >> 8;
            _left = _size;
            _last = -1;
            _same = 0;
            _repeat = 0;
            _crc = 0xFFFFFFFFu;
        }

        // The block's bytes into out, the runs of RLE1 expanded (four equal
        // bytes, then how many more); true when the block is out whole
        bool _output(uint8_t* out, size_t& pos, size_t cap) noexcept {
            const uint32_t* tt = _tt.get();
            size_t at = pos;
            uint32_t position = _position;
            uint32_t left = _left;
            int last = _last;
            uint32_t same = _same;
            uint32_t repeat = _repeat;
            while (at < cap) {
                if (repeat) {
                    size_t n = std::min<size_t>(repeat, cap - at);
                    std::memset(out + at, last, n);
                    at += n;
                    repeat -= uint32_t(n);
                    continue;
                }
                if (!left) {
                    break;
                }
                uint32_t t = tt[position];
                position = t >> 8;
                --left;
                int b = int(t & 255);
                if (same == 4) {
                    repeat = uint32_t(b);
                    same = 0;
                    continue;
                }
                same = b == last ? same + 1 : 1;
                last = b;
                out[at++] = uint8_t(b);
            }
            _crc = bzip2_crc_update(_crc, out + pos, at - pos);
            pos = at;
            _position = position;
            _left = left;
            _last = last;
            _same = same;
            _repeat = repeat;
            return !left && !repeat;
        }

        // Whether k bits are there, the input taken into the bits while
        // they fit
        bool _have(uint32_t k, const uint8_t*& in, const uint8_t* end) noexcept {
            while (_count <= 56 && in < end) {
                _bits = _bits << 8 | *in++;
                _count += 8;
                ++_pulled;
            }
            return _count >= k;
        }

        uint32_t _get(uint32_t k) noexcept {
            _count -= k;
            return uint32_t(_bits >> _count) & uint32_t((uint64_t(1) << k) - 1);
        }

        Bzip2Status _starve(bool final) noexcept {
            if (final) {
                return _fail(errc::unexpected_end, "bzip2: unexpected end of the compressed data");
            }
            return Bzip2Status::need_input;
        }

        Bzip2Status _fail(errc code, const char* text) noexcept {
            error = code;
            error_text = text;
            _stage = Stage::failed;
            return Bzip2Status::failed;
        }

        Stage _stage = Stage::stream;
        uint64_t _bits = 0;          // the input bits not yet used, the next the highest of the lowest _count
        uint32_t _count = 0;
        uint64_t _pulled = 0;        // the input bytes taken into the bits
        uint32_t _streams = 0;       // the streams begun
        uint32_t _block_max = 0;     // the symbols a block of this stream may hold
        uint32_t _stream_crc = 0;
        uint32_t _want_crc = 0;
        uint32_t _orig_ptr = 0;      // the row of the text among the sorted rotations
        uint32_t _used_map = 0;
        uint32_t _used_index = 0;
        uint32_t _in_use = 0;
        uint32_t _alphabet = 0;
        uint32_t _groups = 0;
        uint32_t _selector_count = 0;
        uint32_t _index = 0;
        uint32_t _unary = 0;
        uint32_t _table = 0;
        int32_t _length = 0;
        bool _step = false;
        // the symbols
        uint32_t _size = 0;          // the transform's bytes so far
        uint32_t _group = 0;         // the next selector
        uint32_t _group_left = 0;    // the symbols left of the current group
        uint32_t _run = 0;           // RLE2's run being read, and its next digit's weight
        uint32_t _weight = 1;
        // the output
        uint32_t _position = 0;
        uint32_t _left = 0;
        int _last = -1;
        uint32_t _same = 0;
        uint32_t _repeat = 0;
        uint32_t _crc = 0;
        uint8_t _mtf[256];
        uint32_t _counts[256];
        uint8_t _lengths[MaxAlphabet];
        uint8_t _selectors[MaxSelectors];
        Table _tables[MaxGroups];
        std::unique_ptr<uint32_t[]> _tt;   // the block: its bytes, then the inverse's links above them
        uint32_t _tt_size = 0;
    };
}
