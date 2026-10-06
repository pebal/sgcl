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
#include <vector>

// The entropy coders of Zstandard (RFC 8878, section 4): the bitstreams,
// Finite State Entropy (tANS) and the Huffman coding of literals, both ways.
//
// A bitstream is written forward, each value's bits from its least
// significant one up, and closed by a bit of 1 above the last; it is read
// backward, from that bit down, so a value written last is read first and
// its most significant bit comes first. The reader keeps 64 bits in a word,
// refilled from the stream a byte boundary at a time; reading past the
// start gives zeros and is noticed (overflowed), which the Huffman weights'
// two-state decoder needs to know where to stop.
//
// FSE: a table of 2^AL states for normalized counts summing to 2^AL (a
// count of -1, "less than one", takes one state at the table's end, where
// it reads the whole AL bits). The symbols are spread over the table with
// the step 5/8 of its size plus 3; the states of a symbol, in the order
// they come, take the values count..2*count-1, and a state reads as many
// bits as bring that value back to the table's size, its new state the
// value shifted by them, less the size. The encoder walks the same table
// the other way: from a state it emits the low bits that a decoder will
// read, and moves to the state of the symbol that leads back.
//
// Huffman: weights per symbol (0 absent, w a code of maxBits + 1 - w
// bits), the last weight implied by the others summing to a power of two,
// the codes canonical: the table of 2^maxBits entries filled by weight
// from the lowest up and by symbol within a weight, a symbol of weight w
// taking 2^(w-1) entries.
namespace sgcl::compress::detail {
    // The backward reader of a bitstream [begin, end): the last byte holds
    // the closing 1 bit
    class ZstdBitReader {
    public:
        // false: an empty stream, or a last byte of 0 (no closing bit)
        SGCL_INLINE_HOT bool open(const uint8_t* begin, const uint8_t* end) noexcept {
            _begin = begin;
            if (end <= begin || end[-1] == 0) {
                return false;
            }
            const size_t n = size_t(end - begin);
            _consumed = std::countl_zero(uint8_t(end[-1])) + 1;
            if (n >= 8) {
                _p = end - 8;
                _word = _load(_p);
                _end_bits = 64;
            } else {
                // a stream shorter than a word: its bytes at the top, and
                // its end where its bytes end
                _p = begin;
                _word = 0;
                for (size_t i = 0; i < n; ++i) {
                    _word |= uint64_t(begin[i]) << (8 * i);
                }
                _word <<= 8 * (8 - n);
                _end_bits = int(8 * n);
            }
            return true;
        }

        // The next n bits (0 <= n <= 56 after a refill), the first read the
        // most significant. Past the stream's start what comes is not data
        // (overflowed() says so); two shifts keep n = 0 and a count past 64
        // defined
        SGCL_INLINE_HOT uint64_t peek(int n) const noexcept {
            return ((_word << (_consumed & 63)) >> 1) >> (63 - n);
        }

        // peek() for 1 <= n and fewer than 64 bits read from the word
        SGCL_INLINE_HOT uint64_t peek_fast(int n) const noexcept {
            return (_word << _consumed) >> (64 - n);
        }

        // At least 8 bytes of the stream before the word: the next refill
        // may take the fast way
        SGCL_INLINE_HOT bool roomy8() const noexcept {
            return _p - _begin >= 8;
        }

        SGCL_INLINE_HOT void skip(int n) noexcept {
            _consumed += n;
        }

        SGCL_INLINE_HOT uint64_t read(int n) noexcept {
            const uint64_t v = peek(n);
            _consumed += n;
            return v;
        }

        // Up to 56 bits ready again; past the start the word stays and the
        // count goes on growing (overflowed() then says so)
        SGCL_INLINE_HOT void refill() noexcept {
            if (_consumed > _end_bits || _end_bits != 64) {
                return;   // overflowed already, or a stream shorter than a word: all of it is in the word
            }
            if (_p - _begin >= ptrdiff_t(_consumed >> 3)) {
                _p -= _consumed >> 3;
                _consumed &= 7;
                _word = _load(_p);
            } else if (_p > _begin) {
                const int back = int(_p - _begin);
                _p = _begin;
                _consumed -= 8 * back;
                _word = _load(_p);
            }
        }

