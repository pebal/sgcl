//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lz4_block.h"
#include "zstd_block_encode.h"
#include "zstd_opt.h"
#include "zstd_params.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <vector>

// Zstandard's match finders, by strategy (the levels' parameters below):
//   - fast: one table of the last position under a hash of min_match
//     bytes, two positions probed side by side, the step growing over a run
//     of misses (and by the acceleration of a negative level);
//   - dfast: two tables, a long hash of eight bytes and a short one of
//     min_match, a short match weighed against a long one a byte on;
//   - greedy, lazy, lazy2: rows of the last positions under a hash, each
//     with a tag of 8 more hash bits; the positions whose tag agrees are
//     tried, 2^search_log at most, and the match here weighed against one,
//     or two, bytes on (gain: four a byte, less the offset's bits);
//   - btlazy2: the same lazy parse over a binary tree of the window's
//     suffixes, which finds the longest matches without walking past the
//     shorter ones; the optimal levels price the ways through a stretch
//     with every match the tree gives (zstd_opt.h).
// Positions are 32-bit indexes from a base the caller moves with its
// window, as the LZ4 engine's are; no match reaches before `low` or
// farther back than the window.
namespace sgcl::compress::detail {
    // A hash of the first `mls` bytes (4 to 8) at p into `bits` bits
    SGCL_INLINE_HOT uint32_t zstd_hash(const uint8_t* p, unsigned mls, unsigned bits) noexcept {
        const uint64_t v = zstd_read64(p);
        switch (mls) {
            case 4: return uint32_t((uint32_t(v) * 2654435761u) >> (32 - bits));
            case 5: return uint32_t(((v << 24) * 889523592379ull) >> (64 - bits));
            case 6: return uint32_t(((v << 16) * 227718039650203ull) >> (64 - bits));
            case 7: return uint32_t(((v << 8) * 58295818150454627ull) >> (64 - bits));
            default: return uint32_t((v * 0xCF1BBCDCB7A56463ull) >> (64 - bits));
        }
    }

    // The bits of an offset, for the lazy parse's gain
    SGCL_INLINE_HOT int zstd_offset_bits(uint32_t o) noexcept {
        return 31 - std::countl_zero(o + 1);
    }

    // The match finder of a frame: its tables live as long as it does
    class ZstdMatcher {
    public:
        void configure(const ZstdParams& p) noexcept {
            _p = p;
            const bool rows = p.strategy >= ZstdStrategy::greedy && p.strategy <= ZstdStrategy::lazy2;
            const bool own_hash = p.strategy <= ZstdStrategy::dfast;
            const size_t hs = own_hash ? size_t(1) << p.hash_log : 0;
            const size_t cs = size_t(1) << p.chain_log;
            if (_hash_size != hs) {
                _hash.reset(hs ? new uint32_t[hs] : nullptr);
                _hash_size = hs;
            }
            if (p.strategy >= ZstdStrategy::btopt) {
                if (!_opt) {
                    _opt = std::make_unique<ZstdOptimal>();
                }
                _opt->configure(p);
            }
            const size_t need_chain = p.strategy == ZstdStrategy::dfast ? cs : 0;
            if (p.strategy == ZstdStrategy::btlazy2) {
                if (!_bt) {
                    _bt = std::make_unique<ZstdTree>();
                }
                _bt->configure(p.hash_log, p.chain_log, p.search_log, p.min_match);
            }
            if (_chain_size != need_chain) {
                _chain.reset(need_chain ? new uint32_t[need_chain] : nullptr);
                _chain_size = need_chain;
            }
            // rows for greedy, lazy and lazy2: 2^row_log entries each (16,
            // 32 or 64, as deep as the search), 2^hash_log entries in all
            _row_log = 0;
            size_t need_rows = 0;
            if (rows) {
                _row_log = std::clamp(p.search_log, 4u, 6u);
                need_rows = size_t(1) << std::max(p.hash_log, _row_log);
                _row_keep = 64 - 8 * std::clamp(p.min_match, 4u, 6u);
                _row_drop = 64 - (std::max(p.hash_log, _row_log) - _row_log + 8);
                _row_attempts = 1u << std::min(p.search_log, _row_log);
            }
            if (_rows_size != need_rows) {
                _row_pos.reset(need_rows ? new uint32_t[need_rows] : nullptr);
                _row_tag.reset(need_rows ? new uint8_t[need_rows] : nullptr);
                _row_head.reset(need_rows ? new uint8_t[need_rows >> _row_log] : nullptr);
                _rows_size = need_rows;
            }
            reset();
        }

