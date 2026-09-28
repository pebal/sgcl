//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "range_coder.h"
#include "../error.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace sgcl::compress::detail {
    // The model of LZMA (lzma-specification.txt of the LZMA SDK): twelve
    // states of what the last symbols were, the four last distances (rep0
    // to rep3), and the probabilities every bit is coded with — whether a
    // symbol is a literal, a match or a repeat of one of the four
    // distances, the lengths (2 to 273, a low, a middle and a high tree
    // per position state), the distances (a slot of six bits per length
    // state, then bits under the slot through reverse trees, or direct
    // bits and four aligned ones), and the literals (a tree of eight bits
    // per context of lc bits of the byte before and lp bits of the
    // position, coded against the byte at rep0 after a match).
    namespace lzma_model {
        constexpr uint32_t States = 12;
        constexpr uint32_t LiteralStates = 7;          // states under 7: the last symbol a literal
        constexpr uint32_t PosStatesMax = 16;
        constexpr uint32_t MatchMin = 2;
        constexpr uint32_t MatchMax = 273;
        constexpr uint32_t LenStates = 4;
        constexpr uint32_t DistSlotBits = 6;
        constexpr uint32_t StartPosModel = 4;
        constexpr uint32_t EndPosModel = 14;
        constexpr uint32_t FullDistances = 128;
        constexpr uint32_t AlignBits = 4;
        constexpr uint32_t AlignSize = 16;
        constexpr uint32_t DictionaryMin = uint32_t(1) << 12;
        constexpr uint32_t EndMarker = 0xFFFFFFFF;   // the distance of the end marker

        constexpr uint8_t after_literal(uint32_t s) noexcept {
            return uint8_t(s < 4 ? 0 : s < 10 ? s - 3 : s - 6);
        }

        constexpr uint8_t after_match(uint32_t s) noexcept {
            return uint8_t(s < LiteralStates ? 7 : 10);
        }

        constexpr uint8_t after_rep(uint32_t s) noexcept {
            return uint8_t(s < LiteralStates ? 8 : 11);
        }

        constexpr uint8_t after_short_rep(uint32_t s) noexcept {
            return uint8_t(s < LiteralStates ? 9 : 11);
        }

        // The slot of a distance: its top two bits and the place of the top one
        inline uint32_t dist_slot(uint32_t dist) noexcept {
            if (dist < StartPosModel) {
                return dist;
            }
            uint32_t top = 31 - uint32_t(__builtin_clz(dist));
            return (top << 1) | ((dist >> (top - 1)) & 1);
        }
    }

    struct LzmaLengthProbs {
        uint16_t choice;
        uint16_t choice2;
        uint16_t low[lzma_model::PosStatesMax][8];
        uint16_t mid[lzma_model::PosStatesMax][8];
        uint16_t high[256];
    };

    struct LzmaProbs {
        uint16_t is_match[lzma_model::States][lzma_model::PosStatesMax];
        uint16_t is_rep[lzma_model::States];
        uint16_t is_rep_g0[lzma_model::States];
        uint16_t is_rep_g1[lzma_model::States];
        uint16_t is_rep_g2[lzma_model::States];
        uint16_t is_rep0_long[lzma_model::States][lzma_model::PosStatesMax];
        uint16_t dist_slot[lzma_model::LenStates][1 << lzma_model::DistSlotBits];
        uint16_t dist_special[1 + lzma_model::FullDistances - lzma_model::EndPosModel];
        uint16_t align[lzma_model::AlignSize];
        LzmaLengthProbs len;
        LzmaLengthProbs rep_len;

        void reset() noexcept {
            uint16_t* p = reinterpret_cast<uint16_t*>(this);
            std::fill(p, p + sizeof(*this) / sizeof(uint16_t), rc::ProbInit);
        }
    };

    // lc, lp and pb as the properties byte packs them ((pb * 5 + lp) * 9 + lc),
    // and the dictionary size
    struct LzmaProperties {
        uint32_t lc = 3;
        uint32_t lp = 0;
        uint32_t pb = 2;
        uint32_t dictionary = uint32_t(1) << 23;

        static bool from_byte(uint8_t b, LzmaProperties& p) noexcept {
            if (b >= 9 * 5 * 5) {
                return false;
            }
            p.lc = b % 9;
            b = uint8_t(b / 9);
            p.lp = b % 5;
            p.pb = b / 5;
            return true;
        }

        uint8_t to_byte() const noexcept {
            return uint8_t((pb * 5 + lp) * 9 + lc);
        }

        // The literal probabilities of these lc and lp
        size_t literal_probs() const noexcept {
            return size_t(0x300) << (lc + lp);
        }
    };

    enum class LzmaStatus : uint8_t {
        need_input,   // every input byte taken, the data goes on
        need_room,    // the window is full to the limit given
        done,         // the end: the marker, or the size known reached
        failed        // error and error_text say why
    };

    // The decoder, resumable at any byte of the input and of the output.
    // It decodes into a window the caller owns: in the linear form the
    // window is the whole of the output (a decompression in memory), and a
    // match may write up to CopySlack bytes past the limit; in the circular
    // form it is the dictionary, pos goes round it, and nothing past a
    // match is touched. A symbol whose bytes are all at hand is decoded
    // straight from the input, with the fast loop while at least Margin
    // bytes remain and the room takes the longest match; the last bytes
    // of an input given in pieces wait in a buffer of their own until a
    // symbol's worth is there — found by decoding it dry, without moving a
    // probability (no probability is read twice in one symbol, so the dry
    // decoding takes the same bits and bytes the real one does).
    class LzmaDecoder {
    public:
        static constexpr size_t Margin = 32;      // more than a symbol can take (about 20 bytes)
        static constexpr size_t CopySlack = 16;   // the linear window's bytes past the limit a match may write

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        // For the data of these properties; size is the length decoded
        // (UINT64_MAX: unknown, the end marker ends it)
        void reset(const LzmaProperties& p, uint64_t size) {
            _props = p;
            _dictionary = std::max(p.dictionary, lzma_model::DictionaryMin);
            size_t n = p.literal_probs();
            if (_literal_count < n) {
                _literal.reset(new uint16_t[n]);
                _literal_count = n;
            }
            std::fill(_literal.get(), _literal.get() + n, rc::ProbInit);
            _probs.reset();
            _state = 0;
            _rep[0] = _rep[1] = _rep[2] = _rep[3] = 0;
            _range = 0xFFFFFFFF;
            _code = 0;
            _started = false;
            _ended = false;
            _failed = false;
            _expect_marker = false;
            _lzma2 = false;
            _pending = 0;
            _tmp_size = 0;
            _total = 0;
            _size = size;
            _taken = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        // LZMA2's chunks (detail/lzma2.h): the dictionary once, then a
        // chunk at a time — its size, the range coder started anew, and
        // before it, as the chunk's header says, the dictionary reset (the
        // position starts over), new properties and the state reset
        void lzma2_init(uint32_t dictionary) {
            _dictionary = std::max(dictionary, lzma_model::DictionaryMin);
            _lzma2 = true;
            _failed = false;
            _ended = false;
            _total = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        void lzma2_dictionary_reset() noexcept {
            _total = 0;
        }

        void lzma2_properties(const LzmaProperties& p) {
            _props.lc = p.lc;
            _props.lp = p.lp;
            _props.pb = p.pb;
            size_t n = _props.literal_probs();
            if (_literal_count < n) {
                _literal.reset(new uint16_t[n]);
                _literal_count = n;
            }
        }

        void lzma2_state_reset() noexcept {
            std::fill(_literal.get(), _literal.get() + _props.literal_probs(), rc::ProbInit);
            _probs.reset();
            _state = 0;
            _rep[0] = _rep[1] = _rep[2] = _rep[3] = 0;
        }

        void lzma2_chunk(uint32_t size) noexcept {
            _size = _total + size;
            _range = 0xFFFFFFFF;
            _code = 0;
            _started = false;
            _ended = false;
            _expect_marker = false;
            _pending = 0;
            _tmp_size = 0;
            _taken = 0;
        }

        // An uncompressed chunk's bytes, laid in the window by the caller
        void lzma2_skip(uint64_t n) noexcept {
            _total += n;
        }

        // Bytes decoded so far, and input bytes taken
        uint64_t total() const noexcept {
            return _total;
        }

        uint64_t taken() const noexcept {
            return _taken;
        }

        // Decodes into window[pos, limit): limit is at most the window's
        // size (the linear form: its size less CopySlack)
        template<bool Linear>
        LzmaStatus decode(uint8_t* window, size_t size, size_t& pos, size_t limit, const uint8_t*& in, const uint8_t* end, bool input_ended) {
            if (_failed) {
                return LzmaStatus::failed;
            }
            if (_ended) {
                return LzmaStatus::done;
            }
            if (!_started) {
                while (_tmp_size < 5 && in < end) {
                    _tmp[_tmp_size++] = *in++;
                }
                if (_tmp_size < 5) {
                    return input_ended ? _fail(errc::unexpected_end, "lzma: unexpected end of the compressed data") : LzmaStatus::need_input;
                }
                if (_tmp[0] != 0) {
                    return _fail(errc::corrupt, "lzma: the range coder's first byte is not 0");
                }
                _code = uint32_t(_tmp[1]) << 24 | uint32_t(_tmp[2]) << 16 | uint32_t(_tmp[3]) << 8 | _tmp[4];
                _range = 0xFFFFFFFF;
                if (_code == _range) {
                    return _fail(errc::corrupt, "lzma: corrupt data");
                }
                _tmp_size = 0;
                _taken = 5;
                _started = true;
            }
            for (;;) {
                if (_pending) {
                    size_t n = std::min<size_t>(_pending, limit - pos);
                    _copy_match<false>(window, size, pos, _rep[0], n);
                    _total += n;
                    _pending -= uint32_t(n);
                    if (_pending) {
                        return LzmaStatus::need_room;
                    }
                }
                if (_total == _size && !_expect_marker) {
                    // the size known reached: the end, unless a marker follows
                    if (_code == 0) {
                        _ended = true;
                        return LzmaStatus::done;
                    }
                    if (_lzma2) {
                        return _fail(errc::corrupt, "lzma2: a chunk's range coder does not end at its size");
                    }
                    _expect_marker = true;
                }
                if (pos >= limit && !_expect_marker) {
                    return LzmaStatus::need_room;
                }
                if (_tmp_size == 0 && size_t(end - in) >= Margin) {
                    size_t room = limit - pos;
                    uint64_t left = _size - _total;
                    if (!_expect_marker && room > lzma_model::MatchMax && left > lzma_model::MatchMax) {
                        size_t stop = limit - lzma_model::MatchMax;
                        if (left - lzma_model::MatchMax < stop - pos) {
                            stop = pos + size_t(left - lzma_model::MatchMax);
                        }
                        auto st = _fast<Linear>(window, size, pos, stop, in, end - Margin);
                        if (st != LzmaStatus::need_input) {
                            return st;
                        }
                        continue;
                    }
                    Real r{_range, _code, in};
                    auto st = _one(r, window, size, pos, limit);
                    _taken += uint64_t(r.in - in);
                    in = r.in;
                    _range = r.range;
                    _code = r.code;
                    if (st != LzmaStatus::need_input) {
                        return st;
                    }
                    continue;
                }
                // the input's last bytes: a symbol through the buffer
                size_t had = _tmp_size;
                size_t add = std::min<size_t>(Margin - had, size_t(end - in));
                std::memcpy(_tmp + had, in, add);
                size_t have = had + add;
                Dry d{_range, _code, _tmp, _tmp + have, true};
                _symbol(d, window, size, pos);
                if (!d.ok) {
                    if (have == Margin) {
                        return _fail(errc::corrupt, "lzma: corrupt data");   // cannot be: a symbol takes fewer bytes
                    }
                    in += add;
                    _tmp_size = have;
                    if (input_ended) {
                        return _fail(errc::unexpected_end, "lzma: unexpected end of the compressed data");
                    }
                    return LzmaStatus::need_input;
                }
                Real r{_range, _code, _tmp};
                auto st = _one(r, window, size, pos, limit);
                size_t used = size_t(r.in - _tmp);
                _taken += used;
                _range = r.range;
                _code = r.code;
                if (used >= had) {
                    in += used - had;
                    _tmp_size = 0;
                } else {
                    std::memmove(_tmp, _tmp + used, had - used);
                    _tmp_size = had - used;
                }
                if (st != LzmaStatus::need_input) {
                    return st;
                }
            }
        }

    private:
        // The range decoder over bytes known to be there
        struct Real {
            uint32_t range;
            uint32_t code;
            const uint8_t* in;

            SGCL_LZMA_INLINE void normalize() noexcept {
                if (range < rc::Top) {
                    range <<= 8;
                    code = (code << 8) | *in++;
                }
            }

            SGCL_LZMA_INLINE uint32_t bit(uint16_t& prob) noexcept {
                uint32_t bound = (range >> rc::ProbBits) * prob;
                uint32_t b;
                if (code < bound) {
                    range = bound;
                    prob = uint16_t(prob + ((rc::ProbOne - prob) >> rc::MoveBits));
                    b = 0;
                } else {
                    range -= bound;
                    code -= bound;
                    prob = uint16_t(prob - (prob >> rc::MoveBits));
                    b = 1;
                }
                normalize();
                return b;
            }

            // A bit of a literal, without a branch on its value: a literal's
            // bits are the least predictable of the stream
            SGCL_LZMA_INLINE uint32_t literal_bit(uint16_t& prob) noexcept {
                uint32_t p = prob;
                uint32_t bound = (range >> rc::ProbBits) * p;
                uint32_t b = code >= bound;
                uint32_t mask = 0u - b;
                range = (bound & ~mask) | ((range - bound) & mask);
                code -= bound & mask;
                uint32_t up = p + ((rc::ProbOne - p) >> rc::MoveBits);
                uint32_t down = p - (p >> rc::MoveBits);
                prob = uint16_t((up & ~mask) | (down & mask));
                normalize();
                return b;
            }

            SGCL_LZMA_INLINE uint32_t direct(uint32_t bits) noexcept {
                uint32_t v = 0;
                do {
                    range >>= 1;
                    code -= range;
                    uint32_t t = 0 - (code >> 31);
                    code += range & t;
                    v = (v << 1) + (t + 1);
                    normalize();
                } while (--bits);
                return v;
            }
        };

        // The same over the bytes there are, moving no probability: ok is
        // false when the symbol needs more than there is
        struct Dry {
            uint32_t range;
            uint32_t code;
            const uint8_t* in;
            const uint8_t* end;
            bool ok;

            SGCL_LZMA_INLINE void normalize() noexcept {
                if (range < rc::Top) {
                    range <<= 8;
                    if (in == end) {
                        ok = false;
                        code <<= 8;
                    } else {
                        code = (code << 8) | *in++;
                    }
                }
            }

            SGCL_LZMA_INLINE uint32_t bit(uint16_t& prob) noexcept {
                uint32_t bound = (range >> rc::ProbBits) * prob;
                uint32_t b;
                if (code < bound) {
                    range = bound;
                    b = 0;
                } else {
                    range -= bound;
                    code -= bound;
                    b = 1;
                }
                normalize();
                return b;
            }

            SGCL_LZMA_INLINE uint32_t direct(uint32_t bits) noexcept {
                uint32_t v = 0;
                do {
                    range >>= 1;
                    code -= range;
                    uint32_t t = 0 - (code >> 31);
                    code += range & t;
                    v = (v << 1) + (t + 1);
                    normalize();
                } while (--bits);
                return v;
            }
        };

        enum Kind : uint8_t { Literal, ShortRep, Rep, Match, Marker, Bad };

        struct Symbol {
            Kind kind;
            uint32_t len;     // of a repeat or a match
            uint32_t value;   // the literal, the index of the repeat, the distance
        };

        // The byte `back` + 1 before pos (the window's own, round it in the
        // circular form)
        static SGCL_LZMA_INLINE uint8_t _back(const uint8_t* window, size_t size, size_t pos, uint32_t back) noexcept {
            size_t d = size_t(back) + 1;
            return window[pos >= d ? pos - d : pos + size - d];
        }

        uint16_t* _literal_probs(uint32_t prev) noexcept {
            uint32_t ctx = ((uint32_t(_total) & ((1u << _props.lp) - 1)) << _props.lc) + (prev >> (8 - _props.lc));
            return _literal.get() + size_t(0x300) * ctx;
        }

        template<class R>
        static SGCL_LZMA_INLINE uint32_t _length(R& r, LzmaLengthProbs& p, uint32_t ps) noexcept {
            if (!r.bit(p.choice)) {
                uint32_t s = 1;
                for (int i = 0; i < 3; ++i) s = (s << 1) | r.bit(p.low[ps][s]);
                return s - 8 + lzma_model::MatchMin;
            }
            if (!r.bit(p.choice2)) {
                uint32_t s = 1;
                for (int i = 0; i < 3; ++i) s = (s << 1) | r.bit(p.mid[ps][s]);
                return s - 8 + lzma_model::MatchMin + 8;
            }
            uint32_t s = 1;
            for (int i = 0; i < 8; ++i) s = (s << 1) | r.bit(p.high[s]);
            return s - 256 + lzma_model::MatchMin + 16;
        }

        template<class R>
        SGCL_LZMA_INLINE uint32_t _distance(R& r, uint32_t len) noexcept {
            uint32_t ls = std::min<uint32_t>(len - lzma_model::MatchMin, lzma_model::LenStates - 1);
            uint16_t* t = _probs.dist_slot[ls];
            uint32_t s = 1;
            for (int i = 0; i < 6; ++i) s = (s << 1) | r.bit(t[s]);
            uint32_t slot = s - 64;
            if (slot < lzma_model::StartPosModel) {
                return slot;
            }
            uint32_t bits = (slot >> 1) - 1;
            uint32_t dist = (2 | (slot & 1)) << bits;
            if (slot < lzma_model::EndPosModel) {
                uint16_t* q = _probs.dist_special + dist - slot;
                uint32_t m = 1;
                for (uint32_t i = 0; i < bits; ++i) {
                    uint32_t b = r.bit(q[m]);
                    m = (m << 1) | b;
                    dist |= b << i;
                }
                return dist;
            }
            dist += r.direct(bits - lzma_model::AlignBits) << lzma_model::AlignBits;
            uint32_t m = 1;
            for (uint32_t i = 0; i < lzma_model::AlignBits; ++i) {
                uint32_t b = r.bit(_probs.align[m]);
                m = (m << 1) | b;
                dist |= b << i;
            }
            return dist;
        }

        // One symbol, decoded; nothing of the state changed (but the
        // probabilities, by a Real decoder)
        template<class R>
        Symbol _symbol(R& r, const uint8_t* window, size_t size, size_t pos) {
            uint32_t ps = uint32_t(_total) & ((1u << _props.pb) - 1);
            if (!r.bit(_probs.is_match[_state][ps])) {
                uint16_t* lit = _literal_probs(_total ? _back(window, size, pos, 0) : 0);
                uint32_t s = 1;
                if (_state < lzma_model::LiteralStates) {
                    do s = (s << 1) | r.bit(lit[s]); while (s < 0x100);
                } else {
                    uint32_t match = _back(window, size, pos, _rep[0]);
                    uint32_t offs = 0x100;
                    do {
                        match <<= 1;
                        uint32_t mb = match & offs;
                        uint32_t b = r.bit(lit[offs + mb + s]);
                        s = (s << 1) | b;
                        offs &= b ? mb : ~mb;
                    } while (s < 0x100);
                }
                return {Literal, 1, s & 0xFF};
            }
            if (r.bit(_probs.is_rep[_state])) {
                if (_total == 0) {
                    return {Bad, 0, 0};
                }
                uint32_t index;
                if (!r.bit(_probs.is_rep_g0[_state])) {
                    if (!r.bit(_probs.is_rep0_long[_state][ps])) {
                        return {ShortRep, 1, 0};
                    }
                    index = 0;
                } else if (!r.bit(_probs.is_rep_g1[_state])) {
                    index = 1;
                } else if (!r.bit(_probs.is_rep_g2[_state])) {
                    index = 2;
                } else {
                    index = 3;
                }
                return {Rep, _length(r, _probs.rep_len, ps), index};
            }
            uint32_t len = _length(r, _probs.len, ps);
            uint32_t dist = _distance(r, len);
            if (dist == lzma_model::EndMarker) {
                return {Marker, len, dist};
            }
            return {Match, len, dist};
        }

        // One symbol decoded and applied with every check: the room, the
        // size known, a distance past the start
        LzmaStatus _one(Real& r, uint8_t* window, size_t size, size_t& pos, size_t limit) {
            Symbol s = _symbol(r, window, size, pos);
            if (_expect_marker && s.kind != Marker) {
                return _fail(errc::corrupt, "lzma: data past the size in the header");
            }
            switch (s.kind) {
                case Literal:
                    window[pos++] = uint8_t(s.value);
                    ++_total;
                    _state = lzma_model::after_literal(_state);
                    return LzmaStatus::need_input;
                case ShortRep:
                    window[pos] = _back(window, size, pos, _rep[0]);
                    ++pos;
                    ++_total;
                    _state = lzma_model::after_short_rep(_state);
                    return LzmaStatus::need_input;
                case Rep: {
                    uint32_t d = _rep[s.value];
                    for (uint32_t i = s.value; i > 0; --i) {
                        _rep[i] = _rep[i - 1];
                    }
                    _rep[0] = d;
                    _state = lzma_model::after_rep(_state);
                    break;
                }
                case Match:
                    if (s.value >= std::min<uint64_t>(_dictionary, size) || s.value >= _total) {
                        return _fail(errc::corrupt, "lzma: a distance past the start of the data");
                    }
                    _rep[3] = _rep[2];
                    _rep[2] = _rep[1];
                    _rep[1] = _rep[0];
                    _rep[0] = s.value;
                    _state = lzma_model::after_match(_state);
                    break;
                case Marker:
                    if (r.code != 0) {
                        return _fail(errc::corrupt, "lzma: corrupt data at the end marker");
                    }
                    if (_size != UINT64_MAX && _total != _size) {
                        return _fail(errc::corrupt, "lzma: the end marker before the size in the header");
                    }
                    _ended = true;
                    return LzmaStatus::done;
                case Bad:
                    return _fail(errc::corrupt, "lzma: a repeated match before any data");
            }
            if (s.len > _size - _total) {
                return _fail(errc::corrupt, "lzma: data past the size in the header");
            }
            size_t n = std::min<size_t>(s.len, limit - pos);
            _copy_match<false>(window, size, pos, _rep[0], n);
            _total += n;
            _pending = s.len - uint32_t(n);
            return _pending ? LzmaStatus::need_room : LzmaStatus::need_input;
        }

        // n bytes from dist + 1 back, pos moved on. The linear form (Over)
        // may write up to CopySlack bytes past the n.
        template<bool Over>
        static SGCL_LZMA_INLINE void _copy_match(uint8_t* window, size_t size, size_t& pos, uint32_t dist, size_t n) noexcept {
            size_t d = size_t(dist) + 1;
            uint8_t* dst = window + pos;
            if (SGCL_LZMA_LIKELY(pos >= d)) {
                const uint8_t* src = dst - d;
                pos += n;
                if constexpr (Over) {
                    if (d >= 16) {
                        for (size_t i = 0; i < n; i += 16) {
                            std::memcpy(dst + i, src + i, 16);
                        }
                        return;
                    }
                    if (d >= 8) {
                        for (size_t i = 0; i < n; i += 8) {
                            std::memcpy(dst + i, src + i, 8);
                        }
                        return;
                    }
                }
                if (d == 1) {
                    std::memset(dst, *src, n);
                    return;
                }
                if (d >= n) {
                    std::memcpy(dst, src, n);
                    return;
                }
                size_t i = 0;
                if (d >= 8) {
                    for (; i + 8 <= n; i += 8) {
                        std::memcpy(dst + i, src + i, 8);
                    }
                }
                for (; i < n; ++i) {
                    dst[i] = src[i];
                }
                return;
            }
            // the circular window: the source goes round its end
            size_t from = pos + size - d;
            for (size_t i = 0; i < n; ++i) {
                window[pos++] = window[from++];
                if (from == size) {
                    from = 0;
                }
            }
        }

        // Symbols straight from the input while at least Margin bytes of
        // it remain (in_stop) and the room takes the longest match (stop)
        template<bool Linear>
        LzmaStatus _fast(uint8_t* window, size_t size, size_t& pos_ref, size_t stop, const uint8_t*& in_ref, const uint8_t* in_stop) {
            Real r{_range, _code, in_ref};
            size_t pos = pos_ref;
            uint32_t state = _state;
            uint32_t rep0 = _rep[0], rep1 = _rep[1], rep2 = _rep[2], rep3 = _rep[3];
            const uint64_t base = _total - pos;   // the total at window index 0
            const uint32_t pb_mask = (1u << _props.pb) - 1;
            const uint32_t lp_mask = (1u << _props.lp) - 1;
            const uint32_t lc = _props.lc;
            const uint64_t dictionary = std::min<uint64_t>(_dictionary, size);   // no farther than the window reaches
            uint16_t* const literal = _literal.get();
            LzmaProbs& p = _probs;
            LzmaStatus result = LzmaStatus::need_input;
            auto back = [&](uint32_t dist) SGCL_LZMA_INLINE_LAMBDA -> uint8_t {
                if constexpr (Linear) {
                    return window[pos - dist - 1];
                } else {
                    return _back(window, size, pos, dist);
                }
            };
            while (pos < stop && r.in <= in_stop) {
                uint64_t total = base + pos;
                uint32_t ps = uint32_t(total) & pb_mask;
                if (!r.bit(p.is_match[state][ps])) {
                    uint32_t prev = total ? back(0) : 0;
                    uint16_t* lit = literal + size_t(0x300) * (((uint32_t(total) & lp_mask) << lc) + (prev >> (8 - lc)));
                    uint32_t s = 1;
                    if (state < lzma_model::LiteralStates) {
                        do s = (s << 1) | r.literal_bit(lit[s]); while (s < 0x100);
                    } else {
                        uint32_t match = back(rep0);
                        uint32_t offs = 0x100;
                        do {
                            match <<= 1;
                            uint32_t mb = match & offs;
                            uint32_t b = r.literal_bit(lit[offs + mb + s]);
                            s = (s << 1) | b;
                            offs &= b ? mb : ~mb;
                        } while (s < 0x100);
                    }
                    window[pos++] = uint8_t(s);
                    state = lzma_model::after_literal(state);
                    continue;
                }
                uint32_t len;
                if (r.bit(p.is_rep[state])) {
                    if (SGCL_LZMA_UNLIKELY(total == 0)) {
                        result = _fail(errc::corrupt, "lzma: a repeated match before any data");
                        break;
                    }
                    if (!r.bit(p.is_rep_g0[state])) {
                        if (!r.bit(p.is_rep0_long[state][ps])) {
                            window[pos] = back(rep0);
                            ++pos;
                            state = lzma_model::after_short_rep(state);
                            continue;
                        }
                    } else {
                        uint32_t d;
                        if (!r.bit(p.is_rep_g1[state])) {
                            d = rep1;
                        } else {
                            if (!r.bit(p.is_rep_g2[state])) {
                                d = rep2;
                            } else {
                                d = rep3;
                                rep3 = rep2;
                            }
                            rep2 = rep1;
                        }
                        rep1 = rep0;
                        rep0 = d;
                    }
                    len = _length(r, p.rep_len, ps);
                    state = lzma_model::after_rep(state);
                } else {
                    len = _length(r, p.len, ps);
                    uint32_t dist = _distance(r, len);
                    if (SGCL_LZMA_UNLIKELY(dist >= dictionary || dist >= total)) {
                        if (dist == lzma_model::EndMarker) {
                            _total = total;
                            if (r.code != 0) {
                                result = _fail(errc::corrupt, "lzma: corrupt data at the end marker");
                            } else if (_size != UINT64_MAX && total != _size) {
                                result = _fail(errc::corrupt, "lzma: the end marker before the size in the header");
                            } else {
                                _ended = true;
                                result = LzmaStatus::done;
                            }
                        } else {
                            result = _fail(errc::corrupt, "lzma: a distance past the start of the data");
                        }
                        break;
                    }
                    rep3 = rep2;
                    rep2 = rep1;
                    rep1 = rep0;
                    rep0 = dist;
                    state = lzma_model::after_match(state);
                }
                _copy_match<Linear>(window, size, pos, rep0, len);
            }
            _total = base + pos;
            _state = uint8_t(state);
            _rep[0] = rep0;
            _rep[1] = rep1;
            _rep[2] = rep2;
            _rep[3] = rep3;
            _taken += uint64_t(r.in - in_ref);
            in_ref = r.in;
            _range = r.range;
            _code = r.code;
            pos_ref = pos;
            return result;
        }

        LzmaStatus _fail(errc code, const char* text) noexcept {
            _failed = true;
            error = code;
            error_text = text;
            return LzmaStatus::failed;
        }

        LzmaProperties _props;
        uint32_t _dictionary = lzma_model::DictionaryMin;
        LzmaProbs _probs;
        std::unique_ptr<uint16_t[]> _literal;
        size_t _literal_count = 0;
        uint8_t _state = 0;
        uint32_t _rep[4] = {0, 0, 0, 0};
        uint32_t _range = 0xFFFFFFFF;
        uint32_t _code = 0;
        uint32_t _pending = 0;   // bytes of the last match not yet copied (the room ran out)
        uint64_t _total = 0;
        uint64_t _size = UINT64_MAX;
        uint64_t _taken = 0;
        bool _started = false;
        bool _ended = false;
        bool _failed = false;
        bool _expect_marker = false;   // the size known reached with bytes of the coder left: the marker must come
        bool _lzma2 = false;           // chunks of LZMA2: no marker, the coder ends at every chunk's size
        uint8_t _tmp[Margin];
        size_t _tmp_size = 0;
    };
}