        // At least 16 bytes of the stream before the word: the next two
        // refills may take the fast way
        SGCL_INLINE_HOT bool roomy() const noexcept {
            return _p - _begin >= 16;
        }

        // A refill when roomy(): the word moved back by the whole bytes read
        SGCL_INLINE_HOT void refill_fast() noexcept {
            _p -= _consumed >> 3;
            _consumed &= 7;
            _word = _load(_p);
        }

        // Every bit of the stream read, and none past it
        SGCL_INLINE_HOT bool finished() const noexcept {
            return _p == _begin && _consumed == _end_bits;
        }

        // More read than the stream holds
        SGCL_INLINE_HOT bool overflowed() const noexcept {
            return _p == _begin && _consumed > _end_bits;
        }

        // The bits still to read
        SGCL_INLINE_HOT int64_t left() const noexcept {
            return int64_t(_p - _begin) * 8 + _end_bits - _consumed;
        }

    private:
        SGCL_INLINE_HOT uint64_t _load(const uint8_t* p) const noexcept {
            uint64_t v;
            std::memcpy(&v, p, 8);
            if constexpr (std::endian::native == std::endian::big) {
                v = __builtin_bswap64(v);
            }
            return v;
        }

        const uint8_t* _begin = nullptr;
        const uint8_t* _p = nullptr;
        uint64_t _word = 0;
        int _consumed = 64;
        int _end_bits = 64;   // the bits of the word that are the stream's, from the top
    };

    // The forward writer of a bitstream into a buffer the caller sized
    class ZstdBitWriter {
    public:
        SGCL_INLINE_HOT explicit ZstdBitWriter(uint8_t* out) noexcept
        : _start(out)
        , _p(out) {
        }

        // n <= 56 bits of v (the bits above n must be zero)
        SGCL_INLINE_HOT void add(uint64_t v, int n) noexcept {
            _acc |= v << _bits;
            _bits += n;
        }

        // The whole bytes of the accumulator out (call when 56 bits may be
        // past: after adding up to 56 bits in total)
        SGCL_INLINE_HOT void flush() noexcept {
            uint64_t w = _acc;
            if constexpr (std::endian::native == std::endian::big) {
                w = __builtin_bswap64(w);
            }
            std::memcpy(_p, &w, 8);   // the room has 8 bytes of slack
            const int bytes = _bits >> 3;
            _p += bytes;
            _acc = bytes == 8 ? 0 : _acc >> (8 * bytes);
            _bits &= 7;
        }

        // The closing bit and the last byte; returns the stream's size
        size_t close() noexcept {
            add(1, 1);
            flush();
            if (_bits) {
                ++_p;
            }
            return size_t(_p - _start);
        }

    private:
        uint8_t* _start;
        uint8_t* _p;
        uint64_t _acc = 0;
        int _bits = 0;
    };

    // A normalized distribution: counts of up to 256 symbols, -1 for "less
    // than one", summing to 2^log (with each -1 as one)
    struct FseCounts {
        int16_t count[256];
        unsigned symbols = 0;   // the last symbol + 1
        unsigned log = 0;
    };

    // An entry of a decoding table: the symbol, the bits the state reads,
    // and the state's base
    struct FseEntry {
        uint16_t next;      // the new state's base
        uint8_t bits;       // the bits read to it
        uint8_t symbol;
    };