        void reset() noexcept {
            if (_opt && _p.strategy >= ZstdStrategy::btopt) {
                _opt->reset();
            }
            if (_bt && _p.strategy == ZstdStrategy::btlazy2) {
                _bt->reset();
            }
            if (_rows_size) {
                std::fill(_row_pos.get(), _row_pos.get() + _rows_size, 0u);
                std::fill(_row_tag.get(), _row_tag.get() + _rows_size, uint8_t(0));
                std::fill(_row_head.get(), _row_head.get() + (_rows_size >> _row_log), uint8_t(0));
            }
            std::fill(_hash.get(), _hash.get() + _hash_size, 0u);
            if (_chain) {
                std::fill(_chain.get(), _chain.get() + _chain_size, 0u);
            }
            _next = 0;
            _hc_begin = _hc_end = 0;
        }

        SGCL_INLINE_HOT const ZstdParams& params() const noexcept {
            return _p;
        }

        // Every index lowered by delta (0 below it): the window's base moved
        void shift(uint32_t delta) noexcept {
            auto down = [delta](uint32_t* t, size_t n) noexcept {
                for (size_t i = 0; i < n; ++i) {
                    t[i] = t[i] > delta ? t[i] - delta : 0;
                }
            };
            if (_opt && _p.strategy >= ZstdStrategy::btopt) {
                _opt->shift(delta);
            }
            if (_bt && _p.strategy == ZstdStrategy::btlazy2) {
                _bt->shift(delta);
            }
            down(_hash.get(), _hash_size);
            if (_rows_size) {
                down(_row_pos.get(), _rows_size);
            }
            if (_chain) {
                down(_chain.get(), _chain_size);
            }
            _next = _next > delta ? _next - delta : 0;
            _hc_begin = _hc_end = _next;
        }

        // The positions of [from, to) into the tables, as a dictionary
        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            if (to - from < 8) {
                return;
            }
            _base = base;
            _low = uint32_t(from - base);
            _next = _low;
            if (_p.strategy >= ZstdStrategy::btopt) {
                _opt->load(base, from, to);
            } else if (_p.strategy == ZstdStrategy::fast) {
                const unsigned mls = std::clamp(_p.min_match, 4u, 8u);
                for (const uint8_t* q = from; q + 8 <= to; ++q) {
                    _hash.get()[zstd_hash(q, mls, _p.hash_log)] = uint32_t(q - base);
                }
            } else if (_p.strategy == ZstdStrategy::dfast) {
                const unsigned mls = std::clamp(_p.min_match, 4u, 8u);
                for (const uint8_t* q = from; q + 8 <= to; ++q) {
                    _hash.get()[zstd_hash(q, 8, _p.hash_log)] = uint32_t(q - base);
                    _chain.get()[zstd_hash(q, mls, _p.chain_log)] = uint32_t(q - base);
                }
            } else if (_rows_size) {
                _ahead_end = uint32_t(to - 8 - base);
                // every position of a dictionary or a history, none skipped
                const uint32_t target = uint32_t(to - 8 - base);
                if (target > _next) {
                    _row_insert_range(_next, target);
                    _next = target;
                }
            } else if (_p.strategy == ZstdStrategy::btlazy2) {
                _bt->update(base, _low, to - 8, to);
            }
        }

