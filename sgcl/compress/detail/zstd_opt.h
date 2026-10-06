//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lz4_block.h"
#include "zstd_block_encode.h"
#include "zstd_params.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <vector>

// Zstandard's strongest levels: a binary tree of the window's suffixes and an
// optimal parser over it.
//
// The tree: every position goes in at its hash's bucket and is sorted
// against the positions there by their bytes, each node's two children the
// earlier positions whose suffixes sort below and above it; the walk down
// keeps the longest prefix shared on each side, so the matches it meets
// come with ever longer lengths and none is compared from its first byte.
// A position is a node until the ring of 2^chain_log nodes comes round.
//
// The parser: over a stretch of up to 4096 positions, the cheapest way to
// reach each one, by a literal from the one before or by a match from an
// earlier one — every length up to each match the tree gives there, and the
// three repeated offsets (each path keeps its own, as the decoder will move
// them); the price of a literal, a literal length, a match length and an
// offset in 1/256 bits from the frequencies of the blocks before (the first
// block's literals from its own bytes, the rest from a flat start; btultra2
// parses its first block twice to start from real ones). A match of
// target_length bytes or more is taken at once. The path is read back from
// the stretch's end into sequences.
namespace sgcl::compress::detail {
    // log2 in 1/256 bits, near enough (the fraction linear)
    SGCL_INLINE_HOT uint32_t zstd_log2_256(uint32_t v) noexcept {
        const unsigned hb = 31 - unsigned(std::countl_zero(v | 1));
        const uint32_t frac = hb >= 8 ? (v >> (hb - 8)) & 0xFF : (v << (8 - hb)) & 0xFF;
        return hb * 256 + frac;
    }

    // The frequencies the prices come from
    struct ZstdOptStats {
        uint32_t lit[256];
        uint32_t ll[36];
        uint32_t ml[53];
        uint32_t of[32];
        uint32_t lit_sum = 0, ll_sum = 0, ml_sum = 0, of_sum = 0;
        uint32_t lit_price[256];
        uint32_t ll_price[36];
        uint32_t ml_price[53];
        uint32_t of_price[32];
        bool started = false;

        void prices() noexcept {
            const uint32_t ls = zstd_log2_256(lit_sum);
            for (int i = 0; i < 256; ++i) {
                lit_price[i] = ls - zstd_log2_256(lit[i]);
            }
            const uint32_t lls = zstd_log2_256(ll_sum);
            for (int i = 0; i < 36; ++i) {
                ll_price[i] = lls - zstd_log2_256(ll[i]) + ZstdLiteralBits[i] * 256;
            }
            const uint32_t mls = zstd_log2_256(ml_sum);
            for (int i = 0; i < 53; ++i) {
                ml_price[i] = mls - zstd_log2_256(ml[i]) + ZstdMatchBits[i] * 256;
            }
            const uint32_t ofs = zstd_log2_256(of_sum);
            for (int i = 0; i < 32; ++i) {
                of_price[i] = ofs - zstd_log2_256(of[i]) + uint32_t(i) * 256;
            }
        }

        // The start of a frame's first block: literals from its own bytes,
        // the codes from a flat start leaning to short lengths and offsets
        void start(const uint8_t* p, size_t n) noexcept {
            std::fill(std::begin(lit), std::end(lit), 1u);
            for (size_t i = 0; i < n; ++i) {
                ++lit[p[i]];
            }
            for (auto& v : lit) {
                v = 1 + (v >> 4);
            }
            for (int i = 0; i < 36; ++i) {
                ll[i] = i < 16 ? 4 : 1;
            }
            for (int i = 0; i < 53; ++i) {
                ml[i] = i < 32 ? 3 : 1;
            }
            for (int i = 0; i < 32; ++i) {
                of[i] = i < 24 ? 2 : 1;
            }
            _sums();
            prices();
            started = true;
        }