    // Reads an FSE table description from p (at most n bytes): its counts,
    // up to max_symbols symbols and an accuracy of at most max_log; the
    // bytes read, 0 for a description that is not valid
    inline size_t fse_read_counts(const uint8_t* p, size_t n, FseCounts& out, unsigned max_symbols, unsigned max_log) noexcept {
        if (n == 0) {
            return 0;
        }
        // a bit reader forward, little-endian, over the bytes with zeros
        // past them: a word from the byte of the first bit (count <= 16)
        auto bits = [&](size_t at, int count) noexcept -> unsigned {
            const size_t byte_at = at >> 3;
            uint64_t word = 0;
            if (byte_at + 8 <= n) {
                std::memcpy(&word, p + byte_at, 8);
                if constexpr (std::endian::native == std::endian::big) {
                    word = __builtin_bswap64(word);
                }
            } else {
                for (size_t i = byte_at; i < n; ++i) {
                    word |= uint64_t(p[i]) << (8 * (i - byte_at));
                }
            }
            return unsigned(word >> (at & 7)) & ((1u << count) - 1);
        };
        size_t at = 0;
        const unsigned log = bits(0, 4) + 5;
        at = 4;
        if (log > max_log) {
            return 0;
        }
        out.log = log;
        int remaining = (1 << log) + 1;
        int threshold = 1 << log;
        int nbits = int(log) + 1;
        unsigned symbol = 0;
        bool previous_zero = false;
        while (remaining > 1 && symbol < max_symbols) {
            if (previous_zero) {
                // zeros repeated: 2-bit counts, a 3 meaning three more and another count
                unsigned repeat;
                do {
                    repeat = bits(at, 2);
                    at += 2;
                    for (unsigned i = 0; i < repeat; ++i) {
                        if (symbol >= max_symbols) {
                            return 0;
                        }
                        out.count[symbol++] = 0;
                    }
                    if (at > n * 8) {
                        return 0;
                    }
                } while (repeat == 3);
                previous_zero = false;
                if (symbol >= max_symbols) {
                    break;
                }
            }
            const int max = 2 * threshold - 1 - remaining;
            const unsigned low = bits(at, nbits - 1);
            int value;
            if (int(low) < max) {
                value = int(low);
                at += size_t(nbits - 1);
            } else {
                value = int(bits(at, nbits));
                at += size_t(nbits);
                if (value >= threshold) {
                    value -= max;
                }
            }
            const int count = value - 1;
            remaining -= count < 0 ? -count : count;
            out.count[symbol++] = int16_t(count);
            previous_zero = count == 0;
            while (remaining < threshold) {
                --nbits;
                threshold >>= 1;
            }
            if (at > n * 8) {
                return 0;
            }
        }
        if (remaining != 1) {
            return 0;
        }
        out.symbols = symbol;
        for (unsigned s = symbol; s < 256; ++s) {
            out.count[s] = 0;
        }
        return (at + 7) >> 3;
    }

    // The decoding table of a distribution: 2^log entries
    inline bool fse_build_decoder(const FseCounts& c, FseEntry* table) noexcept {
        const unsigned size = 1u << c.log;
        const unsigned mask = size - 1;
        unsigned high = size - 1;
        uint16_t next[256];
        for (unsigned s = 0; s < c.symbols; ++s) {
            if (c.count[s] == -1) {
                table[high--].symbol = uint8_t(s);
                next[s] = 1;
            } else {
                next[s] = uint16_t(c.count[s]);
            }
        }
        const unsigned step = (size >> 1) + (size >> 3) + 3;
        unsigned position = 0;
        for (unsigned s = 0; s < c.symbols; ++s) {
            for (int i = 0; i < c.count[s]; ++i) {
                table[position].symbol = uint8_t(s);
                do {
                    position = (position + step) & mask;
                } while (position > high);
            }
        }
        if (position != 0) {
            return false;   // the counts do not fill the table
        }
        for (unsigned u = 0; u < size; ++u) {
            const unsigned s = table[u].symbol;
            const unsigned x = next[s]++;
            const int bits = int(c.log) - (31 - std::countl_zero(uint32_t(x)));
            table[u].bits = uint8_t(bits);
            table[u].next = uint16_t((x << bits) - size);
        }
        return true;
    }

    // A table of one symbol (an RLE mode): every state the symbol, no bits
    inline void fse_build_rle(FseEntry* table, uint8_t symbol) noexcept {
        table[0] = FseEntry {0, 0, symbol};
    }

    // The decoding table of the Huffman codes of literals: 2^max_bits
    // entries, each the symbol and the bits of its code
    struct HufEntry {
        uint8_t symbol;
        uint8_t bits;
    };

    struct HufTable {
        HufEntry entry[1 << 11];
        unsigned max_bits = 0;
        bool valid = false;
    };

