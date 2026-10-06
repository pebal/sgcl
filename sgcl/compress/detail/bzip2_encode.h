//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "block.h"
#include "bzip2.h"
#include "stream.h"
#include "zstd_entropy.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

// bzip2's compressor, written from the format (bzip2 1.0 and the readers
// that define it; there is no RFC).
//
// The input goes through the first run-length pass (four bytes of a run,
// then a byte of how many more, up to 251) into a block of up to
// 100 KB × level - 19 bytes; the block's rotations are sorted (the Burrows-
// Wheeler transform: the suffixes of the block written twice, sorted by
// SA-IS in linear time, the ones that start in the first copy kept); the
// last column goes through move-to-front, its runs of zeros written in
// bijective base 2 (RUNA, RUNB), and the symbols are coded by two to six
// Huffman tables chosen for each 50 of them, refined four times. A stream
// is "BZh" and the level, the blocks (each with the CRC of its bytes), the
// end marker and the CRC of the blocks' CRCs.
namespace sgcl::compress::detail {
    // ---- suffix sorting --------------------------------------------------------

    // SA-IS (Nong, Zhang and Chan, 2009): the suffix array of s[0, n), its
    // last symbol a unique 0, the others in [1, k); sa gets n entries
    inline void bzip2_sais(const int32_t* s, int32_t* sa, int32_t n, int32_t k) noexcept {
        std::vector<uint8_t> stype(static_cast<size_t>(n));   // 1: S, 0: L
        stype[size_t(n - 1)] = 1;
        for (int32_t i = n - 2; i >= 0; --i) {
            stype[size_t(i)] = s[i] < s[i + 1] || (s[i] == s[i + 1] && stype[size_t(i + 1)]);
        }
        auto lms = [&](int32_t i) noexcept { return i > 0 && stype[size_t(i)] && !stype[size_t(i - 1)]; };
        std::vector<int32_t> count(size_t(k) + 1, 0);
        for (int32_t i = 0; i < n; ++i) {
            ++count[size_t(s[i])];
        }
        std::vector<int32_t> bucket(static_cast<size_t>(k));
        auto starts = [&]() noexcept {
            int32_t sum = 0;
            for (int32_t c = 0; c < k; ++c) {
                bucket[size_t(c)] = sum;
                sum += count[size_t(c)];
            }
        };
        auto ends = [&]() noexcept {
            int32_t sum = 0;
            for (int32_t c = 0; c < k; ++c) {
                sum += count[size_t(c)];
                bucket[size_t(c)] = sum;
            }
        };
        auto induce = [&]() noexcept {
            starts();
            for (int32_t i = 0; i < n; ++i) {
                const int32_t j = sa[i] - 1;
                if (sa[i] > 0 && !stype[size_t(j)]) {
                    sa[bucket[size_t(s[j])]++] = j;
                }
            }
            ends();
            for (int32_t i = n - 1; i >= 0; --i) {
                const int32_t j = sa[i] - 1;
                if (sa[i] > 0 && stype[size_t(j)]) {
                    sa[--bucket[size_t(s[j])]] = j;
                }
            }
        };
        // the LMS suffixes at their buckets' ends, induced: the LMS substrings sorted
        std::fill(sa, sa + n, -1);
        ends();
        for (int32_t i = 1; i < n; ++i) {
            if (lms(i)) {
                sa[--bucket[size_t(s[i])]] = i;
            }
        }
        induce();
        // the sorted LMS substrings to the front, named
        int32_t n1 = 0;
        for (int32_t i = 0; i < n; ++i) {
            if (lms(sa[i])) {
                sa[n1++] = sa[i];
            }
        }
        std::fill(sa + n1, sa + n, -1);
        int32_t names = 0;
        int32_t prev = -1;
        for (int32_t i = 0; i < n1; ++i) {
            const int32_t pos = sa[i];
            bool differ = false;
            for (int32_t d = 0; d < n; ++d) {
                if (prev == -1 || s[pos + d] != s[prev + d] || stype[size_t(pos + d)] != stype[size_t(prev + d)]) {
                    differ = true;
                    break;
                }
                if (d > 0 && (lms(pos + d) || lms(prev + d))) {
                    break;
                }
            }
            if (differ) {
                ++names;
                prev = pos;
            }
            sa[n1 + pos / 2] = names - 1;
        }
        for (int32_t i = n - 1, j = n - 1; i >= n1; --i) {
            if (sa[i] >= 0) {
                sa[j--] = sa[i];
            }
        }
        // the reduced string's suffixes sorted: by recursion, or at once
        int32_t* const s1 = sa + n - n1;
        if (names < n1) {
            std::vector<int32_t> copy(s1, s1 + n1);
            bzip2_sais(copy.data(), sa, n1, names);
        } else {
            for (int32_t i = 0; i < n1; ++i) {
                sa[s1[i]] = i;
            }
        }
        // the LMS suffixes in their order, induced into the whole
        for (int32_t i = 1, j = 0; i < n; ++i) {
            if (lms(i)) {
                s1[j++] = i;
            }
        }
        for (int32_t i = 0; i < n1; ++i) {
            sa[i] = s1[sa[i]];
        }
        std::fill(sa + n1, sa + n, -1);
        ends();
        for (int32_t i = n1 - 1; i >= 0; --i) {
            const int32_t j = sa[i];
            sa[i] = -1;
            sa[--bucket[size_t(s[j])]] = j;
        }
        induce();
    }