        // The sequences of the block [src, src + n) and its literals into
        // out; matches reach back to `low`, and to the window; rep the
        // offsets as they stand before the block, moved on as the decoder will
        void parse(const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& out) noexcept {
            _base = base;
            const uint32_t here = uint32_t(src - base);
            // the window measured from the block's end: no match of the
            // block reaches farther back than the window
            const uint32_t window = (uint32_t(1) << _p.window_log) - _p.window_trim;
            const uint32_t end = here + uint32_t(n);
            _low = std::max(uint32_t(low - base), end > window ? end - window : 0u);
            if (_next < _low) {
                _next = _low;
            }
            _ahead_end = n >= 8 ? here + uint32_t(n) - 8 : here;
            switch (_p.strategy) {
                case ZstdStrategy::fast:
                    _fast(src, n, rep, out);
                    break;
                case ZstdStrategy::dfast:
                    _dfast(src, n, rep, out);
                    break;
                case ZstdStrategy::btopt:
                case ZstdStrategy::btultra:
                case ZstdStrategy::btultra2:
                    _opt->parse(base, _low, src, n, rep, out);
                    break;
                default:
                    _lazy(src, n, rep, out);
                    break;
            }
        }

    private:
        // A candidate is one when it lies in [low, here): one test
        SGCL_INLINE_HOT bool _valid(uint32_t candidate, uint32_t here) const noexcept {
            return uint32_t(here - 1 - candidate) < uint32_t(here - _low);
        }

        // fast: two positions probed side by side, then a step that grows by
        // one every 128 bytes with no match (from 2, more for a negative
        // level); a match found takes the bytes before it that agree, two of
        // its positions go into the table, and the second offset is tried
        // again right after it (a run of repeats, no literals between)
        void _fast(const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& sink) noexcept {
            switch (std::clamp(_p.min_match, 4u, 8u)) {
                case 4: _fast_with<4>(src, n, rep, sink); break;
                case 5: _fast_with<5>(src, n, rep, sink); break;
                case 6: _fast_with<6>(src, n, rep, sink); break;
                case 7: _fast_with<7>(src, n, rep, sink); break;
                default: _fast_with<8>(src, n, rep, sink); break;
            }
        }

        template <unsigned Mls>
        void _fast_with(const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& sink) noexcept {
            ZstdSink out = sink;
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            if (n < 16) {
                out.rest(anchor, iend);
                sink = out;
                return;
            }
            const uint8_t* const ilimit = iend - 8;
            const unsigned bits = _p.hash_log;
            uint32_t* const table = _hash.get();
            const size_t first_step = size_t(_p.target_length) + 1 + (_p.target_length == 0);
            const uint8_t* const base = _base;
            const uint8_t* const window = base + _low;
            const uint32_t low = _low;
            size_t step = first_step;
            const uint8_t* next_step = ip + 128;
            ++ip;
            while (ip + 1 < ilimit) {
                const uint8_t* at;
                uint32_t offset;
                {
                    const uint32_t here0 = uint32_t(ip - base);
                    const uint32_t h0 = zstd_hash(ip, Mls, bits);
                    const uint32_t h1 = zstd_hash(ip + 1, Mls, bits);
                    const uint32_t c0 = table[h0];
                    table[h0] = here0;
                    const uint32_t r0 = rep[0];
                    const uint32_t v0 = lz4_read32(ip);
                    const uint32_t v1 = lz4_read32(ip + 1);
                    if (ip > anchor && r0 - 1 < here0 - low && lz4_read32(ip - r0) == v0) {
                        at = ip;
                        offset = r0;
                    } else if (uint32_t(here0 - 1 - c0) < uint32_t(here0 - low) && lz4_read32(base + c0) == v0) {
                        at = ip;
                        offset = here0 - c0;
                        table[h1] = here0 + 1;
                    } else {
                        const uint32_t c1 = table[h1];
                        table[h1] = here0 + 1;
                        if (r0 - 1 < here0 + 1 - low && lz4_read32(ip + 1 - r0) == v1) {
                            at = ip + 1;
                            offset = r0;
                        } else if (uint32_t(here0 - c1) < uint32_t(here0 + 1 - low) && lz4_read32(base + c1) == v1) {
                            at = ip + 1;
                            offset = here0 + 1 - c1;
                        } else {
                            ip += step;
                            if (ip >= next_step) {
                                ++step;
                                next_step += 128;
                            }
                            continue;
                        }
                    }
                }
                size_t len = 4 + lz4_count(at + 4, at + 4 - offset, iend);
                // the bytes before that agree
                const uint8_t* start = at;
                const uint8_t* ms = at - offset;
                while (start > anchor && ms > window && start[-1] == ms[-1]) {
                    --start;
                    --ms;
                    ++len;
                }
                const uint32_t start_index = uint32_t(at - base);
                out.match(anchor, start, zstd_offset_value(rep, offset, uint32_t(start - anchor)), len);
                ip = start + len;
                anchor = ip;
                step = first_step;
                next_step = ip + 128;
                if (ip <= ilimit) {
                    table[zstd_hash(base + start_index + 2, Mls, bits)] = start_index + 2;
                    table[zstd_hash(ip - 2, Mls, bits)] = uint32_t(ip - 2 - base);
                    // the second offset again, with no literals before it
                    while (ip <= ilimit && rep[1] && rep[1] <= uint32_t(ip - window) && lz4_read32(ip) == lz4_read32(ip - rep[1])) {
                        const size_t rl = 4 + lz4_count(ip + 4, ip + 4 - rep[1], iend);
                        table[zstd_hash(ip, Mls, bits)] = uint32_t(ip - base);
                        out.match(anchor, ip, 1, rl);
                        std::swap(rep[0], rep[1]);   // the second offset with no literals: value 1
                        ip += rl;
                        anchor = ip;
                    }
                }
            }
            out.rest(anchor, iend);
            sink = out;
        }