    // The table from the weights of the symbols before the last (the last
    // implied); false when the weights cannot complete a power of two
    inline bool huf_build_decoder(const uint8_t* weights, unsigned n, HufTable& t) noexcept {
        if (n == 0 || n > 255) {
            return false;
        }
        uint32_t total = 0;
        for (unsigned i = 0; i < n; ++i) {
            if (weights[i] > 11) {
                return false;
            }
            if (weights[i]) {
                total += uint32_t(1) << (weights[i] - 1);
            }
        }
        if (total == 0) {
            return false;
        }
        const unsigned max_bits = 32 - std::countl_zero(total);   // the power of two above total
        if (max_bits > 11) {
            return false;
        }
        const uint32_t rest = (uint32_t(1) << max_bits) - total;
        if (rest & (rest - 1)) {
            return false;   // not a power of two
        }
        uint8_t w[256];
        std::memcpy(w, weights, n);
        w[n] = uint8_t(std::countr_zero(rest) + 1);
        const unsigned symbols = n + 1;
        // the entries, by weight from 1 up and by symbol within a weight:
        // where each weight's entries begin, from how many symbols have it,
        // then each symbol's 2^(w-1) entries at its weight's next place
        // (four at a time as one word from four on)
        unsigned start[13] = {};
        for (unsigned s = 0; s < symbols; ++s) {
            ++start[w[s]];
        }
        unsigned at = 0;
        for (unsigned weight = 1; weight <= max_bits; ++weight) {
            const unsigned k = start[weight];
            start[weight] = at;
            at += k << (weight - 1);
        }
        if (at != (1u << max_bits)) {
            return false;
        }
        HufEntry* const e = t.entry;
        for (unsigned s = 0; s < symbols; ++s) {
            const unsigned weight = w[s];
            if (weight == 0) {
                continue;
            }
            const HufEntry en {uint8_t(s), uint8_t(max_bits + 1 - weight)};
            HufEntry* const o = e + start[weight];
            const unsigned span = 1u << (weight - 1);
            start[weight] += span;
            if (span < 4) {
                o[0] = en;
                o[span - 1] = en;
            } else {
                uint16_t half;
                std::memcpy(&half, &en, 2);
                const uint64_t word = 0x0001000100010001ull * half;
                for (unsigned k = 0; k < span; k += 4) {
                    std::memcpy(o + k, &word, 8);
                }
            }
        }
        t.max_bits = max_bits;
        t.valid = true;
        return true;
    }

    // Reads a Huffman tree description at p (n bytes): the table; the bytes
    // read, 0 when it is not valid
    inline size_t huf_read_table(const uint8_t* p, size_t n, HufTable& t) noexcept {
        if (n == 0) {
            return 0;
        }
        uint8_t weights[256];
        unsigned count = 0;
        const unsigned head = p[0];
        size_t used;
        if (head >= 128) {
            // direct: 4 bits a weight, the high nibble first
            count = head - 127;
            used = 1 + (count + 1) / 2;
            if (used > n) {
                return 0;
            }
            for (unsigned i = 0; i < count; ++i) {
                const uint8_t b = p[1 + i / 2];
                weights[i] = i & 1 ? b & 15 : b >> 4;
            }
        } else {
            // FSE: a description of accuracy 6 at most, then two states taking turns
            used = 1 + head;
            if (used > n || head == 0) {
                return 0;
            }
            FseCounts c;
            const size_t h = fse_read_counts(p + 1, head, c, 255, 6);
            if (!h || h > head) {
                return 0;
            }
            FseEntry table[64];
            if (!fse_build_decoder(c, table)) {
                return 0;
            }
            ZstdBitReader r;
            if (!r.open(p + 1 + h, p + 1 + head)) {
                return 0;
            }
            unsigned s1 = unsigned(r.read(int(c.log)));
            unsigned s2 = unsigned(r.read(int(c.log)));
            for (;;) {
                if (count >= 255) {
                    return 0;
                }
                r.refill();
                weights[count++] = table[s1].symbol;
                s1 = table[s1].next + unsigned(r.read(table[s1].bits));
                if (r.overflowed()) {
                    if (count >= 255) {
                        return 0;
                    }
                    weights[count++] = table[s2].symbol;
                    break;
                }
                if (count >= 255) {
                    return 0;
                }
                weights[count++] = table[s2].symbol;
                s2 = table[s2].next + unsigned(r.read(table[s2].bits));
                if (r.overflowed()) {
                    if (count >= 255) {
                        return 0;
                    }
                    weights[count++] = table[s1].symbol;
                    break;
                }
            }
        }
        if (!huf_build_decoder(weights, count, t)) {
            return 0;
        }
        return used;
    }

    // ---- the encoders ----------------------------------------------------