        // After a block: the old counts halved, the block's added
        void learn(const ZstdSequence* seqs, size_t count, const uint8_t* lits, size_t lit_count) noexcept {
            for (auto& v : lit) {
                v = 1 + (v >> 2);
            }
            for (auto& v : ll) {
                v = 1 + (v >> 2);
            }
            for (auto& v : ml) {
                v = 1 + (v >> 2);
            }
            for (auto& v : of) {
                v = 1 + (v >> 2);
            }
            for (size_t i = 0; i < lit_count; ++i) {
                lit[lits[i]] += 2;
            }
            for (size_t i = 0; i < count; ++i) {
                ll[zstd_ll_code(seqs[i].literals)] += 2;
                ml[zstd_ml_code(seqs[i].length - 3)] += 2;
                of[31 - std::countl_zero(seqs[i].value)] += 2;
            }
            _sums();
            prices();
        }

        // One sequence counted as it is chosen, the prices made again by the
        // caller now and then
        SGCL_INLINE_HOT void count(const uint8_t* lits, uint32_t ll, uint32_t value, uint32_t length) noexcept {
            for (uint32_t i = 0; i < ll; ++i) {
                lit[lits[i]] += 2;
            }
            lit_sum += 2 * ll;
            ll_sum += 1;
            ++this->ll[zstd_ll_code(ll)];
            ++ml[zstd_ml_code(length - 3)];
            ++ml_sum;
            ++of[31 - std::countl_zero(value)];
            ++of_sum;
        }

        // The price of a literal run's bytes and of its length's code
        SGCL_INLINE_HOT uint32_t literal(uint8_t b) const noexcept {
            return lit_price[b];
        }

        SGCL_INLINE_HOT uint32_t literal_length(uint32_t ll) const noexcept {
            return ll_price[zstd_ll_code(ll)];
        }

        // The price of a match: its offset value's code and bits, its length's
        // (two bits more for every sequence: the prices from the counts leave
        // out what a sequence costs past its codes — the tables, the
        // rounding of FSE's states — and a path of fewer sequences, which
        // also decodes faster, measured smaller on text)
        SGCL_INLINE_HOT uint32_t match(uint32_t value, uint32_t length) const noexcept {
            return of_price[31 - std::countl_zero(value)] + ml_price[zstd_ml_code(length - 3)] + 512;
        }

    private:
        void _sums() noexcept {
            lit_sum = ll_sum = ml_sum = of_sum = 0;
            for (auto v : lit) {
                lit_sum += v;
            }
            for (auto v : ll) {
                ll_sum += v;
            }
            for (auto v : ml) {
                ml_sum += v;
            }
            for (auto v : of) {
                of_sum += v;
            }
        }
    };

    struct ZstdTreeMatch {
        uint32_t length;
        uint32_t offset;
    };

    // The most matches a search gives: one a node met, 2^9 nodes at most
    // (level 22's search_log), the rest not kept
    inline constexpr size_t ZstdTreeMatchesMax = 512;

    // The binary tree over the window
    class ZstdTree {
    public:
        void configure(unsigned hash_log, unsigned chain_log, unsigned search_log, unsigned min_match) noexcept {
            _hash_log = hash_log;
            _chain_log = chain_log;
            _search = 1u << search_log;
            _mls = std::clamp(min_match, 3u, 6u);
            const size_t hs = size_t(1) << hash_log;
            const size_t cs = size_t(2) << chain_log;
            if (_hs != hs) {
                _head.reset(new uint32_t[hs]);
                _hs = hs;
            }
            if (_cs != cs) {
                _tree.reset(new uint32_t[cs]);
                _cs = cs;
            }
            reset();
        }

        void reset() noexcept {
            std::fill(_head.get(), _head.get() + _hs, 0u);
            std::fill(_tree.get(), _tree.get() + _cs, 0u);
            _next = 0;
        }

        void shift(uint32_t delta) noexcept {
            for (size_t i = 0; i < _hs; ++i) {
                _head[i] = _head[i] > delta ? _head[i] - delta : 0;
            }
            for (size_t i = 0; i < _cs; ++i) {
                _tree[i] = _tree[i] > delta ? _tree[i] - delta : 0;
            }
            _next = _next > delta ? _next - delta : 0;
        }

        // Every position before ip into the tree (no matches kept)
        void update(const uint8_t* base, uint32_t low, const uint8_t* ip, const uint8_t* iend) noexcept {
            const uint32_t target = uint32_t(ip - base);
            if (_next < low) {
                _next = low;
            }
            while (_next < target) {
                const uint32_t skip = _insert(base, low, base + _next, iend, nullptr, 0);
                _next += std::max<uint32_t>(skip, 1);
            }
        }