        // dfast: a long table and a short one
        void _dfast(const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& sink) noexcept {
            ZstdSink out = sink;
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            if (n < 16) {
                out.rest(anchor, iend);
                sink = out;
                return;
            }
            const uint8_t* const ilimit = iend - 8;
            const unsigned mls = std::clamp(_p.min_match, 4u, 8u);
            const unsigned lbits = _p.hash_log;
            const unsigned sbits = _p.chain_log;
            uint32_t* const ltable = _hash.get();
            uint32_t* const stable = _chain.get();
            auto put = [&](const uint8_t* p) noexcept {
                const uint32_t i = uint32_t(p - _base);
                ltable[zstd_hash(p, 8, lbits)] = i;
                stable[zstd_hash(p, mls, sbits)] = i;
            };
            ++ip;
            while (ip < ilimit) {
                const uint32_t here = uint32_t(ip - _base);
                const uint32_t hl = zstd_hash(ip, 8, lbits);
                const uint32_t hs = zstd_hash(ip, mls, sbits);
                const uint32_t cl = ltable[hl];
                const uint32_t cs = stable[hs];
                ltable[hl] = here;
                stable[hs] = here;
                const uint32_t r0 = rep[0];
                if (r0 && r0 <= here + 1 - _low && lz4_read32(ip + 1 - r0) == lz4_read32(ip + 1)) {
                    const uint8_t* m = ip + 1;
                    const size_t len = 4 + lz4_count(m + 4, m + 4 - r0, iend);
                    out.match(anchor, m, zstd_offset_value(rep, r0, uint32_t(m - anchor)), len);
                    ip = m + len;
                    anchor = ip;
                    if (ip < ilimit) {
                        put(ip - 2);
                        put(ip - 1);
                    }
                    continue;
                }
                size_t len = 0;
                uint32_t offset = 0;
                const uint8_t* start = ip;
                if (_valid(cl, here) && zstd_read64(_base + cl) == zstd_read64(ip)) {
                    len = 8 + lz4_count(ip + 8, _base + cl + 8, iend);
                    offset = here - cl;
                } else if (_valid(cs, here) && lz4_read32(_base + cs) == lz4_read32(ip)) {
                    len = 4 + lz4_count(ip + 4, _base + cs + 4, iend);
                    offset = here - cs;
                    // a long match a byte on may be better
                    if (ip + 1 < ilimit) {
                        const uint32_t h1 = zstd_hash(ip + 1, 8, lbits);
                        const uint32_t c1 = ltable[h1];
                        ltable[h1] = here + 1;
                        if (_valid(c1, here + 1) && zstd_read64(_base + c1) == zstd_read64(ip + 1)) {
                            const size_t l1 = 8 + lz4_count(ip + 9, _base + c1 + 8, iend);
                            if (l1 > len) {
                                len = l1;
                                offset = here + 1 - c1;
                                start = ip + 1;
                            }
                        }
                    }
                }
                if (!len) {
                    ip += (size_t(ip - anchor) >> 8) + 1;
                    continue;
                }
                const uint8_t* m = start - offset;
                while (start > anchor && m > _base + _low && start[-1] == m[-1]) {
                    --start;
                    --m;
                    ++len;
                }
                out.match(anchor, start, zstd_offset_value(rep, offset, uint32_t(start - anchor)), len);
                ip = start + len;
                anchor = ip;
                if (ip < ilimit) {
                    put(_base + here + 2 < ip ? _base + here + 2 : ip - 2);
                    put(ip - 2);
                    put(ip - 1);
                }
            }
            out.rest(anchor, iend);
            sink = out;
        }