    // The accuracy for a distribution of `total` events over symbols up to
    // max_symbol: enough states for every symbol, no more than the events
    // justify, within [5, max_log]
    inline unsigned fse_choose_log(size_t total, unsigned max_symbol, unsigned max_log) noexcept {
        const unsigned from_events = total > 1 ? unsigned(63 - std::countl_zero(uint64_t(total - 1))) - 1 : 1;
        const unsigned for_symbols = unsigned(31 - std::countl_zero(uint32_t(max_symbol | 1))) + 2;
        const unsigned least = std::min(total > 1 ? unsigned(63 - std::countl_zero(uint64_t(total - 1))) + 1 : 1u, for_symbols);
        unsigned log = std::min(max_log, from_events);
        log = std::max(log, least);
        return std::clamp(log, 5u, max_log);
    }

    // Counts scaled to 2^log, every symbol that occurs keeping one state at
    // least: each takes its share rounded down, and the states left go to the
    // largest remainders (or, when the floors of one each took too many, are
    // taken back from the largest shares)
    inline void fse_normalize(const uint32_t* counts, unsigned symbols, uint64_t total, unsigned log, FseCounts& out) noexcept {
        const int64_t size = int64_t(1) << log;
        int64_t sum = 0;
        uint64_t remainder[256];
        for (unsigned s = 0; s < 256; ++s) {
            out.count[s] = 0;
        }
        for (unsigned s = 0; s < symbols; ++s) {
            if (!counts[s]) {
                remainder[s] = 0;
                continue;
            }
            const uint64_t scaled = uint64_t(counts[s]) << log;
            int64_t share = int64_t(scaled / total);
            remainder[s] = scaled % total;
            if (share < 1) {
                share = 1;
                remainder[s] = 0;
            }
            out.count[s] = int16_t(share);
            sum += share;
        }
        while (sum < size) {   // the largest remainders take one more each
            unsigned best = 0;
            uint64_t r = 0;
            bool found = false;
            for (unsigned s = 0; s < symbols; ++s) {
                if (out.count[s] > 0 && (!found || remainder[s] > r)) {
                    best = s;
                    r = remainder[s];
                    found = true;
                }
            }
            ++out.count[best];
            remainder[best] = 0;
            ++sum;
        }
        while (sum > size) {   // the largest shares give one back
            unsigned best = 0;
            for (unsigned s = 0; s < symbols; ++s) {
                if (out.count[s] > out.count[best]) {
                    best = s;
                }
            }
            --out.count[best];
            --sum;
        }
        unsigned last = 0;
        for (unsigned s = 0; s < symbols; ++s) {
            if (out.count[s]) {
                last = s + 1;
            }
        }
        out.symbols = last;
        out.log = log;
    }

    // An FSE table description of the counts written to out; its bytes
    inline size_t fse_write_counts(const FseCounts& c, uint8_t* out) noexcept {
        uint64_t acc = c.log - 5;
        int bits = 4;
        size_t n = 0;
        auto put = [&](uint64_t v, int count) noexcept {
            acc |= v << bits;
            bits += count;
            while (bits >= 8) {
                out[n++] = uint8_t(acc);
                acc >>= 8;
                bits -= 8;
            }
        };
        int remaining = (1 << c.log) + 1;
        int threshold = 1 << c.log;
        int nbits = int(c.log) + 1;
        unsigned s = 0;
        while (remaining > 1) {
            const int count = c.count[s];
            const int value = count + 1;
            const int max = 2 * threshold - 1 - remaining;
            if (value < max) {
                put(uint64_t(value), nbits - 1);
            } else if (value < threshold) {
                put(uint64_t(value), nbits);
            } else {
                put(uint64_t(value + max), nbits);
            }
            remaining -= count < 0 ? -count : count;
            ++s;
            if (count == 0) {
                // the zeros after it, in 2-bit counts, 3 meaning more follow
                unsigned zeros = 0;
                while (s + zeros < c.symbols && c.count[s + zeros] == 0) {
                    ++zeros;
                }
                s += zeros;
                while (zeros >= 3) {
                    put(3, 2);
                    zeros -= 3;
                }
                put(zeros, 2);
            }
            while (remaining < threshold) {
                --nbits;
                threshold >>= 1;
            }
        }
        if (bits > 0) {
            out[n++] = uint8_t(acc);
        }
        return n;
    }