    // The start of the least rotation of the block t (given twice, tt =
    // t t: Booth's algorithm by the failure function, no index wrapped)
    inline int32_t bzip2_least_rotation(const uint8_t* tt, int32_t n, std::vector<int32_t>& f) noexcept {
        f.assign(size_t(2 * n), -1);
        int32_t k = 0;
        for (int32_t j = 1; j < 2 * n; ++j) {
            const uint8_t sj = tt[j];
            int32_t i = f[size_t(j - k - 1)];
            while (i != -1 && sj != tt[k + i + 1]) {
                if (sj < tt[k + i + 1]) {
                    k = j - i - 1;
                }
                i = f[size_t(i)];
            }
            if (sj != tt[k + i + 1]) {   // i == -1
                if (sj < tt[k]) {
                    k = j;
                }
                f[size_t(j - k)] = -1;
            } else {
                f[size_t(j - k)] = i + 1;
            }
        }
        return k % n;
    }

    // The block's rotations sorted: the last column into last and the place
    // of the rotation from 0 (the format's origPtr). The block turned to its
    // least rotation is a Lyndon word or a power of one; the rotations of a
    // Lyndon word sort as its suffixes do, so its suffixes are sorted (with
    // an end smaller than every byte) and turned back. A power of a shorter
    // word (a periodic block) sorts as the block written twice.
    inline uint32_t bzip2_bwt(const uint8_t* block, int32_t n, uint8_t* last, std::vector<int32_t>& s, std::vector<int32_t>& sa,
                              std::vector<uint8_t>& tt) noexcept {
        tt.resize(size_t(2 * n) + 1);
        std::copy(block, block + n, tt.data());
        std::copy(block, block + n, tt.data() + n);
        const int32_t r = bzip2_least_rotation(tt.data(), n, sa);
        const uint8_t* const w = tt.data() + r;   // the turned block, w[0, n) (and on, for the doubled form)
        // whether the turned block is a power of a shorter word: its least
        // period (the length less its longest border, by the prefix
        // function) divides it
        bool periodic = false;
        {
            std::vector<int32_t>& pi = s;   // borrowed before it holds the string
            pi.assign(size_t(n), 0);
            for (int32_t i = 1; i < n; ++i) {
                int32_t k = pi[size_t(i - 1)];
                const uint8_t c = w[i];
                while (k > 0 && c != w[k]) {
                    k = pi[size_t(k - 1)];
                }
                if (c == w[k]) {
                    ++k;
                }
                pi[size_t(i)] = k;
            }
            const int32_t period = n - pi[size_t(n - 1)];
            periodic = period < n && n % period == 0;
        }
        const int32_t copies = periodic ? 2 : 1;
        const int32_t m = copies * n + 1;
        s.resize(size_t(m));
        sa.resize(size_t(m));
        for (int32_t c = 0; c < copies; ++c) {
            for (int32_t i = 0; i < n; ++i) {
                s[size_t(c * n + i)] = int32_t(w[i]) + 1;
            }
        }
        s[size_t(m - 1)] = 0;
        bzip2_sais(s.data(), sa.data(), m, 257);
        uint32_t orig = 0;
        int32_t k = 0;
        const int32_t back = n - r;   // the rotation from 0 of the block is w's from back (mod n)
        for (int32_t i = 0; i < m; ++i) {
            const int32_t q = sa[size_t(i)];
            if (q < n) {
                if (q == back % n) {
                    orig = uint32_t(k);
                }
                last[k++] = w[q == 0 ? n - 1 : q - 1];
            }
        }
        return orig;
    }

    // ---- the bit writer ----------------------------------------------------------