        // ---- chains and trees ----

        struct Found {
            size_t length = 0;
            uint32_t offset = 0;
            int bits = 0;   // the offset's cost: its bits, none for a repeat

            SGCL_INLINE_HOT int gain() const noexcept {
                return int(length) * 4 - bits;
            }
        };

        // ---- rows ----
        // A row holds the last 2^row_log positions whose hash picks it, a
        // ring with its newest entry at the head, and beside each a tag: 8
        // more bits of the hash. A search compares the tag with the row's
        // tags eight at a time (a byte equal to it found by the carry-free
        // zero-byte test) and tries the positions that agree, newest first.

        // (the first min_match bytes kept by a shift, one multiply: no
        // branch on min_match)
        SGCL_INLINE_HOT uint32_t _row_hash(const uint8_t* p) const noexcept {
            return uint32_t(((zstd_read64(p) << _row_keep) * 0xCF1BBCDCB7A56463ull) >> _row_drop);
        }

        // A position's hash is taken eight positions ahead, its row fetched
        // into the cache then, while the rows between are worked on; the
        // hashes of [_hc_begin, _hc_end) wait in a ring of 16. Past a long
        // match (more than RowSkip positions to insert) only its first
        // RowSkipHead and its last RowSkipTail go in: the positions inside
        // a long match seldom begin a better one, and inserting them all
        // made a run of zeros of a program cost more than its search
        static constexpr uint32_t RowSkip = 384;
        static constexpr uint32_t RowSkipHead = 96;
        static constexpr uint32_t RowSkipTail = 32;

        SGCL_INLINE_HOT void _row_insert_to(const uint8_t* p) noexcept {
            const uint32_t target = uint32_t(p - _base);
            if (target > _next && target - _next > RowSkip) [[unlikely]] {
                _row_insert_range(_next, _next + RowSkipHead);
                _next = target - RowSkipTail;
            }
            _row_insert_range(_next, target);
            if (target > _next) {
                _next = target;
            }
        }

        SGCL_INLINE_HOT void _row_insert_range(uint32_t from, uint32_t target) noexcept {
            const uint32_t mask = (uint32_t(1) << _row_log) - 1;
            const uint32_t ahead_end = _ahead_end;
            uint32_t hb = _hc_begin;
            uint32_t he = _hc_end;
            if (he < from || hb > from) {
                hb = he = from;
            }
            uint32_t* const hc = _hc;
            uint8_t* const tags = _row_tag.get();
            uint32_t* const pos = _row_pos.get();
            uint8_t* const heads = _row_head.get();
            const uint8_t* const base = _base;
            const unsigned row_log = _row_log;
            for (uint32_t idx = from; idx < target; ++idx) {
                const uint32_t want = std::min(idx + 9, ahead_end);
                while (he < want) {
                    const uint32_t a = _row_hash(base + he);
                    hc[he & 15] = a;
                    const size_t ahead = size_t(a >> 8) << row_log;
                    __builtin_prefetch(tags + ahead);
                    __builtin_prefetch(pos + ahead);
                    ++he;
                }
                const uint32_t h = hc[idx & 15];   // idx < ahead_end: hashed above
                const size_t row = size_t(h >> 8);
                const uint32_t head = (heads[row] - 1u) & mask;
                heads[row] = uint8_t(head);
                tags[(row << row_log) + head] = uint8_t(h);
                pos[(row << row_log) + head] = idx;
            }
            _hc_begin = std::max(hb, he > 16 ? he - 16 : 0u);
            _hc_end = he;
        }