        // The matches at ip of `least` bytes and more (every one longer than
        // the one before), ip put into the tree; their count
        size_t find(const uint8_t* base, uint32_t low, const uint8_t* ip, const uint8_t* iend, ZstdTreeMatch* out, uint32_t least) noexcept {
            update(base, low, ip, iend);
            size_t n = 0;
            if (_next == uint32_t(ip - base)) {
                const uint32_t skip = _insert(base, low, ip, iend, out, least, &n);
                _next += std::max<uint32_t>(skip, 1);
            }
            return n;
        }

    private:
        SGCL_INLINE_HOT uint32_t _hash(const uint8_t* p) const noexcept {
            const uint64_t v = zstd_read64(p);
            switch (_mls) {
                case 3: return uint32_t(((uint32_t(v) << 8) * 506832829u) >> (32 - _hash_log));
                case 4: return uint32_t((uint32_t(v) * 2654435761u) >> (32 - _hash_log));
                case 5: return uint32_t(((v << 24) * 889523592379ull) >> (64 - _hash_log));
                default: return uint32_t(((v << 16) * 227718039650203ull) >> (64 - _hash_log));
            }
        }

        // ip into the tree, the matches met kept in out when given; how many
        // positions after it need no node of their own (inside a long match)
        uint32_t _insert(const uint8_t* base, uint32_t low, const uint8_t* ip, const uint8_t* iend, ZstdTreeMatch* out, uint32_t least, size_t* found = nullptr) noexcept {
            const uint32_t here = uint32_t(ip - base);
            const uint32_t mask = (uint32_t(1) << _chain_log) - 1;
            const uint32_t floor = std::max(low, here > mask ? here - mask : 0u);
            const uint32_t h = _hash(ip);
            uint32_t candidate = _head[h];
            _head[h] = here;
            uint32_t* smaller = &_tree[2 * (here & mask)];
            uint32_t* larger = smaller + 1;
            size_t common_smaller = 0, common_larger = 0;
            unsigned compares = _search;
            size_t best = least ? least - 1 : 0;
            uint32_t match_end = here + 9;
            uint32_t dummy = 0;
            const size_t room = size_t(iend - ip);
            while (compares-- && candidate >= floor && candidate < here && candidate != 0) {
                uint32_t* node = &_tree[2 * (candidate & mask)];
                size_t len = std::min(common_smaller, common_larger);
                const uint8_t* m = base + candidate;
                len += lz4_count(ip + len, m + len, iend);
                if (len > best) {
                    best = len;
                    if (out && len >= 3 && *found < ZstdTreeMatchesMax) {
                        out[(*found)++] = {uint32_t(len), here - candidate};
                    }
                    if (candidate + len > match_end) {
                        match_end = candidate + uint32_t(len);
                    }
                }
                if (len >= room) {
                    // as far as the input goes: the node is left out, its children taken over
                    *smaller = node[1];
                    *larger = node[0];
                    break;
                }
                if (m[len] < ip[len]) {
                    *smaller = candidate;
                    common_smaller = len;
                    if (candidate <= floor) {
                        smaller = &dummy;
                        break;
                    }
                    smaller = node + 1;
                    candidate = node[1];
                } else {
                    *larger = candidate;
                    common_larger = len;
                    if (candidate <= floor) {
                        larger = &dummy;
                        break;
                    }
                    larger = node;
                    candidate = node[0];
                }
            }
            *smaller = 0;
            *larger = 0;
            // a match far past this position: the positions inside it are covered
            return match_end > here + 384 ? std::min<uint32_t>(match_end - here - 384, 192) : 1;
        }

        unsigned _hash_log = 0, _chain_log = 0, _search = 0, _mls = 4;
        std::unique_ptr<uint32_t[]> _head, _tree;
        size_t _hs = 0, _cs = 0;
        uint32_t _next = 0;
    };

    // The optimal parse of a block
    class ZstdOptimal {
    public:
        static constexpr size_t Num = 4096;