    // The encoding side of a table: for each symbol its states in the order
    // the decoding table holds them, and what tells how many bits a state
    // gives up
    struct FseEncoder {
        uint16_t states[512];     // the next state, 2^log added
        uint16_t first[256];      // a symbol's first entry in states
        uint16_t count[256];      // its states (a "less than one" counts one)
        // the bits a symbol gives up from state x: (x + delta_bits) >> 16,
        // one less below its threshold; the next state at
        // states[(x >> bits) + delta_state]
        uint32_t delta_bits[256];
        int32_t delta_state[256];
        unsigned log = 0;
        unsigned size = 0;

        // From a decoding table of 2^log entries
        void build(const FseEntry* table, unsigned table_log) noexcept {
            log = table_log;
            size = 1u << table_log;
            std::fill(std::begin(count), std::end(count), uint16_t(0));
            for (unsigned u = 0; u < size; ++u) {
                ++count[table[u].symbol];
            }
            unsigned at = 0;
            for (unsigned sym = 0; sym < 256; ++sym) {
                first[sym] = uint16_t(at);
                const unsigned c = count[sym];
                at += c;
                if (c == 0) {
                    delta_bits[sym] = 0;
                    delta_state[sym] = 0;
                    continue;
                }
                const unsigned most = c == 1 ? log + 1 : log - (31 - unsigned(std::countl_zero(uint32_t(c - 1))));
                const uint32_t threshold = c == 1 ? 2u << log : c << most;
                delta_bits[sym] = (most << 16) - threshold;
                delta_state[sym] = int32_t(first[sym]) - int32_t(c);
            }
            uint16_t next[256];
            for (unsigned sym = 0; sym < 256; ++sym) {
                next[sym] = first[sym];
            }
            for (unsigned u = 0; u < size; ++u) {
                states[next[table[u].symbol]++] = uint16_t(u + size);
            }
        }

        // The state a stream of this table ends in for its last symbol
        SGCL_INLINE_HOT uint32_t start(unsigned symbol) const noexcept {
            return states[first[symbol]];
        }

        SGCL_INLINE_HOT void encode(uint32_t& x, unsigned symbol, ZstdBitWriter& w) const noexcept {
            uint32_t v;
            const uint32_t bits = step(x, symbol, v);
            w.add(v, int(bits));
        }

        // One symbol: the bits the state gives up (into v; their count
        // returned), the state moved on
        SGCL_INLINE_HOT uint32_t step(uint32_t& x, unsigned symbol, uint32_t& v) const noexcept {
            const uint32_t bits = (x + delta_bits[symbol]) >> 16;
            v = x & ((uint32_t(1) << bits) - 1);
            x = states[int32_t(x >> bits) + delta_state[symbol]];
            return bits;
        }

        SGCL_INLINE_HOT void finish(uint32_t x, ZstdBitWriter& w) const noexcept {
            w.add(x - size, int(log));
        }

        // The cost of a symbol in 1/256 bits, near enough: log2(size / count)
        SGCL_INLINE_HOT uint32_t cost(unsigned symbol) const noexcept {
            const unsigned c = count[symbol];
            if (!c) {
                return UINT32_MAX / 4;
            }
            return fse_log2_256(size) - fse_log2_256(c);
        }

        static uint32_t fse_log2_256(uint32_t v) noexcept {
            // log2 in 1/256 units: the integer part and a linear fraction
            const unsigned hb = 31 - unsigned(std::countl_zero(v));
            const uint32_t frac = hb >= 8 ? (v >> (hb - 8)) & 0xFF : (v << (8 - hb)) & 0xFF;
            return hb * 256 + frac;
        }
    };

    // ---- Huffman, encoding ---------------------------------------------------