    // Bits most significant first, into a vector
    struct Bzip2BitWriter {
        std::vector<uint8_t> buf;
        uint64_t acc = 0;
        unsigned bits = 0;

        SGCL_INLINE_HOT void add(uint32_t v, unsigned n) noexcept {
            acc = (acc << n) | v;
            bits += n;
            if (bits >= 32) {
                bits -= 32;
                const uint32_t w = uint32_t(acc >> bits);
                const size_t at = buf.size();
                buf.resize(at + 4);
                buf[at] = uint8_t(w >> 24);
                buf[at + 1] = uint8_t(w >> 16);
                buf[at + 2] = uint8_t(w >> 8);
                buf[at + 3] = uint8_t(w);
            }
        }

        // The last partial byte padded with zeros
        void align() noexcept {
            while (bits >= 8) {
                bits -= 8;
                buf.push_back(uint8_t(acc >> bits));
            }
            if (bits) {
                buf.push_back(uint8_t(acc << (8 - bits)));
                bits = 0;
            }
            acc = 0;
        }

        // The whole bytes made, taken by the caller (the partial one waits)
        template<class Out>
        void drain(Out& out) noexcept {
            while (bits >= 8) {
                bits -= 8;
                buf.push_back(uint8_t(acc >> bits));
            }
            if (!buf.empty()) {
                append_bytes(out, buf.data(), buf.size());
                buf.clear();
            }
        }
    };

    // ---- the encoder ---------------------------------------------------------------

    class Bzip2Encoder {
    public:
        static constexpr const char* name = "bzip2";

        template<class O>
        explicit Bzip2Encoder(const O& o, uint64_t = UINT64_MAX) noexcept
        : _level(o.level.value()) {
            if (_level < 1 || _level > 9) {
                _error = "a level of 1..9";
                return;
            }
            _max = size_t(_level) * 100000 - 19;
            _block.reset(new uint8_t[_max + 8]);
            _last.reset(new uint8_t[_max + 8]);
            reset();
        }

        explicit Bzip2Encoder(int level) noexcept
        : Bzip2Encoder(Options {level}) {
        }

        SGCL_INLINE_HOT const char* setup_error() const noexcept {
            return _error;
        }

        template<class Out>
        void start(Out&) noexcept {
        }

        template<class Out>
        void write(const uint8_t* p, size_t n, Out& out) noexcept {
            for (size_t i = 0; i < n; ++i) {
                const uint8_t b = p[i];
                if (_run && b == _run_byte && _run < 255) {
                    ++_run;
                    continue;
                }
                _put_run(out);
                _run_byte = b;
                _run = 1;
            }
        }

        // The block so far and the end of the stream: decodable at once (the
        // next write begins another stream)
        template<class Out>
        void flush(Out& out) noexcept {
            _put_run(out);
            if (_size) {
                _compress_block(out);
            }
            if (_open) {
                _end_stream(out);
            }
        }

        template<class Out>
        void finish(Out& out) noexcept {
            _put_run(out);
            if (_size) {
                _compress_block(out);
            }
            if (_open || !_any) {
                if (!_open) {
                    _begin_stream();
                }
                _end_stream(out);
            }
        }

        void reset() noexcept {
            if (_error) {
                return;
            }
            _w = Bzip2BitWriter {};
            _size = 0;
            _run = 0;
            _crc = 0xFFFFFFFFu;
            _open = false;
            _any = false;
            _stream_crc = 0;
        }

    private:
        struct Options {
            struct L {
                int v;
                int value() const noexcept {
                    return v;
                }
            } level;
        };

        // The run gathered, through the first run-length pass, into the block
        template<class Out>
        void _put_run(Out& out) noexcept {
            if (!_run) {
                return;
            }
            const unsigned bytes = _run < 4 ? _run : 5;
            if (_size + bytes > _max) {
                _compress_block(out);   // the block is full: the run begins the next
            }
            // the block's CRC over the bytes of the run
            for (unsigned i = 0; i < _run; ++i) {
                _crc = (_crc << 8) ^ Bzip2Crc.t[0][(_crc >> 24) ^ _run_byte];
            }
            if (_run < 4) {
                for (unsigned i = 0; i < _run; ++i) {
                    _block[_size++] = _run_byte;
                }
            } else {
                for (unsigned i = 0; i < 4; ++i) {
                    _block[_size++] = _run_byte;
                }
                _block[_size++] = uint8_t(_run - 4);
            }
            _run = 0;
        }