        void configure(const ZstdParams& p) noexcept {
            _p = p;
            _tree.configure(p.hash_log, p.chain_log, p.search_log, p.min_match);
            if (!_nodes) {
                _nodes.reset(new Node[Num + 2]);
            }
            reset();
        }

        void reset() noexcept {
            _tree.reset();
            _stats.started = false;
        }

        void shift(uint32_t delta) noexcept {
            _tree.shift(delta);
        }

        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            if (to - from >= 8) {
                _tree.update(base, uint32_t(from - base), to - 8, to);
            }
        }

        // The sequences of [src, src + n) and its literals into out; rep moved on
        void parse(const uint8_t* base, uint32_t low, const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& out) noexcept {
            if (!_stats.started) {
                _stats.start(src, n);
                if (_p.strategy == ZstdStrategy::btultra2 && n > 1024) {
                    // a first pass to learn from, its output dropped (the
                    // tree it fills is made again)
                    std::unique_ptr<ZstdSequence[]> first_seqs(new ZstdSequence[n / 3 + 1]);
                    std::unique_ptr<uint8_t[]> first_lits(new uint8_t[n + ZstdOutSlack]);
                    ZstdSink first {first_seqs.get(), first_lits.get()};
                    uint32_t first_rep[3] = {rep[0], rep[1], rep[2]};
                    _parse(base, low, src, n, first_rep, first);
                    _stats.learn(first_seqs.get(), size_t(first.seq - first_seqs.get()), first_lits.get(), size_t(first.lit - first_lits.get()));
                    _tree.reset();
                }
            }
            const ZstdSink before = out;
            _parse(base, low, src, n, rep, out);
            _stats.learn(before.seq, size_t(out.seq - before.seq), before.lit, size_t(out.lit - before.lit));
        }

    private:
        struct Node {
            uint32_t price;
            uint32_t length;    // of the match ending here; 0 a literal
            uint32_t offset;
            uint32_t literals;  // the literals since the last match
            uint32_t rep[3];
        };