    // The code lengths of an optimal prefix code for the counts, none longer
    // than max_bits: Huffman's by merging the two lightest, then lengths past
    // the limit cut to it and the code made whole again by lengthening the
    // longest codes still short of it (the Kraft sum back to one), and
    // shortening where room is left
    // (N: the largest alphabet; lengths has N entries, every one written)
    template<unsigned N>
    inline unsigned huf_lengths_n(const uint32_t* counts, unsigned symbols, unsigned max_bits, uint8_t* lengths) noexcept {
        struct Node {
            uint64_t weight;
            int left, right;
        };
        Node nodes[2 * N];
        int order[N];
        unsigned n = 0;
        for (unsigned s = 0; s < symbols; ++s) {
            lengths[s] = 0;
            if (counts[s]) {
                nodes[n] = {counts[s], -1, int(s)};
                order[n] = int(n);
                ++n;
            }
        }
        for (unsigned s = symbols; s < N; ++s) {
            lengths[s] = 0;
        }
        if (n < 2) {
            return 0;
        }
        std::sort(order, order + n, [&](int a, int b) { return nodes[a].weight < nodes[b].weight || (nodes[a].weight == nodes[b].weight && a < b); });
        // two queues: the leaves sorted, the merged nodes in the order made
        int leaves[N];
        for (unsigned i = 0; i < n; ++i) {
            leaves[i] = order[i];
        }
        int merged[N];
        unsigned li = 0, mi = 0, mn = 0;
        unsigned count = n;
        auto take = [&]() noexcept {
            if (li < n && (mi >= mn || nodes[leaves[li]].weight <= nodes[merged[mi]].weight)) {
                return leaves[li++];
            }
            return merged[mi++];
        };
        while ((n - li) + (mn - mi) > 1) {
            const int a = take();
            const int b = take();
            nodes[count] = {nodes[a].weight + nodes[b].weight, a, b};
            merged[mn++] = int(count);
            ++count;
        }
        // depths by walking down from the root
        int stack[2 * N];
        uint8_t depth[2 * N];
        int top = 0;
        stack[top] = int(count - 1);
        depth[count - 1] = 0;
        ++top;
        unsigned longest = 0;
        while (top) {
            const int i = stack[--top];
            if (i < int(n)) {
                const unsigned s = unsigned(nodes[i].right);
                lengths[s] = uint8_t(std::max<unsigned>(depth[i], 1));
                longest = std::max<unsigned>(longest, lengths[s]);
            } else {
                depth[nodes[i].left] = uint8_t(depth[i] + 1);
                depth[nodes[i].right] = uint8_t(depth[i] + 1);
                stack[top++] = nodes[i].left;
                stack[top++] = nodes[i].right;
            }
        }
        if (longest > max_bits) {
            // cut, then mend the Kraft sum (in units of 2^-max_bits)
            int64_t kraft = 0;
            const int64_t full = int64_t(1) << max_bits;
            for (unsigned s = 0; s < symbols; ++s) {
                if (lengths[s] > max_bits) {
                    lengths[s] = uint8_t(max_bits);
                }
                if (lengths[s]) {
                    kraft += int64_t(1) << (max_bits - lengths[s]);
                }
            }
            // too much: lengthen the code of the rarest symbol that can grow
            while (kraft > full) {
                int best = -1;
                for (unsigned s = 0; s < symbols; ++s) {
                    if (lengths[s] && lengths[s] < max_bits && (best < 0 || counts[s] < counts[best] ||
                                                                (counts[s] == counts[best] && lengths[s] > lengths[best]))) {
                        best = int(s);
                    }
                }
                kraft -= int64_t(1) << (max_bits - lengths[best] - 1);
                ++lengths[best];
            }
            // room left: shorten the codes of the most frequent where it fits
            bool changed = true;
            while (kraft < full && changed) {
                changed = false;
                int best = -1;
                for (unsigned s = 0; s < symbols; ++s) {
                    if (lengths[s] > 1 && kraft + (int64_t(1) << (max_bits - lengths[s])) <= full &&
                        (best < 0 || counts[s] > counts[best])) {
                        best = int(s);
                    }
                }
                if (best >= 0) {
                    kraft += int64_t(1) << (max_bits - lengths[best]);
                    --lengths[best];
                    changed = true;
                }
            }
            longest = 0;
            for (unsigned s = 0; s < symbols; ++s) {
                longest = std::max<unsigned>(longest, lengths[s]);
            }
        }
        return longest;
    }

    inline unsigned huf_lengths(const uint32_t* counts, unsigned symbols, unsigned max_bits, uint8_t* lengths) noexcept {
        return huf_lengths_n<256>(counts, symbols, max_bits, lengths);
    }