        void _begin_stream() noexcept {
            _w.add('B', 8);
            _w.add('Z', 8);
            _w.add('h', 8);
            _w.add(uint32_t('0' + _level), 8);
            _open = true;
            _any = true;
            _stream_crc = 0;
        }

        template<class Out>
        void _end_stream(Out& out) noexcept {
            _w.add(0x177245, 24);
            _w.add(0x385090, 24);
            _w.add(_stream_crc, 32);
            _w.align();
            _w.drain(out);
            _open = false;
        }

        template<class Out>
        void _compress_block(Out& out) noexcept;

        int _level = 9;
        const char* _error = nullptr;
        size_t _max = 0;
        std::unique_ptr<uint8_t[]> _block;
        std::unique_ptr<uint8_t[]> _last;
        size_t _size = 0;
        uint8_t _run_byte = 0;
        unsigned _run = 0;
        uint32_t _crc = 0xFFFFFFFFu;       // the register over the block's input so far
        uint32_t _stream_crc = 0;
        bool _open = false;
        bool _any = false;
        Bzip2BitWriter _w;
        std::vector<int32_t> _s, _sa;
        std::vector<uint8_t> _tt;
        std::vector<uint16_t> _mtf;
    };

    // One block: the transform, move-to-front with the runs of zeros, the
    // tables and the selectors, the symbols
    template<class Out>
    void Bzip2Encoder::_compress_block(Out& out) noexcept {
        if (!_open) {
            _begin_stream();
        }
        const uint32_t block_crc = ~_crc;
        _stream_crc = ((_stream_crc << 1) | (_stream_crc >> 31)) ^ block_crc;
        const int32_t n = int32_t(_size);
        const uint32_t orig = bzip2_bwt(_block.get(), n, _last.get(), _s, _sa, _tt);
        const uint8_t* const last = _last.get();
        // the bytes in use, numbered in order
        bool used[256] = {};
        for (int32_t i = 0; i < n; ++i) {
            used[last[i]] = true;
        }
        uint8_t seq[256];
        unsigned in_use = 0;
        for (unsigned c = 0; c < 256; ++c) {
            if (used[c]) {
                seq[c] = uint8_t(in_use++);
            }
        }
        const unsigned alpha = in_use + 2;
        const uint16_t eob = uint16_t(in_use + 1);
        // move to front, the runs of zeros in bijective base 2 (RUNA 0, RUNB 1)
        _mtf.clear();
        _mtf.reserve(size_t(n) + 1);
        uint32_t freq[258] = {};
        uint8_t yy[256];
        for (unsigned i = 0; i < in_use; ++i) {
            yy[i] = uint8_t(i);
        }
        uint32_t zeros = 0;
        auto runs = [&]() noexcept {
            if (!zeros) {
                return;
            }
            uint32_t z = zeros - 1;
            for (;;) {
                const uint16_t sym = uint16_t(z & 1);
                _mtf.push_back(sym);
                ++freq[sym];
                if (z < 2) {
                    break;
                }
                z = (z - 2) / 2;
            }
            zeros = 0;
        };
        for (int32_t i = 0; i < n; ++i) {
            const uint8_t c = seq[last[i]];
            if (yy[0] == c) {
                ++zeros;
                continue;
            }
            runs();
            unsigned j = 1;
            uint8_t prev = yy[0];
            while (yy[j] != c) {
                const uint8_t t = yy[j];
                yy[j] = prev;
                prev = t;
                ++j;
            }
            yy[j] = prev;
            yy[0] = c;
            _mtf.push_back(uint16_t(j + 1));
            ++freq[j + 1];
        }
        runs();
        _mtf.push_back(eob);
        ++freq[eob];
        // the tables: as many as the symbols justify, first by ranges of the
        // alphabet of about equal weight, then refined: each 50 symbols to
        // the table that codes them shortest, the tables made again from
        // what they were given
        const size_t nmtf = _mtf.size();
        const unsigned groups = nmtf < 200 ? 2 : nmtf < 600 ? 3 : nmtf < 1200 ? 4 : nmtf < 2400 ? 5 : 6;
        const size_t nsel = (nmtf + 49) / 50;
        uint8_t len[6][258];
        {
            size_t left = nmtf;
            unsigned from = 0;
            for (unsigned t = 0; t < groups; ++t) {
                const size_t target = left / (groups - t);
                size_t got = 0;
                unsigned to = from;
                while (to < alpha && (got < target || to == from)) {
                    got += freq[to++];
                }
                if (t + 1 == groups) {
                    to = alpha;
                }
                for (unsigned v = 0; v < alpha; ++v) {
                    len[t][v] = v >= from && v < to ? 0 : 15;
                }
                from = to;
                left = left > got ? left - got : 0;
            }
        }
        std::vector<uint8_t> sel(nsel);
        for (int pass = 0; pass < 4; ++pass) {
            uint32_t tf[6][258] = {};
            // a symbol's lengths in the tables, ten bits each in one word
            // (50 of 17 bits at most sum to under 1024): a group's costs in
            // one sum
            uint64_t packed[258];
            for (unsigned v = 0; v < alpha; ++v) {
                uint64_t x = 0;
                for (unsigned t = 0; t < groups; ++t) {
                    x |= uint64_t(len[t][v]) << (10 * t);
                }
                packed[v] = x;
            }
            for (size_t g = 0; g < nsel; ++g) {
                const size_t a = g * 50;
                const size_t b = std::min(a + 50, nmtf);
                uint64_t sum = 0;
                for (size_t i = a; i < b; ++i) {
                    sum += packed[_mtf[i]];
                }
                unsigned best = 0;
                uint32_t best_cost = UINT32_MAX;
                for (unsigned t = 0; t < groups; ++t) {
                    const uint32_t cost = uint32_t(sum >> (10 * t)) & 1023;
                    if (cost < best_cost) {
                        best_cost = cost;
                        best = t;
                    }
                }
                sel[g] = uint8_t(best);
                for (size_t i = a; i < b; ++i) {
                    ++tf[best][_mtf[i]];
                }
            }
            for (unsigned t = 0; t < groups; ++t) {
                uint32_t counts[258];
                for (unsigned v = 0; v < alpha; ++v) {
                    counts[v] = tf[t][v] ? tf[t][v] * 2 : 1;   // every symbol a code
                }
                huf_lengths_n<258>(counts, alpha, 17, len[t]);
            }
        }
        // the codes, canonical by length and then symbol
        uint32_t code[6][258];
        for (unsigned t = 0; t < groups; ++t) {
            uint32_t next = 0;
            for (unsigned l = 1; l <= 20; ++l) {
                for (unsigned v = 0; v < alpha; ++v) {
                    if (len[t][v] == l) {
                        code[t][v] = next++;
                    }
                }
                next <<= 1;
            }
        }
        // the block
        Bzip2BitWriter& w = _w;
        w.add(0x314159, 24);
        w.add(0x265359, 24);
        w.add(block_crc, 32);
        w.add(0, 1);   // not randomised
        w.add(orig, 24);
        uint32_t ranges = 0;
        for (unsigned i = 0; i < 16; ++i) {
            for (unsigned j = 0; j < 16; ++j) {
                if (used[i * 16 + j]) {
                    ranges |= 1u << (15 - i);
                }
            }
        }
        w.add(ranges, 16);
        for (unsigned i = 0; i < 16; ++i) {
            if (ranges & (1u << (15 - i))) {
                uint32_t bits = 0;
                for (unsigned j = 0; j < 16; ++j) {
                    if (used[i * 16 + j]) {
                        bits |= 1u << (15 - j);
                    }
                }
                w.add(bits, 16);
            }
        }
        w.add(groups, 3);
        w.add(uint32_t(nsel), 15);
        uint8_t order[6] = {0, 1, 2, 3, 4, 5};
        for (size_t g = 0; g < nsel; ++g) {
            unsigned j = 0;
            while (order[j] != sel[g]) {
                ++j;
            }
            for (unsigned k = j; k > 0; --k) {
                order[k] = order[k - 1];
            }
            order[0] = sel[g];
            w.add((1u << (j + 1)) - 2, j + 1);   // j ones and a zero
        }
        for (unsigned t = 0; t < groups; ++t) {
            unsigned cur = len[t][0];
            w.add(cur, 5);
            for (unsigned v = 0; v < alpha; ++v) {
                while (cur < len[t][v]) {
                    w.add(2, 2);
                    ++cur;
                }
                while (cur > len[t][v]) {
                    w.add(3, 2);
                    --cur;
                }
                w.add(0, 1);
            }
        }
        for (size_t g = 0; g < nsel; ++g) {
            const unsigned t = sel[g];
            const size_t a = g * 50;
            const size_t b = std::min(a + 50, nmtf);
            for (size_t i = a; i < b; ++i) {
                w.add(code[t][_mtf[i]], len[t][_mtf[i]]);
            }
        }
        w.drain(out);
        _size = 0;
        _crc = 0xFFFFFFFFu;
    }
}