        // The entries of a row whose tag is `tag`, one bit each, bit i for
        // the i-th newest
        SGCL_INLINE_HOT uint64_t _row_matches(const uint8_t* tags, uint8_t tag, unsigned head) const noexcept {
            const uint64_t pattern = 0x0101010101010101ull * tag;
            const unsigned words = 1u << (_row_log - 3);
            uint64_t mask = 0;
            for (unsigned w = 0; w < words; ++w) {
                const uint64_t x = zstd_read64(tags + 8 * w) ^ pattern;
                const uint64_t zero = ~(((x & 0x7F7F7F7F7F7F7F7Full) + 0x7F7F7F7F7F7F7F7Full) | x) & 0x8080808080808080ull;
                mask |= (((zero >> 7) * 0x0102040810204080ull) >> 56) << (8 * w);
            }
            const unsigned r = 1u << _row_log;
            if (r == 64) {
                return std::rotr(mask, int(head));
            }
            return ((mask >> head) | (mask << (r - head))) & ((uint64_t(1) << r) - 1);
        }

        // The longest match at ip from its row, 2^search_log tries at most
        Found _row_search(const uint8_t* ip, const uint8_t* iend) noexcept {
            _row_insert_to(ip);
            Found best;
            const uint32_t here = uint32_t(ip - _base);
            const uint32_t h = here >= _hc_begin && here < _hc_end ? _hc[here & 15] : _row_hash(ip);
            const size_t row = size_t(h >> 8);
            const uint8_t* const tags = _row_tag.get() + (row << _row_log);
            const uint32_t* const pos = _row_pos.get() + (row << _row_log);
            const unsigned head = _row_head[row];
            const uint32_t rmask = (uint32_t(1) << _row_log) - 1;
            uint64_t found = _row_matches(tags, uint8_t(h), head);
            // the candidates first, each one's bytes fetched into the cache,
            // then compared: the fetches overlap
            uint32_t candidates[64];
            unsigned count = 0;
            for (unsigned attempts = _row_attempts; found && attempts; --attempts) {
                const unsigned i = (unsigned(std::countr_zero(found)) + head) & rmask;
                found &= found - 1;
                const uint32_t candidate = pos[i];
                if (_valid(candidate, here)) {
                    __builtin_prefetch(_base + candidate);
                    candidates[count++] = candidate;
                }
            }
            const size_t room = size_t(iend - ip);
            for (unsigned c = 0; c < count; ++c) {
                const uint32_t candidate = candidates[c];
                const uint8_t* m = _base + candidate;
                if (best.length < room && m[best.length] == ip[best.length] && lz4_read32(m) == lz4_read32(ip)) {
                    const size_t len = 4 + lz4_count(ip + 4, m + 4, iend);
                    if (len > best.length) {
                        best = {len, here - candidate};
                        if (len == room) {
                            break;
                        }
                    }
                }
            }
            return best;
        }

        // The longest match at ip through the binary tree, 2^search_log
        // nodes deep (ip goes into the tree)
        Found _tree_search(const uint8_t* ip, const uint8_t* iend) noexcept {
            ZstdTreeMatch found[ZstdTreeMatchesMax];
            const size_t n = _bt->find(_base, _low, ip, iend, found, 4);
            if (!n) {
                return {};
            }
            return {found[n - 1].length, found[n - 1].offset};
        }

        // The longest match at ip: the last three offsets and the chain; a
        // repeat costs no offset's bits, which its gain counts (four a byte,
        // less the offset's bits)
        Found _best(const uint8_t* ip, const uint8_t* iend, const uint8_t* anchor, const uint32_t (&rep)[3]) noexcept {
            Found best = _rows_size ? _row_search(ip, iend) : _tree_search(ip, iend);
            best.bits = zstd_offset_bits(best.offset + 2);   // the value: the distance + 3
            int best_gain = best.length >= 4 ? best.gain() : -1;
            const uint32_t here = uint32_t(ip - _base);
            for (int i = 0; i < 3; ++i) {
                const uint32_t r = i == 0 && ip == anchor ? rep[0] - 1 : rep[i];   // with no literals the first is "rep - 1"
                if (r && r <= here - _low && lz4_read32(ip - r) == lz4_read32(ip)) {
                    const size_t len = 4 + lz4_count(ip + 4, ip + 4 - r, iend);
                    const int gain = int(len) * 4 + 1;
                    if (gain > best_gain) {
                        best = {len, r, 0};
                        best_gain = gain;
                    }
                }
            }
            return best;
        }