        void _parse(const uint8_t* base, uint32_t low, const uint8_t* src, size_t n, uint32_t (&rep)[3], ZstdSink& out) noexcept {
            const uint8_t* ip = src;
            const uint8_t* anchor = src;
            const uint8_t* const iend = src + n;
            auto emit = [&](const uint8_t* start, uint32_t offset, uint32_t length) noexcept {
                const uint32_t ll = uint32_t(start - anchor);
                const uint32_t value = zstd_offset_value(rep, offset, ll);
                out.match(anchor, start, value, length);
                _stats.count(anchor, ll, value, length);
                anchor = start + length;
            };
            if (n < 16) {
                out.rest(src, iend);
                return;
            }
            const uint8_t* const ilimit = iend - 8;
            const uint32_t sufficient = std::max<uint32_t>(_p.target_length, 16);
            const uint32_t least = std::max<uint32_t>(_p.min_match, 3);
            Node* const opt = _nodes.get();
            ZstdTreeMatch matches[ZstdTreeMatchesMax];
            ZstdOptStats& st = _stats;
            while (ip < ilimit) {
                // the stretch from ip
                const uint32_t lead = uint32_t(ip - anchor);
                opt[0] = {0, 0, 0, lead, {rep[0], rep[1], rep[2]}};
                size_t last = 0;
                size_t forced_at = SIZE_MAX;
                ZstdTreeMatch forced {0, 0};
                auto price_from = [&](size_t at, uint32_t offset, uint32_t value, uint32_t from_len, uint32_t to_len) noexcept {
                    const Node& f = opt[at];
                    const uint32_t base_price = f.price + st.literal_length(f.literals);
                    if (at + to_len > last) {
                        for (size_t k = last + 1; k <= at + to_len; ++k) {
                            opt[k].price = UINT32_MAX;
                        }
                        last = at + to_len;
                    }
                    for (uint32_t len = from_len; len <= to_len; ++len) {
                        const uint32_t price = base_price + st.match(value, len);
                        Node& t = opt[at + len];
                        if (price < t.price) {
                            t.price = price;
                            t.length = len;
                            t.offset = offset;
                            t.literals = 0;
                            t.rep[0] = f.rep[0];
                            t.rep[1] = f.rep[1];
                            t.rep[2] = f.rep[2];
                            zstd_update_rep(t.rep, offset, f.literals);
                        }
                    }
                };
                // the matches at a position of the stretch, priced
                auto search = [&](size_t at) noexcept -> bool {
                    const uint8_t* p = ip + at;
                    const Node& f = opt[at];
                    const uint32_t here = uint32_t(p - base);
                    uint32_t longest = least - 1;
                    // the repeated offsets
                    for (int r = 0; r < 3; ++r) {
                        uint32_t o, value;
                        if (f.literals) {
                            o = f.rep[r];
                            value = uint32_t(r) + 1;
                        } else {
                            o = r == 2 ? f.rep[0] - 1 : f.rep[r + 1];
                            value = uint32_t(r) + 1;
                        }
                        if (!o || o > here - low || lz4_read32(p) != lz4_read32(p - o)) {
                            continue;
                        }
                        const uint32_t len = uint32_t(4 + lz4_count(p + 4, p + 4 - o, iend));
                        if (len > longest) {
                            if (len >= sufficient || at + len >= Num) {
                                forced = {len, o};   // long enough to take at once: the longest such, below
                            } else {
                                price_from(at, o, value, least, len);
                            }
                            longest = len;
                        }
                    }
                    // the tree's matches, longer than the repeats' (each one
                    // longer than the one before)
                    const size_t count = _tree.find(base, low, p, iend, matches, longest + 1);
                    for (size_t i = 0; i < count; ++i) {
                        const uint32_t len = std::min<uint32_t>(matches[i].length, uint32_t(iend - p));
                        if (len <= longest) {
                            continue;
                        }
                        if (len >= sufficient || at + len >= Num) {
                            forced = {len, matches[i].offset};
                        } else {
                            price_from(at, matches[i].offset, matches[i].offset + 3, std::max(longest + 1, least), len);
                        }
                        longest = len;
                    }
                    if (forced.length && forced_at == SIZE_MAX) {
                        forced_at = at;
                        return true;
                    }
                    return false;
                };
                bool stop = search(0);
                size_t end = 0;
                if (!stop) {
                    if (last == 0) {
                        // nothing here: a literal, and on
                        ++ip;
                        continue;
                    }
                    for (size_t cur = 1;; ++cur) {
                        if (cur > last) {
                            end = last;
                            break;
                        }
                        // a literal from cur - 1
                        const Node& prev = opt[cur - 1];
                        const uint32_t lits_here = prev.literals + 1;
                        const uint32_t lp = prev.price + st.literal(ip[cur - 1]) + st.literal_length(lits_here) - st.literal_length(prev.literals);
                        if (lp < opt[cur].price) {
                            opt[cur] = {lp, 0, 0, lits_here, {prev.rep[0], prev.rep[1], prev.rep[2]}};
                        }
                        if (ip + cur >= ilimit) {
                            continue;
                        }
                        // btopt: no search where the next position is no dearer
                        if (_p.strategy == ZstdStrategy::btopt && cur + 1 <= last && opt[cur + 1].price <= opt[cur].price + 128) {
                            continue;
                        }
                        if (search(cur)) {
                            end = forced_at;
                            stop = true;
                            break;
                        }
                    }
                } else {
                    end = 0;
                }
                // the path back from end, then out in order
                size_t steps[Num + 1];
                size_t count = 0;
                for (size_t at = end; at > 0;) {
                    if (opt[at].length) {
                        steps[count++] = at;
                        at -= opt[at].length;
                    } else {
                        --at;
                    }
                }
                while (count) {
                    const size_t at = steps[--count];
                    emit(ip + at - opt[at].length, opt[at].offset, opt[at].length);
                }
                if (stop && forced.length) {
                    emit(ip + forced_at, forced.offset, forced.length);
                    ip = anchor;
                } else {
                    ip += std::max<size_t>(end, 1);
                }
                _stats.prices();   // what this stretch chose counts for the next
            }
            out.rest(anchor, iend);
        }

        ZstdParams _p {};
        ZstdTree _tree;
        ZstdOptStats _stats;
        std::unique_ptr<Node[]> _nodes;
    };
}
