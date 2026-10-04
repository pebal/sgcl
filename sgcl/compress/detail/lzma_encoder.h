//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lzma_decoder.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace sgcl::compress::detail {
    // Where LzmaEncoder::run stopped: everything it could code done (all
    // of the data when finishing; else what the lookahead allows), the
    // budget spent, or an LZMA2 chunk full
    enum class LzmaRun : uint8_t {
        done,
        budget,
        chunk
    };

    // How the encoder works at a level: the dictionary, the parser (the
    // optimal one, pricing every way to code the next few thousand bytes,
    // or a greedy one that looks one position ahead), the match finder (a
    // binary tree or a hash chain under a hash of four bytes), the length
    // at which a match is taken without looking further (nice) and how many
    // candidates a search visits (depth). Levels 0..9 follow xz's -0..-9 in
    // the dictionary and the mode; extreme is xz's -e.
    struct LzmaEncoderSettings {
        LzmaProperties props;
        bool optimal = true;
        bool tree = true;
        uint32_t nice = 64;
        uint32_t depth = 48;

        static LzmaEncoderSettings of(int level, bool extreme) {
            static constexpr uint8_t dict_bits[] = {18, 20, 21, 22, 22, 23, 23, 24, 25, 26};
            static constexpr uint8_t fast_depth[] = {4, 8, 24, 48};
            if (level < 0 || level > 9) {
                throw std::invalid_argument("compress::lzma: level 0..9");
            }
            LzmaEncoderSettings s;
            s.props.dictionary = uint32_t(1) << dict_bits[level];
            if (level <= 3) {
                s.optimal = false;
                s.tree = false;
                s.nice = level <= 1 ? 128 : 273;
                s.depth = fast_depth[level];
            } else {
                s.optimal = true;
                s.tree = true;
                s.nice = level == 4 ? 16 : level == 5 ? 32 : 64;
                s.depth = 16 + s.nice / 2;
            }
            if (extreme) {
                s.optimal = true;
                s.tree = true;
                if (level == 3 || level == 5) {
                    s.nice = 192;
                    s.depth = 16 + s.nice / 2;
                } else {
                    s.nice = 273;
                    s.depth = 512;
                }
            }
            return s;
        }
    };

    // The finder of repeats: every position of the data goes in, in order,
    // either searched (find: the matches there, each longer than the one
    // before, at the nearest distance found for its length) or only
    // entered (skip). Three tables under hashes of the next two, three and
    // four bytes give the last position of each; under the four-byte
    // hash, the positions of the dictionary form either a chain, newest
    // first (the fast levels), or a binary tree ordered by the bytes that
    // follow them, rebuilt at every insertion so that the search walks
    // towards the longest match (the other levels). Positions are 32-bit
    // numbers that start at the dictionary's size, so that an empty entry
    // (0) is always out of reach; near 2^32 every entry is moved down.
    class LzmaMatchFinder {
    public:
        struct Match {
            uint32_t len;
            uint32_t dist;   // the distance less one, as LZMA codes it
        };

        static constexpr uint32_t MaxMatches = lzma_model::MatchMax + 8;

        const uint8_t* data = nullptr;   // the window: the data at index read is the next position
        size_t read = 0;
        size_t end = 0;

        void init(uint32_t dictionary, uint32_t nice, uint32_t depth, bool tree) noexcept {
            _cyclic = dictionary + 1;
            _nice = nice;
            _depth = depth;
            _tree = tree;
            uint32_t bits = std::clamp<uint32_t>(uint32_t(std::bit_width(dictionary - 1)) - 1, 16, 24);
            _hash4_shift = 32 - bits;
            size_t hash_entries = Hash2Size + Hash3Size + (size_t(1) << bits);
            _hash.reset(new uint32_t[hash_entries]());
            _hash3 = _hash.get() + Hash2Size;
            _hash4 = _hash3 + Hash3Size;
            _son.reset(new uint32_t[size_t(_cyclic) * (tree ? 2 : 1)]);
            _hash_entries = hash_entries;
            _pos = _cyclic;
            _cyc = 0;
        }

        // A new stream: the tables emptied, their memory kept
        SGCL_INLINE_HOT void restart() noexcept {
            std::fill(_hash.get(), _hash.get() + _hash_entries, 0u);
            _pos = _cyclic;
            _cyc = 0;
            read = 0;
            end = 0;
        }

        // The matches at the current position, into m; the position moves on
        template<bool Tree>
        uint32_t find(Match* m) noexcept {
            size_t avail = end - read;
            if (avail < 4) {
                _move();
                return 0;
            }
            uint32_t limit = uint32_t(std::min<size_t>(avail, _nice));
            const uint8_t* cur = data + read;
            uint32_t v;
            std::memcpy(&v, cur, 4);
            uint32_t h2 = v & 0xFFFF;
            uint32_t h3 = ((v & 0xFFFFFF) * 0x9E3779B1u) >> (32 - 16);
            uint32_t h4 = (v * 0x9E3779B1u) >> _hash4_shift;
            uint32_t d2 = _pos - _hash[h2];
            uint32_t d3 = _pos - _hash3[h3];
            uint32_t next = _hash4[h4];
            _hash[h2] = _pos;
            _hash3[h3] = _pos;
            _hash4[h4] = _pos;
            uint32_t count = 0;
            uint32_t best = 1;
            uint32_t best_delta = 0;
            if (d2 < _cyclic && *(cur - d2) == cur[0]) {
                best = 2;
                best_delta = d2;
                m[count++] = {2, d2 - 1};
            }
            if (d3 != d2 && d3 < _cyclic && *(cur - d3) == cur[0] && cur[1 - ptrdiff_t(d3)] == cur[1] && cur[2 - ptrdiff_t(d3)] == cur[2]) {
                best = 3;
                best_delta = d3;
                m[count++] = {3, d3 - 1};
            }
            if (count) {
                best = _extend(cur, cur - best_delta, best, limit);
                m[count - 1].len = best;
                if (best == limit) {
                    // as long as it may be: the tree is only put in order
                    if constexpr (Tree) {
                        _tree_insert(cur, next, limit);
                    } else {
                        _son[_cyc] = next;
                    }
                    _finish_long(m, count, cur, avail);
                    _move();
                    return count;
                }
            }
            if constexpr (Tree) {
                count = _tree_find(cur, next, limit, best, m, count);
            } else {
                count = _chain_find(cur, next, limit, best, m, count);
            }
            _finish_long(m, count, cur, avail);
            _move();
            return count;
        }

        // n positions entered without a search
        template<bool Tree>
        void skip(uint32_t n) noexcept {
            while (n--) {
                size_t avail = end - read;
                if (avail < 4) {
                    _move();
                    continue;
                }
                uint32_t limit = uint32_t(std::min<size_t>(avail, _nice));
                const uint8_t* cur = data + read;
                uint32_t v;
                std::memcpy(&v, cur, 4);
                uint32_t h2 = v & 0xFFFF;
                uint32_t h3 = ((v & 0xFFFFFF) * 0x9E3779B1u) >> (32 - 16);
                uint32_t h4 = (v * 0x9E3779B1u) >> _hash4_shift;
                uint32_t next = _hash4[h4];
                _hash[h2] = _pos;
                _hash3[h3] = _pos;
                _hash4[h4] = _pos;
                if constexpr (Tree) {
                    _tree_insert(cur, next, limit);
                } else {
                    _son[_cyc] = next;
                }
                _move();
            }
        }

        // The window moved down by n bytes: the positions stay, their bytes moved
        SGCL_INLINE_HOT void shifted(size_t n) noexcept {
            read -= n;
            end -= n;
        }

    private:
        static constexpr uint32_t Hash2Size = uint32_t(1) << 16;
        static constexpr uint32_t Hash3Size = uint32_t(1) << 16;

        // The bytes of a and b equal from len on, up to limit
        static SGCL_LZMA_INLINE uint32_t _extend(const uint8_t* a, const uint8_t* b, uint32_t len, uint32_t limit) noexcept {
            while (len + 8 <= limit) {
                uint64_t x, y;
                std::memcpy(&x, a + len, 8);
                std::memcpy(&y, b + len, 8);
                if (uint64_t d = x ^ y) {
                    return len + uint32_t(std::countr_zero(d) >> 3);
                }
                len += 8;
            }
            while (len < limit && a[len] == b[len]) {
                ++len;
            }
            return len;
        }

        // A longest match at the nice length goes on as far as it may
        SGCL_INLINE_HOT void _finish_long(Match* m, uint32_t count, const uint8_t* cur, size_t avail) const noexcept {
            if (count && m[count - 1].len == _nice && avail > _nice) {
                uint32_t limit = uint32_t(std::min<size_t>(avail, lzma_model::MatchMax));
                m[count - 1].len = _extend(cur, cur - m[count - 1].dist - 1, _nice, limit);
            }
        }

        SGCL_LZMA_INLINE void _move() noexcept {
            ++read;
            if (++_cyc == _cyclic) {
                _cyc = 0;
            }
            if (SGCL_LZMA_UNLIKELY(++_pos == UINT32_MAX)) {
                _normalize();
            }
        }

        // Every entry moved down so that the position is the dictionary's
        // size again; entries out of reach become 0
        void _normalize() noexcept {
            uint32_t sub = _pos - _cyclic;
            auto fix = [sub](uint32_t* p, size_t n) noexcept {
                for (size_t i = 0; i < n; ++i) {
                    p[i] = p[i] <= sub ? 0 : p[i] - sub;
                }
            };
            fix(_hash.get(), _hash_entries);
            fix(_son.get(), size_t(_cyclic) * (_tree ? 2 : 1));
            _pos -= sub;
        }

        SGCL_LZMA_INLINE uint32_t _slot(uint32_t delta) const noexcept {
            return _cyc - delta + (delta > _cyc ? _cyclic : 0);
        }

        uint32_t _chain_find(const uint8_t* cur, uint32_t next, uint32_t limit, uint32_t best, Match* m, uint32_t count) noexcept {
            _son[_cyc] = next;
            uint32_t depth = _depth;
            for (;;) {
                uint32_t delta = _pos - next;
                if (depth-- == 0 || delta >= _cyclic) {
                    return count;
                }
                const uint8_t* p = cur - delta;
                next = _son[_slot(delta)];
                if (p[best] == cur[best] && p[0] == cur[0]) {
                    uint32_t len = _extend(cur, p, 1, limit);
                    if (len > best) {
                        best = len;
                        m[count++] = {len, delta - 1};
                        if (len == limit) {
                            return count;
                        }
                    }
                }
            }
        }

        // The search down the tree: every node visited is put on the side
        // of the new position its bytes sort to, so that the new position
        // becomes the root with the old tree split under it
        uint32_t _tree_find(const uint8_t* cur, uint32_t next, uint32_t limit, uint32_t best, Match* m, uint32_t count) noexcept {
            uint32_t* left = _son.get() + size_t(_cyc) * 2 + 1;    // where the next node greater than cur goes
            uint32_t* right = _son.get() + size_t(_cyc) * 2;       // and the next node less
            uint32_t len_left = 0, len_right = 0;
            uint32_t depth = _depth;
            for (;;) {
                uint32_t delta = _pos - next;
                if (depth-- == 0 || delta >= _cyclic) {
                    *left = 0;
                    *right = 0;
                    return count;
                }
                uint32_t* pair = _son.get() + size_t(_slot(delta)) * 2;
                const uint8_t* p = cur - delta;
                uint32_t len = std::min(len_left, len_right);
                if (p[len] == cur[len]) {
                    len = _extend(cur, p, len + 1, limit);
                    if (len > best) {
                        best = len;
                        m[count++] = {len, delta - 1};
                        if (len == limit) {
                            *right = pair[0];
                            *left = pair[1];
                            return count;
                        }
                    }
                }
                if (p[len] < cur[len]) {
                    *right = next;
                    right = pair + 1;
                    next = *right;
                    len_right = len;
                } else {
                    *left = next;
                    left = pair;
                    next = *left;
                    len_left = len;
                }
            }
        }

        void _tree_insert(const uint8_t* cur, uint32_t next, uint32_t limit) noexcept {
            uint32_t* left = _son.get() + size_t(_cyc) * 2 + 1;
            uint32_t* right = _son.get() + size_t(_cyc) * 2;
            uint32_t len_left = 0, len_right = 0;
            uint32_t depth = _depth;
            for (;;) {
                uint32_t delta = _pos - next;
                if (depth-- == 0 || delta >= _cyclic) {
                    *left = 0;
                    *right = 0;
                    return;
                }
                uint32_t* pair = _son.get() + size_t(_slot(delta)) * 2;
                const uint8_t* p = cur - delta;
                uint32_t len = std::min(len_left, len_right);
                if (p[len] == cur[len]) {
                    len = _extend(cur, p, len + 1, limit);
                    if (len == limit) {
                        *right = pair[0];
                        *left = pair[1];
                        return;
                    }
                }
                if (p[len] < cur[len]) {
                    *right = next;
                    right = pair + 1;
                    next = *right;
                    len_right = len;
                } else {
                    *left = next;
                    left = pair;
                    next = *left;
                    len_left = len;
                }
            }
        }

        std::unique_ptr<uint32_t[]> _hash;   // the two-byte table, then the three- and the four-byte ones
        uint32_t* _hash3 = nullptr;
        uint32_t* _hash4 = nullptr;
        size_t _hash_entries = 0;
        std::unique_ptr<uint32_t[]> _son;    // per position of the dictionary: the chain's link, or the tree's two
        uint32_t _hash4_shift = 16;
        uint32_t _cyclic = 0;
        uint32_t _cyc = 0;
        uint32_t _pos = 0;
        uint32_t _nice = 64;
        uint32_t _depth = 48;
        bool _tree = true;
    };

    // The encoder of LZMA. Data comes into a window (the encoder's own,
    // when written as a stream; the caller's, when the whole of it is at
    // hand) and is coded as far as the parser can see: to the end when
    // finishing, otherwise while a whole lookahead of it is there.
    class LzmaEncoder {
    public:
        static constexpr uint32_t Opts = uint32_t(1) << 12;   // the positions the optimal parser prices at most
        static constexpr size_t Lookahead = Opts + lzma_model::MatchMax + 8;

        SGCL_INLINE_HOT explicit LzmaEncoder(const LzmaEncoderSettings& s) noexcept
        : _s(s)
        , _rc(_sink) {
            _dictionary = std::max(s.props.dictionary, lzma_model::DictionaryMin);
            _literal.reset(new uint16_t[s.props.literal_probs()]);
            _reset_model();
        }

        LzmaEncoder(const LzmaEncoder&) = delete;
        LzmaEncoder& operator=(const LzmaEncoder&) = delete;

        SGCL_INLINE_HOT uint32_t dictionary() const noexcept {
            return _dictionary;
        }

        // A new stream with the same settings: the window and the tables kept
        void restart() noexcept {
            _reset_model();
            _rc.reset();
            _total = 0;
            _at = 0;
            _have_look = false;
            _path_at = _path_len = 0;
            _prices_ready = false;
            _match_count = _align_count = 0;
            if (_ready) {
                _mf.restart();
            }
        }

        // The whole of the data, in place (the caller keeps it alive); the
        // dictionary no larger than the data needs
        void attach(const uint8_t* p, size_t n) noexcept {
            if (n < _dictionary) {
                _dictionary = std::max(_round_dictionary(uint32_t(n)), lzma_model::DictionaryMin);
            }
            _mf.init(_dictionary, _s.nice, _s.depth, _s.tree);
            _mf.data = p;
            _mf.read = 0;
            _mf.end = n;
            _ready = true;
        }

        // Bytes of a stream into the window: as many as there is room for
        // (0: run first, then write again)
        size_t append(const uint8_t* p, size_t n) noexcept {
            if (!_ready) {
                // LZMA2 may store a chunk as it is: its bytes stay in the window
                _keep = std::max<size_t>(_dictionary, _chunked ? ChunkUnpacked : 0) + 64;
                _capacity = _keep + std::max<size_t>(_dictionary / 4, size_t(1) << 20) + Lookahead;
                _window.reset(new uint8_t[_capacity]);
                _mf.init(_dictionary, _s.nice, _s.depth, _s.tree);
                _mf.data = _window.get();
                _ready = true;
            }
            if (_mf.end == _capacity && _at > _keep) {
                size_t shift = _at - _keep;
                sgcl::detail::move_bytes(_window.get(), _window.get() + shift, _mf.end - shift);
                _mf.shifted(shift);
                _at -= shift;
            }
            size_t k = std::min(n, _capacity - _mf.end);
            sgcl::detail::copy_bytes(_window.get() + _mf.end, p, k);
            _mf.end += k;
            return k;
        }

        // Codes what can be coded, into out: to the end of the data when
        // finishing. budget: the input bytes to go through at most (0: no
        // bound). In LZMA2's chunks, it stops as well where the chunk is full.
        SGCL_INLINE_HOT LzmaRun run(bool finish, std::vector<uint8_t>& out, uint64_t budget = 0) noexcept {
            _rc_out(out);
            if (!_ready) {
                return LzmaRun::done;
            }
            if (!_prices_ready && _s.optimal) {
                _update_prices();
                _prices_ready = true;
            }
            return _s.tree ? _run<true>(finish, budget) : _run<false>(finish, budget);
        }

        // --- LZMA2 (detail/lzma2.h): the data in chunks of at most 2 MiB,
        // each coded into at most 64 KiB with a range coder of its own

        static constexpr uint32_t ChunkUnpacked = uint32_t(1) << 21;
        static constexpr uint32_t ChunkPacked = uint32_t(1) << 16;

        // Before the first append
        SGCL_INLINE_HOT void chunked() noexcept {
            _chunked = true;
        }

        SGCL_INLINE_HOT void chunk_begin() noexcept {
            _rc.reset();
            _chunk_start = _total;
        }

        // The chunk's last bytes into out; the bytes it codes
        SGCL_INLINE_HOT uint32_t chunk_end(std::vector<uint8_t>& out) noexcept {
            _rc_out(out);
            _rc.finish();
            return uint32_t(_total - _chunk_start);
        }

        // The chunk's bytes as they are, for a chunk stored uncompressed
        SGCL_INLINE_HOT const uint8_t* chunk_data() const noexcept {
            return _mf.data + (_at - size_t(_total - _chunk_start));
        }

        // The probabilities, the state and the repeats as at the start
        // (after a chunk stored uncompressed, the next one resets them)
        SGCL_INLINE_HOT void reset_state() noexcept {
            _reset_model();
            _prices_ready = false;
            _match_count = _align_count = 0;
        }

        // The end: the marker when asked, and the coder's last bytes
        void finish(bool marker, std::vector<uint8_t>& out) noexcept {
            _rc_out(out);
            if (marker) {
                uint32_t ps = uint32_t(_total) & _pb_mask();
                _rc.bit(_probs.is_match[_state][ps], 1);
                _rc.bit(_probs.is_rep[_state], 0);
                _encode_len(_probs.len, lzma_model::MatchMin, ps, false);
                _encode_dist(lzma_model::EndMarker, lzma_model::MatchMin);
            }
            _rc.finish();
        }

    private:
        // A step of the parse, with its distance: coded as planned while the
        // repeats are those the parser saw, and by the distance otherwise
        // (after a state reset between LZMA2 chunks)
        struct Step {
            uint32_t len;
            uint32_t back;   // Literal, a repeat 0..3 (0 with len 1: the short repeat), a distance + 4
            uint32_t dist;
        };

        static constexpr uint32_t Literal = UINT32_MAX;

        struct Opt {
            uint32_t price;
            uint32_t prev;
            uint32_t back;
            uint32_t first;    // Chain: the length of the first step
            uint8_t chain;     // 0: one step; 1: a literal, then rep0; 2: a step of `first` bytes, a literal, then rep0
            uint8_t state;
            uint32_t reps[4];
        };

        SGCL_INLINE_HOT static uint32_t _round_dictionary(uint32_t n) noexcept {
            // the next 2^k or 3 * 2^(k-1), as decoders size their windows
            if (n <= lzma_model::DictionaryMin) {
                return lzma_model::DictionaryMin;
            }
            uint32_t p = std::bit_ceil(n);
            uint32_t three = p / 4 * 3;
            return three >= n ? three : p;
        }

        SGCL_INLINE_HOT size_t _rc_out_size() const noexcept {
            return _rc_sink->size();
        }

        SGCL_INLINE_HOT void _rc_out(std::vector<uint8_t>& out) noexcept {
            _rc_sink = &out;
            _rc.sink(out);
        }

        SGCL_INLINE_HOT uint32_t _pb_mask() const noexcept {
            return (1u << _s.props.pb) - 1;
        }

        SGCL_INLINE_HOT void _reset_model() noexcept {
            _probs.reset();
            std::fill(_literal.get(), _literal.get() + _s.props.literal_probs(), rc::ProbInit);
            _state = 0;
            _reps[0] = _reps[1] = _reps[2] = _reps[3] = 0;
        }

        // --- coding

        SGCL_INLINE_HOT uint16_t* _literal_probs(uint64_t at_total, uint32_t prev) noexcept {
            uint32_t ctx = ((uint32_t(at_total) & ((1u << _s.props.lp) - 1)) << _s.props.lc) + (prev >> (8 - _s.props.lc));
            return _literal.get() + size_t(0x300) * ctx;
        }

        void _encode_literal() noexcept {
            const uint8_t* cur = _mf.data + _at;
            uint32_t ps = uint32_t(_total) & _pb_mask();
            _rc.bit(_probs.is_match[_state][ps], 0);
            uint16_t* probs = _literal_probs(_total, _total ? cur[-1] : 0);
            uint32_t byte = cur[0];
            if (_state < lzma_model::LiteralStates) {
                _rc.tree(probs, 8, byte);
            } else {
                uint32_t match = cur[-ptrdiff_t(_reps[0]) - 1];
                uint32_t s = 1, offs = 0x100;
                for (int i = 7; i >= 0; --i) {
                    uint32_t bit = (byte >> i) & 1;
                    match <<= 1;
                    uint32_t mb = match & offs;
                    _rc.bit(probs[offs + mb + s], bit);
                    s = (s << 1) | bit;
                    offs &= bit ? mb : ~mb;
                }
            }
            _state = lzma_model::after_literal(_state);
        }

        void _encode_len(LzmaLengthProbs& p, uint32_t len, uint32_t ps, bool rep) noexcept {
            len -= lzma_model::MatchMin;
            if (len < 8) {
                _rc.bit(p.choice, 0);
                _rc.tree(p.low[ps], 3, len);
            } else if (len < 16) {
                _rc.bit(p.choice, 1);
                _rc.bit(p.choice2, 0);
                _rc.tree(p.mid[ps], 3, len - 8);
            } else {
                _rc.bit(p.choice, 1);
                _rc.bit(p.choice2, 1);
                _rc.tree(p.high, 8, len - 16);
            }
            if (_s.optimal && --(rep ? _rep_len_count : _len_count)[ps] == 0) {
                _update_len_prices(p, rep ? _rep_len_prices[ps] : _len_prices[ps], ps);
                (rep ? _rep_len_count : _len_count)[ps] = LenPriceUpdate;
            }
        }

        void _encode_dist(uint32_t dist, uint32_t len) noexcept {
            uint32_t ls = std::min<uint32_t>(len - lzma_model::MatchMin, lzma_model::LenStates - 1);
            uint32_t slot = lzma_model::dist_slot(dist);
            _rc.tree(_probs.dist_slot[ls], lzma_model::DistSlotBits, slot);
            if (slot >= lzma_model::StartPosModel) {
                uint32_t bits = (slot >> 1) - 1;
                uint32_t base = (2 | (slot & 1)) << bits;
                uint32_t reduced = dist - base;
                if (slot < lzma_model::EndPosModel) {
                    _rc.reverse_tree(_probs.dist_special + base - slot, bits, reduced);
                } else {
                    _rc.direct(reduced >> lzma_model::AlignBits, bits - lzma_model::AlignBits);
                    _rc.reverse_tree(_probs.align, lzma_model::AlignBits, reduced & (lzma_model::AlignSize - 1));
                    ++_align_count;
                }
            }
            ++_match_count;
        }

        void _encode_match(uint32_t dist, uint32_t len) noexcept {
            uint32_t ps = uint32_t(_total) & _pb_mask();
            _rc.bit(_probs.is_match[_state][ps], 1);
            _rc.bit(_probs.is_rep[_state], 0);
            _encode_len(_probs.len, len, ps, false);
            _encode_dist(dist, len);
            _reps[3] = _reps[2];
            _reps[2] = _reps[1];
            _reps[1] = _reps[0];
            _reps[0] = dist;
            _state = lzma_model::after_match(_state);
        }

        void _encode_rep(uint32_t index, uint32_t len) noexcept {
            uint32_t ps = uint32_t(_total) & _pb_mask();
            _rc.bit(_probs.is_match[_state][ps], 1);
            _rc.bit(_probs.is_rep[_state], 1);
            if (index == 0) {
                _rc.bit(_probs.is_rep_g0[_state], 0);
                _rc.bit(_probs.is_rep0_long[_state][ps], len == 1 ? 0 : 1);
            } else {
                _rc.bit(_probs.is_rep_g0[_state], 1);
                if (index == 1) {
                    _rc.bit(_probs.is_rep_g1[_state], 0);
                } else {
                    _rc.bit(_probs.is_rep_g1[_state], 1);
                    _rc.bit(_probs.is_rep_g2[_state], index - 2);
                }
            }
            if (len == 1) {
                _state = lzma_model::after_short_rep(_state);
                return;
            }
            _encode_len(_probs.rep_len, len, ps, true);
            uint32_t d = _reps[index];
            for (uint32_t i = index; i > 0; --i) {
                _reps[i] = _reps[i - 1];
            }
            _reps[0] = d;
            _state = lzma_model::after_rep(_state);
        }

        void _emit(const Step& s) noexcept {
            uint32_t len = s.len;
            if (s.back == Literal) {
                _encode_literal();
            } else if (s.back < 4 && _reps[s.back] == s.dist) {
                _encode_rep(s.back, len);
            } else if (len == 1) {
                // a short repeat whose distance is no longer rep0
                _encode_literal();
            } else if (s.back < 4) {
                uint32_t i = 0;
                while (i < 4 && _reps[i] != s.dist) {
                    ++i;
                }
                if (i < 4) {
                    _encode_rep(i, len);
                } else {
                    _encode_match(s.dist, len);
                }
            } else {
                _encode_match(s.dist, len);
            }
            _at += len;
            _total += len;
            if (_s.optimal) {
                if (_match_count >= MatchPriceUpdate) {
                    _update_dist_prices();
                }
                if (_align_count >= lzma_model::AlignSize) {
                    _update_align_prices();
                }
            }
        }

        // --- prices

        static constexpr uint32_t LenPrices = lzma_model::MatchMax - lzma_model::MatchMin + 1;
        static constexpr uint32_t LenPriceUpdate = 64;
        static constexpr uint32_t MatchPriceUpdate = 128;

        void _update_len_prices(const LzmaLengthProbs& p, uint32_t* out, uint32_t ps) noexcept {
            const uint32_t* t = _price;
            uint32_t a0 = rc::price0(t, p.choice);
            uint32_t a1 = rc::price1(t, p.choice);
            uint32_t b0 = a1 + rc::price0(t, p.choice2);
            uint32_t b1 = a1 + rc::price1(t, p.choice2);
            for (uint32_t i = 0; i < LenPrices; ++i) {
                if (i < 8) {
                    out[i] = a0 + rc::tree_price(t, p.low[ps], 3, i);
                } else if (i < 16) {
                    out[i] = b0 + rc::tree_price(t, p.mid[ps], 3, i - 8);
                } else {
                    out[i] = b1 + rc::tree_price(t, p.high, 8, i - 16);
                }
            }
        }

        void _update_dist_prices() noexcept {
            const uint32_t* t = _price;
            uint32_t slots = lzma_model::dist_slot(_dictionary - 1) + 1;
            for (uint32_t ls = 0; ls < lzma_model::LenStates; ++ls) {
                for (uint32_t slot = 0; slot < slots; ++slot) {
                    uint32_t price = rc::tree_price(t, _probs.dist_slot[ls], lzma_model::DistSlotBits, slot);
                    if (slot >= lzma_model::EndPosModel) {
                        price += (((slot >> 1) - 1) - lzma_model::AlignBits) << rc::PriceShift;
                    }
                    _slot_prices[ls][slot] = price;
                }
                for (uint32_t d = 0; d < lzma_model::StartPosModel; ++d) {
                    _dist_prices[ls][d] = _slot_prices[ls][d];
                }
            }
            for (uint32_t d = lzma_model::StartPosModel; d < lzma_model::FullDistances; ++d) {
                uint32_t slot = lzma_model::dist_slot(d);
                uint32_t bits = (slot >> 1) - 1;
                uint32_t base = (2 | (slot & 1)) << bits;
                uint32_t price = rc::reverse_tree_price(t, _probs.dist_special + base - slot, bits, d - base);
                for (uint32_t ls = 0; ls < lzma_model::LenStates; ++ls) {
                    _dist_prices[ls][d] = _slot_prices[ls][slot] + price;
                }
            }
            _match_count = 0;
        }

        void _update_align_prices() noexcept {
            for (uint32_t i = 0; i < lzma_model::AlignSize; ++i) {
                _align_prices[i] = rc::reverse_tree_price(_price, _probs.align, lzma_model::AlignBits, i);
            }
            _align_count = 0;
        }

        void _update_prices() noexcept {
            _price = rc::prices().v;
            _update_dist_prices();
            _update_align_prices();
            for (uint32_t ps = 0; ps <= _pb_mask(); ++ps) {
                _update_len_prices(_probs.len, _len_prices[ps], ps);
                _update_len_prices(_probs.rep_len, _rep_len_prices[ps], ps);
                _len_count[ps] = _rep_len_count[ps] = LenPriceUpdate;
            }
        }

        SGCL_LZMA_INLINE uint32_t _literal_price(uint64_t at_total, uint32_t prev, uint32_t state, uint32_t match, uint32_t byte) noexcept {
            const uint16_t* probs = _literal_probs(at_total, prev);
            const uint32_t* t = _price;
            if (state < lzma_model::LiteralStates) {
                return rc::tree_price(t, probs, 8, byte);
            }
            uint32_t price = 0, s = 1, offs = 0x100;
            for (int i = 7; i >= 0; --i) {
                uint32_t bit = (byte >> i) & 1;
                match <<= 1;
                uint32_t mb = match & offs;
                price += rc::price(t, probs[offs + mb + s], bit);
                s = (s << 1) | bit;
                offs &= bit ? mb : ~mb;
            }
            return price;
        }

        SGCL_LZMA_INLINE uint32_t _rep_index_price(uint32_t index, uint32_t state, uint32_t ps) const noexcept {
            const uint32_t* t = _price;
            if (index == 0) {
                return rc::price0(t, _probs.is_rep_g0[state]) + rc::price1(t, _probs.is_rep0_long[state][ps]);
            }
            uint32_t price = rc::price1(t, _probs.is_rep_g0[state]);
            if (index == 1) {
                return price + rc::price0(t, _probs.is_rep_g1[state]);
            }
            return price + rc::price1(t, _probs.is_rep_g1[state]) + rc::price(t, _probs.is_rep_g2[state], index - 2);
        }

        SGCL_LZMA_INLINE uint32_t _short_rep_price(uint32_t state, uint32_t ps) const noexcept {
            return rc::price0(_price, _probs.is_rep_g0[state]) + rc::price0(_price, _probs.is_rep0_long[state][ps]);
        }

        SGCL_LZMA_INLINE uint32_t _match_price(uint32_t dist, uint32_t len, uint32_t ps) const noexcept {
            uint32_t ls = std::min<uint32_t>(len - lzma_model::MatchMin, lzma_model::LenStates - 1);
            uint32_t price = _len_prices[ps][len - lzma_model::MatchMin];
            if (dist < lzma_model::FullDistances) {
                return price + _dist_prices[ls][dist];
            }
            return price + _slot_prices[ls][lzma_model::dist_slot(dist)] + _align_prices[dist & (lzma_model::AlignSize - 1)];
        }

        // --- parsing

        // A chunk of LZMA2 full: the next symbol might not fit (one takes
        // at most 273 bytes of input and some 20 bytes of output)
        SGCL_INLINE_HOT bool _chunk_full() const noexcept {
            return _chunked && (_total - _chunk_start > ChunkUnpacked - lzma_model::MatchMax || _rc_out_size() + _rc.pending() + 32 > ChunkPacked);
        }

        template<bool Tree>
        LzmaRun _run(bool finish, uint64_t budget) noexcept {
            uint64_t start = _total;
            for (;;) {
                if (_chunk_full()) {
                    return LzmaRun::chunk;
                }
                if (_path_at < _path_len) {
                    _emit(_path[_path_at++]);
                    continue;
                }
                size_t left = _mf.end - _at;
                if (left == 0 || (!finish && left < Lookahead)) {
                    return LzmaRun::done;
                }
                if (budget && _total - start >= budget) {
                    return LzmaRun::budget;
                }
                _path_at = _path_len = 0;
                if (_s.optimal) {
                    _optimum<Tree>();
                } else {
                    _fast<Tree>();
                }
            }
        }

        // The matches at _at: the ones found ahead of time, or a search now
        template<bool Tree>
        SGCL_INLINE_HOT uint32_t _matches_here() noexcept {
            if (_have_look) {
                _have_look = false;
                return _look_count;
            }
            return _mf.find<Tree>(_m);
        }

        // The length of the repeat of the distance at cur, up to limit
        SGCL_LZMA_INLINE uint32_t _rep_len(const uint8_t* cur, uint64_t at_total, uint32_t dist, uint32_t limit) const noexcept {
            if (dist >= at_total || limit < 2) {
                return 0;
            }
            const uint8_t* p = cur - dist - 1;
            if (p[0] != cur[0] || p[1] != cur[1]) {
                return 0;
            }
            return _extend(cur, p, 2, limit);
        }

        static SGCL_LZMA_INLINE uint32_t _extend(const uint8_t* a, const uint8_t* b, uint32_t len, uint32_t limit) noexcept {
            while (len + 8 <= limit) {
                uint64_t x, y;
                std::memcpy(&x, a + len, 8);
                std::memcpy(&y, b + len, 8);
                if (uint64_t d = x ^ y) {
                    return len + uint32_t(std::countr_zero(d) >> 3);
                }
                len += 8;
            }
            while (len < limit && a[len] == b[len]) {
                ++len;
            }
            return len;
        }

        SGCL_INLINE_HOT void _one_step(uint32_t len, uint32_t back) noexcept {
            _path[0] = {len, back, back == Literal ? 0 : back < 4 ? _reps[back] : back - 4};
            _path_len = 1;
        }

        SGCL_INLINE_HOT static bool _much_closer(uint32_t small, uint32_t big) noexcept {
            return small < (big >> 7);
        }

        // The fast levels: the longest match, a repeat when it is nearly as
        // long, and a literal instead when the next position has a better one
        template<bool Tree>
        void _fast() noexcept {
            uint32_t count = _matches_here<Tree>();
            const uint8_t* cur = _mf.data + _at;
            uint32_t avail = uint32_t(std::min<size_t>(_mf.end - _at, lzma_model::MatchMax));
            if (avail < 2) {
                _one_step(1, Literal);
                return;
            }
            uint32_t rep_len = 0, rep_index = 0;
            for (uint32_t i = 0; i < 4; ++i) {
                uint32_t len = _rep_len(cur, _total, _reps[i], avail);
                if (len > rep_len) {
                    rep_len = len;
                    rep_index = i;
                }
            }
            if (rep_len >= _s.nice) {
                _one_step(rep_len, rep_index);
                _mf.skip<Tree>(rep_len - 1);
                return;
            }
            uint32_t main_len = count ? _m[count - 1].len : 0;
            uint32_t main_dist = count ? _m[count - 1].dist : 0;
            if (main_len >= _s.nice) {
                _one_step(main_len, main_dist + 4);
                _mf.skip<Tree>(main_len - 1);
                return;
            }
            while (count > 1 && main_len == _m[count - 2].len + 1 && _much_closer(_m[count - 2].dist, main_dist)) {
                --count;
                main_len = _m[count - 1].len;
                main_dist = _m[count - 1].dist;
            }
            if (main_len == 2 && main_dist >= 0x80) {
                main_len = 1;
            }
            if (rep_len >= 2 && (rep_len + 1 >= main_len || (rep_len + 2 >= main_len && main_dist >= (1u << 9)) || (rep_len + 3 >= main_len && main_dist >= (1u << 15)))) {
                _one_step(rep_len, rep_index);
                _mf.skip<Tree>(rep_len - 1);
                return;
            }
            if (main_len < 2) {
                _one_step(1, Literal);
                return;
            }
            // one position ahead
            _look_count = _mf.find<Tree>(_m);
            _have_look = true;
            if (_look_count) {
                uint32_t next_len = _m[_look_count - 1].len;
                uint32_t next_dist = _m[_look_count - 1].dist;
                if ((next_len >= main_len && next_dist < main_dist) ||
                    (next_len == main_len + 1 && !_much_closer(main_dist, next_dist)) ||
                    next_len > main_len + 1 ||
                    (next_len + 1 >= main_len && main_len >= 3 && _much_closer(next_dist, main_dist))) {
                    _one_step(1, Literal);
                    return;
                }
            }
            uint32_t limit = std::max<uint32_t>(main_len - 1, 2);
            uint32_t next_avail = std::min<uint32_t>(avail - 1, limit);
            for (uint32_t i = 0; i < 4; ++i) {
                if (_rep_len(cur + 1, _total + 1, _reps[i], next_avail) >= limit) {
                    _one_step(1, Literal);
                    return;
                }
            }
            _have_look = false;
            _one_step(main_len, main_dist + 4);
            _mf.skip<Tree>(main_len - 2);
        }

        // The state and the repeats at a node, from the node it is reached from
        void _settle(Opt& o) const noexcept {
            const Opt& q = _opts[o.prev];
            uint32_t state = q.state;
            uint32_t r[4] = {q.reps[0], q.reps[1], q.reps[2], q.reps[3]};
            auto step = [&](uint32_t len, uint32_t back) noexcept {
                if (back == Literal) {
                    state = lzma_model::after_literal(state);
                } else if (back < 4) {
                    if (len == 1) {
                        state = lzma_model::after_short_rep(state);
                    } else {
                        uint32_t d = r[back];
                        for (uint32_t i = back; i > 0; --i) {
                            r[i] = r[i - 1];
                        }
                        r[0] = d;
                        state = lzma_model::after_rep(state);
                    }
                } else {
                    r[3] = r[2];
                    r[2] = r[1];
                    r[1] = r[0];
                    r[0] = back - 4;
                    state = lzma_model::after_match(state);
                }
            };
            uint32_t len = uint32_t(&o - _opts.get()) - o.prev;
            if (o.chain == 0) {
                step(len, o.back);
            } else {
                if (o.chain == 2) {
                    step(o.first, o.back);
                }
                state = lzma_model::after_literal(state);
                state = lzma_model::after_rep(state);
            }
            o.state = uint8_t(state);
            std::memcpy(o.reps, r, sizeof(r));
        }

        void _set(uint32_t at, uint32_t price, uint32_t prev, uint32_t back, uint8_t chain = 0, uint32_t first = 0) noexcept {
            Opt& o = _opts[at];
            o.price = price;
            o.prev = prev;
            o.back = back;
            o.chain = chain;
            o.first = first;
        }

        // The steps from node 0 to node end, into the path
        void _backtrack(uint32_t end) noexcept {
            uint32_t n = 0;
            uint32_t node = end;
            while (node > 0) {
                const Opt& o = _opts[node];
                const Opt& from = _opts[o.prev];
                uint32_t span = node - o.prev;
                uint32_t dist = o.back == Literal ? 0 : o.back < 4 ? from.reps[o.back] : o.back - 4;
                if (o.chain == 0) {
                    _path[n++] = {span, o.back, dist};
                } else if (o.chain == 1) {
                    _path[n++] = {span - 1, 0, from.reps[0]};
                    _path[n++] = {1, Literal, 0};
                } else {
                    _path[n++] = {span - o.first - 1, 0, dist};
                    _path[n++] = {1, Literal, 0};
                    _path[n++] = {o.first, o.back, dist};
                }
                node = o.prev;
            }
            std::reverse(_path.get(), _path.get() + n);
            _path_len = n;
            _path_at = 0;
        }

        // The optimal parser: the cheapest way to code the bytes ahead, node
        // by node — node k the cheapest way found to reach k bytes on, each
        // node priced into the ones its literal, short repeat, repeats and
        // matches reach, and into the ones a step followed by a literal and
        // a repeat of rep0 reaches — until a node has a match of the nice
        // length or no node reaches further
        template<bool Tree>
        void _optimum() noexcept {
            uint32_t count = _matches_here<Tree>();
            const uint8_t* base = _mf.data + _at;
            const uint32_t* t = _price;
            const uint32_t pb_mask = _pb_mask();
            uint32_t avail = uint32_t(std::min<size_t>(_mf.end - _at, lzma_model::MatchMax));
            if (avail < 2) {
                _one_step(1, Literal);
                return;
            }
            uint32_t rep_lens[4];
            uint32_t best_rep = 0;
            for (uint32_t i = 0; i < 4; ++i) {
                rep_lens[i] = _rep_len(base, _total, _reps[i], avail);
                if (rep_lens[i] > rep_lens[best_rep]) {
                    best_rep = i;
                }
            }
            if (rep_lens[best_rep] >= _s.nice) {
                _one_step(rep_lens[best_rep], best_rep);
                _mf.skip<Tree>(rep_lens[best_rep] - 1);
                return;
            }
            uint32_t main_len = count ? _m[count - 1].len : 0;
            if (main_len >= _s.nice) {
                _one_step(main_len, _m[count - 1].dist + 4);
                _mf.skip<Tree>(main_len - 1);
                return;
            }
            uint32_t cur_byte = base[0];
            uint32_t match_byte = _total > _reps[0] ? base[-ptrdiff_t(_reps[0]) - 1] : ~cur_byte & 0xFF;
            if (main_len < 2 && cur_byte != match_byte && rep_lens[best_rep] < 2) {
                _one_step(1, Literal);
                return;
            }
            Opt* opts = _opts.get();
            opts[0].state = _state;
            std::memcpy(opts[0].reps, _reps, sizeof(_reps));
            opts[0].price = 0;
            uint32_t ps = uint32_t(_total) & pb_mask;
            _set(1, rc::price0(t, _probs.is_match[_state][ps]) + _literal_price(_total, _total ? base[-1] : 0, _state, match_byte, cur_byte), 0, Literal);
            uint32_t rep_match = rc::price1(t, _probs.is_match[_state][ps]);
            uint32_t rep_base = rep_match + rc::price1(t, _probs.is_rep[_state]);
            if (match_byte == cur_byte) {
                uint32_t price = rep_base + _short_rep_price(_state, ps);
                if (price < opts[1].price) {
                    _set(1, price, 0, 0);
                }
            }
            uint32_t len_end = std::max(main_len, rep_lens[best_rep]);
            if (len_end < 2) {
                _one_step(1, opts[1].back);
                return;
            }
            for (uint32_t i = 2; i <= len_end; ++i) {
                opts[i].price = rc::Infinity;
            }
            for (uint32_t i = 0; i < 4; ++i) {
                if (rep_lens[i] < 2) {
                    continue;
                }
                uint32_t price = rep_base + _rep_index_price(i, _state, ps);
                for (uint32_t len = rep_lens[i]; len >= 2; --len) {
                    uint32_t p = price + _rep_len_prices[ps][len - lzma_model::MatchMin];
                    if (p < opts[len].price) {
                        _set(len, p, 0, i);
                    }
                }
            }
            uint32_t normal = rep_match + rc::price0(t, _probs.is_rep[_state]);
            uint32_t len = rep_lens[0] >= 2 ? rep_lens[0] + 1 : 2;
            if (len <= main_len) {
                uint32_t k = 0;
                while (len > _m[k].len) {
                    ++k;
                }
                for (;; ++len) {
                    uint32_t dist = _m[k].dist;
                    uint32_t p = normal + _match_price(dist, len, ps);
                    if (p < opts[len].price) {
                        _set(len, p, 0, dist + 4);
                    }
                    if (len == _m[k].len && ++k == count) {
                        break;
                    }
                }
            }
            uint32_t cur = 0;
            for (;;) {
                ++cur;
                if (cur == len_end) {
                    break;
                }
                Opt& here = opts[cur];
                if (here.price >= rc::Infinity) {
                    _mf.skip<Tree>(1);
                    continue;
                }
                _settle(here);
                uint32_t n = _mf.find<Tree>(_m);
                uint32_t new_len = n ? _m[n - 1].len : 0;
                if (new_len >= _s.nice) {
                    _look_count = n;
                    _have_look = true;
                    break;
                }
                const uint8_t* p = base + cur;
                uint64_t at_total = _total + cur;
                uint32_t pps = uint32_t(at_total) & pb_mask;
                uint32_t state = here.state;
                const uint32_t* r = here.reps;
                uint32_t price = here.price;
                uint32_t avail_full = uint32_t(std::min<size_t>(_mf.end - _at - cur, lzma_model::MatchMax));
                cur_byte = p[0];
                match_byte = p[-ptrdiff_t(r[0]) - 1];
                // a literal, or the short repeat
                uint32_t lit = price + rc::price0(t, _probs.is_match[state][pps]) + _literal_price(at_total, p[-1], state, match_byte, cur_byte);
                bool next_literal = false;
                if (lit < opts[cur + 1].price) {
                    _set(cur + 1, lit, cur, Literal);
                    next_literal = true;
                }
                uint32_t rm = price + rc::price1(t, _probs.is_match[state][pps]);
                uint32_t rb = rm + rc::price1(t, _probs.is_rep[state]);
                if (match_byte == cur_byte && !(opts[cur + 1].prev == cur && opts[cur + 1].back == 0 && opts[cur + 1].chain == 0)) {
                    uint32_t sp = rb + _short_rep_price(state, pps);
                    if (sp <= opts[cur + 1].price) {
                        _set(cur + 1, sp, cur, 0);
                        next_literal = false;
                    }
                }
                uint32_t avail_here = std::min(avail_full, Opts - 1 - cur);
                if (avail_here < 2) {
                    continue;
                }
                auto reach = [&](uint32_t to) noexcept {
                    while (len_end < to) {
                        opts[++len_end].price = rc::Infinity;
                    }
                };
                // a literal, then a repeat of rep0
                if (!next_literal && match_byte != cur_byte) {
                    uint32_t limit = std::min(avail_full - 1, _s.nice);
                    uint32_t len2 = _rep_len(p + 1, at_total + 1, r[0], limit);
                    if (len2 >= 2 && cur + 1 + len2 <= Opts - 1) {
                        uint32_t s2 = lzma_model::after_literal(state);
                        uint32_t ps2 = uint32_t(at_total + 1) & pb_mask;
                        uint32_t p2 = lit + rc::price1(t, _probs.is_match[s2][ps2]) + rc::price1(t, _probs.is_rep[s2]) + _rep_index_price(0, s2, ps2) + _rep_len_prices[ps2][len2 - lzma_model::MatchMin];
                        uint32_t to = cur + 1 + len2;
                        reach(to);
                        if (p2 < opts[to].price) {
                            _set(to, p2, cur, 0, 1);
                        }
                    }
                }
                uint32_t start_len = 2;
                for (uint32_t i = 0; i < 4; ++i) {
                    uint32_t rl = _rep_len(p, at_total, r[i], avail_here);
                    if (rl < 2) {
                        continue;
                    }
                    reach(cur + rl);
                    uint32_t rp = rb + _rep_index_price(i, state, pps);
                    for (uint32_t l = rl; l >= 2; --l) {
                        uint32_t c = rp + _rep_len_prices[pps][l - lzma_model::MatchMin];
                        if (c < opts[cur + l].price) {
                            _set(cur + l, c, cur, i);
                        }
                    }
                    if (i == 0) {
                        start_len = rl + 1;
                    }
                    // the repeat, a literal, then the same distance again
                    if (avail_full > rl + 2) {
                        const uint8_t* src = p - r[i] - 1;
                        uint32_t limit = std::min(avail_full - rl - 1, _s.nice);
                        uint32_t len2 = _extend(p + rl + 1, src + rl + 1, 0, limit);
                        if (len2 >= 2 && cur + rl + 1 + len2 <= Opts - 1) {
                            uint32_t s2 = lzma_model::after_rep(state);
                            uint32_t ps2 = uint32_t(at_total + rl) & pb_mask;
                            uint32_t c = rp + _rep_len_prices[pps][rl - lzma_model::MatchMin] + rc::price0(t, _probs.is_match[s2][ps2]) + _literal_price(at_total + rl, p[rl - 1], s2, src[rl], p[rl]);
                            uint32_t s3 = lzma_model::after_literal(s2);
                            uint32_t ps3 = uint32_t(at_total + rl + 1) & pb_mask;
                            c += rc::price1(t, _probs.is_match[s3][ps3]) + rc::price1(t, _probs.is_rep[s3]) + _rep_index_price(0, s3, ps3) + _rep_len_prices[ps3][len2 - lzma_model::MatchMin];
                            uint32_t to = cur + rl + 1 + len2;
                            reach(to);
                            if (c < opts[to].price) {
                                _set(to, c, cur, i, 2, rl);
                            }
                        }
                    }
                }
                if (new_len > avail_here) {
                    uint32_t k = 0;
                    while (_m[k].len < avail_here) {
                        ++k;
                    }
                    _m[k].len = avail_here;
                    n = k + 1;
                    new_len = avail_here;
                }
                if (new_len < start_len) {
                    continue;
                }
                reach(cur + new_len);
                uint32_t np = rm + rc::price0(t, _probs.is_rep[state]);
                uint32_t k = 0;
                while (start_len > _m[k].len) {
                    ++k;
                }
                for (uint32_t l = start_len;; ++l) {
                    uint32_t dist = _m[k].dist;
                    uint32_t c = np + _match_price(dist, l, pps);
                    if (c < opts[cur + l].price) {
                        _set(cur + l, c, cur, dist + 4);
                    }
                    if (l == _m[k].len) {
                        // the match, a literal, then its distance again
                        if (avail_full > l + 2) {
                            const uint8_t* src = p - dist - 1;
                            uint32_t limit = std::min(avail_full - l - 1, _s.nice);
                            uint32_t len2 = _extend(p + l + 1, src + l + 1, 0, limit);
                            if (len2 >= 2 && cur + l + 1 + len2 <= Opts - 1) {
                                uint32_t s2 = lzma_model::after_match(state);
                                uint32_t ps2 = uint32_t(at_total + l) & pb_mask;
                                uint32_t c2 = c + rc::price0(t, _probs.is_match[s2][ps2]) + _literal_price(at_total + l, p[l - 1], s2, src[l], p[l]);
                                uint32_t s3 = lzma_model::after_literal(s2);
                                uint32_t ps3 = uint32_t(at_total + l + 1) & pb_mask;
                                c2 += rc::price1(t, _probs.is_match[s3][ps3]) + rc::price1(t, _probs.is_rep[s3]) + _rep_index_price(0, s3, ps3) + _rep_len_prices[ps3][len2 - lzma_model::MatchMin];
                                uint32_t to = cur + l + 1 + len2;
                                reach(to);
                                if (c2 < opts[to].price) {
                                    _set(to, c2, cur, dist + 4, 2, l);
                                }
                            }
                        }
                        if (++k == n) {
                            break;
                        }
                    }
                }
            }
            _backtrack(cur);
        }

        LzmaEncoderSettings _s;
        uint32_t _dictionary;
        std::vector<uint8_t> _sink;    // the coder's output before a sink is given
        RangeEncoder _rc;
        LzmaProbs _probs;
        std::unique_ptr<uint16_t[]> _literal;
        uint32_t _state = 0;
        uint32_t _reps[4] = {0, 0, 0, 0};
        uint64_t _total = 0;           // bytes coded
        size_t _at = 0;                // the window's index of the next byte to code
        LzmaMatchFinder _mf;
        std::unique_ptr<uint8_t[]> _window;   // a stream's own window
        size_t _capacity = 0;
        size_t _keep = 0;              // the bytes before _at kept when the window moves down
        bool _ready = false;
        bool _chunked = false;         // LZMA2's chunks
        uint64_t _chunk_start = 0;     // the total at the chunk's start
        std::vector<uint8_t>* _rc_sink = &_sink;
        LzmaMatchFinder::Match _m[LzmaMatchFinder::MaxMatches];
        uint32_t _look_count = 0;
        bool _have_look = false;       // _m holds the matches at _at, found ahead of time
        std::unique_ptr<Opt[]> _opts{new Opt[Opts + 1]};
        std::unique_ptr<Step[]> _path{new Step[Opts + 4]};
        uint32_t _path_at = 0;
        uint32_t _path_len = 0;
        const uint32_t* _price = rc::prices().v;
        bool _prices_ready = false;
        uint32_t _len_prices[lzma_model::PosStatesMax][LenPrices];
        uint32_t _rep_len_prices[lzma_model::PosStatesMax][LenPrices];
        uint32_t _len_count[lzma_model::PosStatesMax] = {};
        uint32_t _rep_len_count[lzma_model::PosStatesMax] = {};
        uint32_t _slot_prices[lzma_model::LenStates][1 << lzma_model::DistSlotBits];
        uint32_t _dist_prices[lzma_model::LenStates][lzma_model::FullDistances];
        uint32_t _align_prices[lzma_model::AlignSize];
        uint32_t _match_count = 0;
        uint32_t _align_count = 0;
    };
}