        // greedy, lazy, lazy2 (and the tree's levels, until zstd_opt.h)
        void _lazy(const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& sink) noexcept {
            ZstdSink out = sink;
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            if (n < 16) {
                out.rest(anchor, iend);
                sink = out;
                return;
            }
            const uint8_t* const ilimit = iend - 8;
            const int depth = _p.strategy == ZstdStrategy::greedy ? 0 : _p.strategy == ZstdStrategy::lazy ? 1 : 2;
            while (ip < ilimit) {
                // greedy: the last offset again a byte on, taken at once
                // with no search — one literal and a repeat, the commonest
                // sequence of structured data (records of one shape whose
                // fields differ in a byte); the search would find a longer
                // match farther back and lose the repeat, which costs no
                // offset (the lazy levels weigh it in _best(ip + 1))
                if (depth == 0) {
                    const uint32_t r = rep[0];
                    const uint32_t at = uint32_t(ip + 1 - _base);
                    if (r && r <= at - _low && lz4_read32(ip + 1 - r) == lz4_read32(ip + 1)) {
                        const size_t len = 4 + lz4_count(ip + 5, ip + 5 - r, iend);
                        out.match(anchor, ip + 1, zstd_offset_value(rep, r, uint32_t(ip + 1 - anchor)), len);
                        ip += 1 + len;
                        anchor = ip;
                        continue;
                    }
                }
                Found m = _best(ip, iend, anchor, rep);
                if (m.length < 4) {
                    ip += (size_t(ip - anchor) >> 8) + 1;
                    continue;
                }
                // a better match a byte or two on
                for (int d = 1; d <= depth && ip + 1 < ilimit;) {
                    Found next = _best(ip + 1, iend, anchor, rep);
                    const int gain_here = m.gain() + 4;
                    const int gain_next = next.gain();
                    if (next.length >= 4 && gain_next > gain_here) {
                        ++ip;
                        m = next;
                        d = 1;
                        continue;
                    }
                    ++d;
                    if (d > depth) {
                        break;
                    }
                    if (ip + 2 >= ilimit) {
                        break;
                    }
                    Found far = _best(ip + 2, iend, anchor, rep);
                    if (far.length >= 4 && far.gain() > m.gain() + 7) {
                        ip += 2;
                        m = far;
                        d = 1;
                        continue;
                    }
                    break;
                }
                // back over the literals
                const uint8_t* start = ip;
                const uint8_t* mp = ip - m.offset;
                while (start > anchor && mp > _base + _low && start[-1] == mp[-1]) {
                    --start;
                    --mp;
                    ++m.length;
                }
                out.match(anchor, start, zstd_offset_value(rep, m.offset, uint32_t(start - anchor)), m.length);
                ip = start + m.length;
                anchor = ip;
            }
            out.rest(anchor, iend);
            sink = out;
        }

        ZstdParams _p {};
        std::unique_ptr<ZstdOptimal> _opt;
        std::unique_ptr<ZstdTree> _bt;
        std::unique_ptr<uint32_t[]> _hash;
        size_t _hash_size = 0;
        std::unique_ptr<uint32_t[]> _chain;
        size_t _chain_size = 0;
        std::unique_ptr<uint32_t[]> _row_pos;
        std::unique_ptr<uint8_t[]> _row_tag;
        std::unique_ptr<uint8_t[]> _row_head;
        size_t _rows_size = 0;
        unsigned _row_log = 0;
        unsigned _row_keep = 32;   // 64 less the bits of min_match bytes
        unsigned _row_drop = 56;   // 64 less the bits of a row's index and tag
        unsigned _row_attempts = 0;
        uint32_t _ahead_end = 0;   // positions before it may be hashed (8 bytes read)
        uint32_t _hc[16];
        uint32_t _hc_begin = 0;
        uint32_t _hc_end = 0;
        const uint8_t* _base = nullptr;
        uint32_t _low = 0;
        uint32_t _next = 0;
    };
}