    // The canonical codes of the lengths, laid out as the decoder fills its
    // table: by weight (max_bits + 1 - length) from the lowest up, by symbol
    // within a weight; a symbol's code is the index of its first entry over
    // the entries a weight takes
    inline void huf_codes(const uint8_t* lengths, unsigned symbols, unsigned max_bits, uint16_t* codes) noexcept {
        unsigned at = 0;
        for (unsigned weight = 1; weight <= max_bits; ++weight) {
            const unsigned length = max_bits + 1 - weight;
            for (unsigned s = 0; s < symbols; ++s) {
                if (lengths[s] == length) {
                    codes[s] = uint16_t(at >> (weight - 1));
                    at += 1u << (weight - 1);
                }
            }
        }
    }

    // A Huffman tree description of the lengths (the last symbol's weight
    // left out): the weights FSE-coded with two states, or four bits each,
    // whichever is shorter; its bytes, 0 when neither form can hold it
    inline size_t huf_write_table(const uint8_t* lengths, unsigned symbols, unsigned max_bits, uint8_t* out) noexcept {
        unsigned last = 0;
        for (unsigned s = 0; s < symbols; ++s) {
            if (lengths[s]) {
                last = s;
            }
        }
        uint8_t weights[256];
        for (unsigned s = 0; s < last; ++s) {
            weights[s] = lengths[s] ? uint8_t(max_bits + 1 - lengths[s]) : 0;
        }
        const unsigned n = last;   // weights written
        // the FSE form
        size_t fse = 0;
        uint8_t fse_out[256];
        if (n >= 2) {
            uint32_t counts[13] = {};
            unsigned top = 0;
            unsigned distinct = 0;
            for (unsigned i = 0; i < n; ++i) {
                distinct += counts[weights[i]]++ == 0;
                top = std::max<unsigned>(top, weights[i]);
            }
            if (distinct >= 2) {
                FseCounts c;
                const unsigned log = std::min(6u, std::max(5u, fse_choose_log(n, top, 6)));
                fse_normalize(counts, top + 1, n, log, c);
                FseEntry table[64];
                if (fse_build_decoder(c, table)) {
                    FseEncoder e;
                    e.build(table, c.log);
                    const size_t h = fse_write_counts(c, fse_out + 1);
                    uint8_t bits[256];
                    ZstdBitWriter w(bits);
                    // two states taking turns from the end: the last two
                    // weights start them, the rest go in backwards
                    uint32_t s1, s2;
                    unsigned i = n;
                    if (n & 1) {
                        s1 = e.start(weights[--i]);
                        s2 = e.start(weights[--i]);
                        e.encode(s1, weights[--i], w);
                    } else {
                        s2 = e.start(weights[--i]);
                        s1 = e.start(weights[--i]);
                    }
                    while (i) {
                        e.encode(s2, weights[--i], w);
                        w.flush();
                        if (!i) {
                            break;   // n odd and even both end on state 1's symbols
                        }
                        e.encode(s1, weights[--i], w);
                        w.flush();
                    }
                    e.finish(s2, w);
                    e.finish(s1, w);
                    const size_t b = w.close();
                    if (1 + h + b <= 128 && h + b < 128) {
                        fse_out[0] = uint8_t(h + b);
                        sgcl::detail::copy_bytes(fse_out + 1 + h, bits, b);
                        fse = 1 + h + b;
                    }
                }
            }
        }
        // the direct form: up to 128 weights
        const size_t direct = n <= 128 ? 1 + (n + 1) / 2 : SIZE_MAX;
        if (fse && fse <= direct) {
            sgcl::detail::copy_bytes(out, fse_out, fse);
            return fse;
        }
        if (direct == SIZE_MAX) {
            return 0;
        }
        out[0] = uint8_t(127 + n);
        for (unsigned i = 0; i < n; i += 2) {
            out[1 + i / 2] = uint8_t(weights[i] << 4 | (i + 1 < n ? weights[i + 1] : 0));
        }
        return direct;
    }

    // The literals as one Huffman stream, written backwards so that the
    // decoder makes them forwards; its bytes (out has n + 8 bytes of room)
    inline size_t huf_encode_stream(const uint8_t* p, size_t n, const uint8_t* lengths, const uint16_t* codes, uint8_t* out) noexcept {
        ZstdBitWriter w(out);
        size_t i = n;
        while (i >= 4) {
            for (int k = 0; k < 4; ++k) {
                const uint8_t s = p[--i];
                w.add(codes[s], lengths[s]);
            }
            w.flush();
        }
        while (i) {
            const uint8_t s = p[--i];
            w.add(codes[s], lengths[s]);
        }
        w.flush();
        return w.close();
    }
}
