//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "lzma_decoder.h"
#include "range_coder.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace sgcl::compress::detail {
    // PPMd variant H (Dmitry Shkarin's "PPMd var.H", 2001), as 7z uses it
    // (method 03 04 01, properties: the order in one byte and the memory
    // in four). Prediction by partial matching: every context of up to
    // `order` bytes that has been seen keeps the symbols that followed it
    // with their frequencies; a symbol is coded in the longest context
    // that has it, escaping to shorter ones (each shorter context's
    // symbols already tried being masked out) until it is found, or down
    // to the empty context of all 256. A context of one symbol codes it
    // with an adaptive binary probability (BinSumm); the escape of the
    // others is estimated by secondary escape estimation (SEE). The model
    // lives in one block of plain memory: the text of recent bytes grows
    // from its bottom, contexts (12 bytes) and arrays of states (6 bytes
    // each, in units of 12) are taken from its top by a sub-allocator of
    // 38 sizes with free lists that glues neighbouring free blocks when a
    // size runs out; when the memory is spent the model starts over.
    // References inside the block are 32-bit offsets from its start. The
    // coder is 7z's range coder (the range divided by the total, two
    // renormalisation steps at most).
    //
    // A decoder must build exactly the model the encoder built — the
    // allocator's success and failure included, which decide when the
    // model restarts — so the rules below (the unit table, the rounding
    // of every frequency update, the glue) are the format itself; the
    // code is this library's.
    namespace ppmd {
        constexpr uint32_t UnitSize = 12;
        constexpr uint32_t Indexes = 38;
        constexpr uint32_t MaxFreq = 124;
        constexpr uint32_t IntBits = 7;
        constexpr uint32_t PeriodBits = 7;
        constexpr uint32_t BinScale = uint32_t(1) << (IntBits + PeriodBits);
        constexpr uint32_t MinOrder = 2;
        constexpr uint32_t MaxOrder = 64;
        constexpr uint32_t MinMemory = uint32_t(1) << 11;
        constexpr uint32_t MaxMemory = 0xFFFFFFFFu - 12 * 3;
        constexpr uint16_t InitBinEsc[8] = {0x3CDD, 0x1F3F, 0x59BF, 0x48F3, 0x64A1, 0x5ABC, 0x6632, 0x6051};
        constexpr uint8_t ExpEscape[16] = {25, 14, 9, 7, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2};

        struct State {
            uint8_t symbol;
            uint8_t freq;
            uint16_t successor_low;
            uint16_t successor_high;
        };

        struct Context {
            uint16_t num_stats;
            uint16_t summ_freq;
            uint32_t stats;
            uint32_t suffix;
        };

        struct See {
            uint16_t summ;
            uint8_t shift;
            uint8_t count;
        };

        // A free block while the allocator glues: its units and neighbours
        struct Node {
            uint16_t stamp;
            uint16_t nu;
            uint32_t next;
            uint32_t prev;
        };

        static_assert(sizeof(State) == 6 && sizeof(Context) == 12 && sizeof(Node) == 12);

        inline uint32_t mean(uint32_t prob) noexcept {
            return (prob + (uint32_t(1) << (PeriodBits - 2))) >> PeriodBits;
        }
    }

    // The range decoder of 7z's PPMd over bytes at hand; a read past their
    // end gives 0 and is counted
    struct Ppmd7RangeDecoder {
        uint32_t range = 0xFFFFFFFF;
        uint32_t code = 0;
        const uint8_t* in = nullptr;
        const uint8_t* end = nullptr;
        uint32_t overrun = 0;

        SGCL_LZMA_INLINE uint32_t next() noexcept {
            if (in < end) {
                return *in++;
            }
            ++overrun;
            return 0;
        }

        SGCL_LZMA_INLINE void normalize() noexcept {
            if (range < rc::Top) {
                code = (code << 8) | next();
                range <<= 8;
                if (range < rc::Top) {
                    code = (code << 8) | next();
                    range <<= 8;
                }
            }
        }

        SGCL_LZMA_INLINE uint32_t threshold(uint32_t total) noexcept {
            range /= total;
            return code / range;
        }

        SGCL_LZMA_INLINE void decode(uint32_t start, uint32_t size) noexcept {
            code -= start * range;
            range *= size;
            normalize();
        }

        SGCL_LZMA_INLINE uint32_t bit(uint32_t size0) noexcept {
            uint32_t bound = (range >> 14) * size0;
            uint32_t b;
            if (code < bound) {
                range = bound;
                b = 0;
            } else {
                code -= bound;
                range -= bound;
                b = 1;
            }
            normalize();
            return b;
        }
    };

    class Ppmd7 {
    public:
        Ppmd7() {
            for (uint32_t i = 0, k = 0; i < ppmd::Indexes; ++i) {
                uint32_t step = i >= 12 ? 4 : (i >> 2) + 1;
                do {
                    _units_index[k++] = uint8_t(i);
                } while (--step);
                _index_units[i] = uint8_t(k);
            }
            _ns_bin[0] = 0;
            _ns_bin[1] = 2;
            std::memset(_ns_bin + 2, 4, 9);
            std::memset(_ns_bin + 11, 6, 256 - 11);
            for (uint32_t i = 0; i < 3; ++i) {
                _ns_index[i] = uint8_t(i);
            }
            for (uint32_t i = 3, m = 3, k = 1; i < 256; ++i) {
                _ns_index[i] = uint8_t(m);
                if (--k == 0) {
                    k = ++m - 2;
                }
            }
            std::memset(_hb_flag, 0, 0x40);
            std::memset(_hb_flag + 0x40, 8, 0x100 - 0x40);
        }

        // How often the model started over and the allocator glued its free
        // blocks (the first start counted): for the tests
        uint32_t restarts = 0;
        uint32_t glues = 0;

        Ppmd7(const Ppmd7&) = delete;
        Ppmd7& operator=(const Ppmd7&) = delete;

        // The block for a model of `size` bytes (the memory of the
        // properties), kept when a model of the same size had it
        void allocate(uint32_t size) {
            if (!_memory || _size != size) {
                _align = 4 - (size & 3);
                // the glue's sentinel node lies just past the model's memory
                _memory.reset(new uint8_t[size_t(_align) + size + ppmd::UnitSize]);
                _size = size;
            }
        }

        void init(uint32_t order) {
            _max_order = order;
            _restart();
            _dummy_see.shift = ppmd::PeriodBits;
            _dummy_see.summ = 0;
            _dummy_see.count = 64;
        }

        // A symbol, or -1 for the end marker, -2 for data the model cannot
        // have made
        int decode(Ppmd7RangeDecoder& rc) {
            int8_t mask[256];
            if (_min->num_stats != 1) {
                ppmd::State* s = _stats(_min);
                uint32_t count = rc.threshold(_min->summ_freq);
                uint32_t hi = s->freq;
                if (count < hi) {
                    rc.decode(0, s->freq);
                    _found = s;
                    uint8_t symbol = s->symbol;
                    _update1_0();
                    return symbol;
                }
                _prev_success = 0;
                uint32_t i = _min->num_stats - 1;
                do {
                    if ((hi += (++s)->freq) > count) {
                        rc.decode(hi - s->freq, s->freq);
                        _found = s;
                        uint8_t symbol = s->symbol;
                        _update1();
                        return symbol;
                    }
                } while (--i);
                if (count >= _min->summ_freq) {
                    return -2;
                }
                _hi_bits = _hb_flag[_found->symbol];
                rc.decode(hi, _min->summ_freq - hi);
                std::memset(mask, -1, sizeof(mask));
                mask[s->symbol] = 0;
                i = _min->num_stats - 1;
                do {
                    mask[(--s)->symbol] = 0;
                } while (--i);
            } else {
                uint16_t* prob = _bin_summ();
                if (rc.bit(*prob) == 0) {
                    *prob = uint16_t(*prob + (1 << ppmd::IntBits) - ppmd::mean(*prob));
                    _found = _one_state(_min);
                    uint8_t symbol = _found->symbol;
                    _update_bin();
                    return symbol;
                }
                *prob = uint16_t(*prob - ppmd::mean(*prob));
                _init_esc = ppmd::ExpEscape[*prob >> 10];
                std::memset(mask, -1, sizeof(mask));
                mask[_one_state(_min)->symbol] = 0;
                _prev_success = 0;
            }
            for (;;) {
                ppmd::State* ps[256];
                uint32_t masked = _min->num_stats;
                do {
                    ++_order_fall;
                    if (!_min->suffix) {
                        return -1;
                    }
                    _min = _ctx(_min->suffix);
                } while (_min->num_stats == masked);
                uint32_t hi = 0;
                ppmd::State* s = _stats(_min);
                uint32_t i = 0;
                uint32_t num = _min->num_stats - masked;
                do {
                    int k = mask[s->symbol];
                    hi += s->freq & uint32_t(k);
                    ps[i] = s++;
                    i -= uint32_t(k);
                } while (i != num);
                uint32_t esc;
                ppmd::See* see = _esc_freq(masked, esc);
                uint32_t total = esc + hi;
                uint32_t count = rc.threshold(total);
                if (count < hi) {
                    ppmd::State** pp = ps;
                    for (hi = 0; (hi += (*pp)->freq) <= count; ++pp) {
                    }
                    s = *pp;
                    rc.decode(hi - s->freq, s->freq);
                    _see_update(see);
                    _found = s;
                    uint8_t symbol = s->symbol;
                    _update2();
                    return symbol;
                }
                if (count >= total) {
                    return -2;
                }
                rc.decode(hi, total - hi);
                see->summ = uint16_t(see->summ + total);
                do {
                    mask[ps[--i]->symbol] = 0;
                } while (i != 0);
            }
        }

        // A symbol, or -1 for the end marker
        void encode(RangeEncoder& rc, int symbol) {
            int8_t mask[256];
            if (_min->num_stats != 1) {
                ppmd::State* s = _stats(_min);
                if (s->symbol == symbol) {
                    rc.encode(0, s->freq, _min->summ_freq);
                    _found = s;
                    _update1_0();
                    return;
                }
                _prev_success = 0;
                uint32_t sum = s->freq;
                uint32_t i = _min->num_stats - 1;
                do {
                    if ((++s)->symbol == symbol) {
                        rc.encode(sum, s->freq, _min->summ_freq);
                        _found = s;
                        _update1();
                        return;
                    }
                    sum += s->freq;
                } while (--i);
                _hi_bits = _hb_flag[_found->symbol];
                std::memset(mask, -1, sizeof(mask));
                mask[s->symbol] = 0;
                i = _min->num_stats - 1;
                do {
                    mask[(--s)->symbol] = 0;
                } while (--i);
                rc.encode(sum, _min->summ_freq - sum, _min->summ_freq);
            } else {
                uint16_t* prob = _bin_summ();
                ppmd::State* s = _one_state(_min);
                if (s->symbol == symbol) {
                    rc.encode_bit(0, *prob, 14);
                    *prob = uint16_t(*prob + (1 << ppmd::IntBits) - ppmd::mean(*prob));
                    _found = s;
                    _update_bin();
                    return;
                }
                rc.encode_bit(1, *prob, 14);
                *prob = uint16_t(*prob - ppmd::mean(*prob));
                _init_esc = ppmd::ExpEscape[*prob >> 10];
                std::memset(mask, -1, sizeof(mask));
                mask[s->symbol] = 0;
                _prev_success = 0;
            }
            for (;;) {
                uint32_t masked = _min->num_stats;
                do {
                    ++_order_fall;
                    if (!_min->suffix) {
                        return;   // the end marker: escaped from the empty context
                    }
                    _min = _ctx(_min->suffix);
                } while (_min->num_stats == masked);
                uint32_t esc;
                ppmd::See* see = _esc_freq(masked, esc);
                ppmd::State* s = _stats(_min);
                uint32_t sum = 0;
                uint32_t i = _min->num_stats;
                do {
                    int cur = s->symbol;
                    if (cur == symbol) {
                        uint32_t low = sum;
                        ppmd::State* found = s;
                        do {
                            sum += s->freq & uint32_t(int(mask[s->symbol]));
                            ++s;
                        } while (--i);
                        rc.encode(low, found->freq, sum + esc);
                        _see_update(see);
                        _found = found;
                        _update2();
                        return;
                    }
                    sum += s->freq & uint32_t(int(mask[cur]));
                    mask[cur] = 0;
                    ++s;
                } while (--i);
                rc.encode(sum, esc, sum + esc);
                see->summ = uint16_t(see->summ + sum + esc);
            }
        }

    private:
        using State = ppmd::State;
        using Context = ppmd::Context;

        // --- the block and its references

        uint8_t* _base() const noexcept {
            return _memory.get();
        }

        uint32_t _ref(const void* p) const noexcept {
            return uint32_t(static_cast<const uint8_t*>(p) - _base());
        }

        Context* _ctx(uint32_t ref) const noexcept {
            return reinterpret_cast<Context*>(_base() + ref);
        }

        State* _stats(const Context* c) const noexcept {
            return reinterpret_cast<State*>(_base() + c->stats);
        }

        // The state of a context of one symbol lives in the context itself,
        // over its summ_freq and stats
        static State* _one_state(Context* c) noexcept {
            return reinterpret_cast<State*>(reinterpret_cast<uint8_t*>(c) + 2);
        }

        static uint32_t _successor(const State* s) noexcept {
            return uint32_t(s->successor_low) | uint32_t(s->successor_high) << 16;
        }

        static void _set_successor(State* s, uint32_t v) noexcept {
            s->successor_low = uint16_t(v);
            s->successor_high = uint16_t(v >> 16);
        }

        ppmd::Node* _node(uint32_t ref) const noexcept {
            return reinterpret_cast<ppmd::Node*>(_base() + ref);
        }

        // --- the sub-allocator

        uint32_t _i2u(uint32_t index) const noexcept {
            return _index_units[index];
        }

        uint32_t _u2i(uint32_t units) const noexcept {
            return _units_index[units - 1];
        }

        static uint32_t _u2b(uint32_t units) noexcept {
            return units * ppmd::UnitSize;
        }

        void _insert(void* p, uint32_t index) noexcept {
            std::memcpy(p, &_free[index], 4);
            _free[index] = _ref(p);
        }

        void* _remove(uint32_t index) noexcept {
            uint8_t* p = _base() + _free[index];
            std::memcpy(&_free[index], p, 4);
            return p;
        }

        void _split(void* p, uint32_t old_index, uint32_t new_index) noexcept {
            uint32_t nu = _i2u(old_index) - _i2u(new_index);
            uint8_t* q = static_cast<uint8_t*>(p) + _u2b(_i2u(new_index));
            uint32_t i = _u2i(nu);
            if (_i2u(i) != nu) {
                uint32_t k = _i2u(--i);
                _insert(q + _u2b(k), nu - k - 1);
            }
            _insert(q, i);
        }

        // The free blocks joined with the free blocks after them, and put
        // back on the lists by size
        void _glue() noexcept {
            ++glues;
            uint32_t head = _align + _size;
            uint32_t n = head;
            _glue_count = 255;
            for (uint32_t i = 0; i < ppmd::Indexes; ++i) {
                uint16_t nu = uint16_t(_i2u(i));
                uint32_t next = _free[i];
                _free[i] = 0;
                while (next != 0) {
                    ppmd::Node* node = _node(next);
                    node->next = n;
                    _node(n)->prev = next;
                    n = next;
                    std::memcpy(&next, node, 4);
                    node->stamp = 0;
                    node->nu = nu;
                }
            }
            _node(head)->stamp = 1;
            _node(head)->next = n;
            _node(n)->prev = head;
            if (_lo != _hi) {
                reinterpret_cast<ppmd::Node*>(_lo)->stamp = 1;
            }
            while (n != head) {
                ppmd::Node* node = _node(n);
                uint32_t nu = node->nu;
                for (;;) {
                    ppmd::Node* next = node + nu;
                    nu += next->nu;
                    if (next->stamp != 0 || nu >= 0x10000) {
                        break;
                    }
                    _node(next->prev)->next = next->next;
                    _node(next->next)->prev = next->prev;
                    node->nu = uint16_t(nu);
                }
                n = node->next;
            }
            for (n = _node(head)->next; n != head;) {
                ppmd::Node* node = _node(n);
                uint32_t next = node->next;
                uint32_t nu = node->nu;
                for (; nu > 128; nu -= 128, node += 128) {
                    _insert(node, ppmd::Indexes - 1);
                }
                uint32_t i = _u2i(nu);
                if (_i2u(i) != nu) {
                    uint32_t k = _i2u(--i);
                    _insert(node + k, nu - k - 1);
                }
                _insert(node, i);
                n = next;
            }
        }

        void* _alloc_rare(uint32_t index) noexcept {
            if (_glue_count == 0) {
                _glue();
                if (_free[index] != 0) {
                    return _remove(index);
                }
            }
            uint32_t i = index;
            do {
                if (++i == ppmd::Indexes) {
                    uint32_t bytes = _u2b(_i2u(index));
                    --_glue_count;
                    if (uint32_t(_units_start - _text) > bytes) {
                        _units_start -= bytes;
                        return _units_start;
                    }
                    return nullptr;
                }
            } while (_free[i] == 0);
            void* p = _remove(i);
            _split(p, i, index);
            return p;
        }

        void* _alloc_units(uint32_t index) noexcept {
            if (_free[index] != 0) {
                return _remove(index);
            }
            uint32_t bytes = _u2b(_i2u(index));
            if (bytes <= uint32_t(_hi - _lo)) {
                void* p = _lo;
                _lo += bytes;
                return p;
            }
            return _alloc_rare(index);
        }

        void* _shrink_units(void* old, uint32_t old_nu, uint32_t new_nu) noexcept {
            uint32_t i0 = _u2i(old_nu);
            uint32_t i1 = _u2i(new_nu);
            if (i0 == i1) {
                return old;
            }
            if (_free[i1] != 0) {
                void* p = _remove(i1);
                std::memcpy(p, old, _u2b(new_nu));
                _insert(old, i0);
                return p;
            }
            _split(old, i0, i1);
            return old;
        }

        // --- the model

        void _restart() {
            ++restarts;
            std::memset(_free, 0, sizeof(_free));
            _text = _base() + _align;
            _hi = _text + _size;
            _lo = _units_start = _hi - _size / 8 / ppmd::UnitSize * 7 * ppmd::UnitSize;
            _glue_count = 0;
            _order_fall = _max_order;
            _run_length = _init_rl = -int32_t(_max_order < 12 ? _max_order : 12) - 1;
            _prev_success = 0;
            _hi -= ppmd::UnitSize;
            _min = _max = reinterpret_cast<Context*>(_hi);
            _min->suffix = 0;
            _min->num_stats = 256;
            _min->summ_freq = 256 + 1;
            _found = reinterpret_cast<State*>(_lo);
            _lo += _u2b(256 / 2);
            _min->stats = _ref(_found);
            for (uint32_t i = 0; i < 256; ++i) {
                State* s = &_found[i];
                s->symbol = uint8_t(i);
                s->freq = 1;
                _set_successor(s, 0);
            }
            for (uint32_t i = 0; i < 128; ++i) {
                for (uint32_t k = 0; k < 8; ++k) {
                    uint16_t v = uint16_t(ppmd::BinScale - ppmd::InitBinEsc[k] / (i + 2));
                    for (uint32_t m = 0; m < 64; m += 8) {
                        _bin[i][k + m] = v;
                    }
                }
            }
            for (uint32_t i = 0; i < 25; ++i) {
                for (uint32_t k = 0; k < 16; ++k) {
                    ppmd::See& s = _see[i][k];
                    s.shift = ppmd::PeriodBits - 4;
                    s.summ = uint16_t((5 * i + 10) << s.shift);
                    s.count = 4;
                }
            }
        }

        // The contexts of the found symbol's successors from the current one
        // up, made where they are still text; nullptr when the memory is spent
        Context* _create_successors(bool skip) noexcept {
            Context* c = _min;
            uint32_t up_branch = _successor(_found);
            State* ps[ppmd::MaxOrder];
            uint32_t n = 0;
            if (!skip) {
                ps[n++] = _found;
            }
            while (c->suffix) {
                c = _ctx(c->suffix);
                State* s;
                if (c->num_stats != 1) {
                    for (s = _stats(c); s->symbol != _found->symbol; ++s) {
                    }
                } else {
                    s = _one_state(c);
                }
                uint32_t successor = _successor(s);
                if (successor != up_branch) {
                    c = _ctx(successor);
                    if (n == 0) {
                        return c;
                    }
                    break;
                }
                ps[n++] = s;
            }
            State up;
            up.symbol = _base()[up_branch];
            _set_successor(&up, up_branch + 1);
            if (c->num_stats == 1) {
                up.freq = _one_state(c)->freq;
            } else {
                State* s;
                for (s = _stats(c); s->symbol != up.symbol; ++s) {
                }
                uint32_t cf = s->freq - 1u;
                uint32_t s0 = c->summ_freq - c->num_stats - cf;
                up.freq = uint8_t(1 + ((2 * cf <= s0) ? (5 * cf > s0) : ((2 * cf + 3 * s0 - 1) / (2 * s0))));
            }
            do {
                Context* c1;
                if (_hi != _lo) {
                    _hi -= ppmd::UnitSize;
                    c1 = reinterpret_cast<Context*>(_hi);
                } else if (_free[0] != 0) {
                    c1 = static_cast<Context*>(_remove(0));
                } else {
                    c1 = static_cast<Context*>(_alloc_rare(0));
                    if (!c1) {
                        return nullptr;
                    }
                }
                c1->num_stats = 1;
                *_one_state(c1) = up;
                c1->suffix = _ref(c);
                _set_successor(ps[--n], _ref(c1));
                c = c1;
            } while (n != 0);
            return c;
        }

        void _update_model() noexcept {
            uint32_t f_successor = _successor(_found);
            if (_found->freq < ppmd::MaxFreq / 4 && _min->suffix != 0) {
                Context* c = _ctx(_min->suffix);
                if (c->num_stats == 1) {
                    State* s = _one_state(c);
                    if (s->freq < 32) {
                        ++s->freq;
                    }
                } else {
                    State* s = _stats(c);
                    if (s->symbol != _found->symbol) {
                        do {
                            ++s;
                        } while (s->symbol != _found->symbol);
                        if (s[0].freq >= s[-1].freq) {
                            std::swap(s[0], s[-1]);
                            --s;
                        }
                    }
                    if (s->freq < ppmd::MaxFreq - 9) {
                        s->freq = uint8_t(s->freq + 2);
                        c->summ_freq = uint16_t(c->summ_freq + 2);
                    }
                }
            }
            if (_order_fall == 0) {
                _min = _max = _create_successors(true);
                if (!_min) {
                    _restart();
                    return;
                }
                _set_successor(_found, _ref(_min));
                return;
            }
            *_text++ = _found->symbol;
            uint32_t successor = _ref(_text);
            if (_text >= _units_start) {
                _restart();
                return;
            }
            if (f_successor) {
                if (f_successor <= successor) {
                    Context* cs = _create_successors(false);
                    if (!cs) {
                        _restart();
                        return;
                    }
                    f_successor = _ref(cs);
                }
                if (--_order_fall == 0) {
                    successor = f_successor;
                    _text -= (_max != _min);
                }
            } else {
                _set_successor(_found, successor);
                f_successor = _ref(_min);
            }
            uint32_t ns = _min->num_stats;
            uint32_t s0 = _min->summ_freq - ns - (_found->freq - 1u);
            for (Context* c = _max; c != _min; c = _ctx(c->suffix)) {
                uint32_t ns1 = c->num_stats;
                if (ns1 != 1) {
                    if ((ns1 & 1) == 0) {
                        // one more unit for the next state
                        uint32_t old_nu = ns1 >> 1;
                        uint32_t i = _u2i(old_nu);
                        if (i != _u2i(old_nu + 1)) {
                            void* p = _alloc_units(i + 1);
                            if (!p) {
                                _restart();
                                return;
                            }
                            void* old = _stats(c);
                            std::memcpy(p, old, _u2b(old_nu));
                            _insert(old, i);
                            c->stats = _ref(p);
                        }
                    }
                    c->summ_freq = uint16_t(c->summ_freq + (2 * ns1 < ns) + 2 * ((4 * ns1 <= ns) & (c->summ_freq <= 8 * ns1)));
                } else {
                    State* s = static_cast<State*>(_alloc_units(0));
                    if (!s) {
                        _restart();
                        return;
                    }
                    *s = *_one_state(c);
                    c->stats = _ref(s);
                    if (s->freq < ppmd::MaxFreq / 4 - 1) {
                        s->freq = uint8_t(s->freq << 1);
                    } else {
                        s->freq = ppmd::MaxFreq - 4;
                    }
                    c->summ_freq = uint16_t(s->freq + _init_esc + (ns > 3));
                }
                uint32_t cf = 2 * uint32_t(_found->freq) * (c->summ_freq + 6u);
                uint32_t sf = s0 + c->summ_freq;
                if (cf < 6 * sf) {
                    cf = 1 + (cf > sf) + (cf >= 4 * sf);
                    c->summ_freq = uint16_t(c->summ_freq + 3);
                } else {
                    cf = 4 + (cf >= 9 * sf) + (cf >= 12 * sf) + (cf >= 15 * sf);
                    c->summ_freq = uint16_t(c->summ_freq + cf);
                }
                State* s = _stats(c) + ns1;
                _set_successor(s, successor);
                s->symbol = _found->symbol;
                s->freq = uint8_t(cf);
                c->num_stats = uint16_t(ns1 + 1);
            }
            _max = _min = _ctx(f_successor);
        }

        // The frequencies of the current context halved (after the found
        // state moved to the front); states that fall to 0 dropped
        void _rescale() noexcept {
            State* stats = _stats(_min);
            State* s = _found;
            {
                State tmp = *s;
                for (; s != stats; --s) {
                    s[0] = s[-1];
                }
                *s = tmp;
            }
            uint32_t esc = _min->summ_freq - s->freq;
            s->freq = uint8_t(s->freq + 4);
            uint32_t adder = _order_fall != 0;
            s->freq = uint8_t((s->freq + adder) >> 1);
            uint32_t sum = s->freq;
            uint32_t i = _min->num_stats - 1u;
            do {
                esc -= (++s)->freq;
                s->freq = uint8_t((s->freq + adder) >> 1);
                sum += s->freq;
                if (s[0].freq > s[-1].freq) {
                    State* s1 = s;
                    State tmp = *s1;
                    do {
                        s1[0] = s1[-1];
                    } while (--s1 != stats && tmp.freq > s1[-1].freq);
                    *s1 = tmp;
                }
            } while (--i);
            if (s->freq == 0) {
                uint32_t num_stats = _min->num_stats;
                do {
                    ++i;
                } while ((--s)->freq == 0);
                esc += i;
                _min->num_stats = uint16_t(_min->num_stats - i);
                if (_min->num_stats == 1) {
                    State tmp = *stats;
                    do {
                        tmp.freq = uint8_t(tmp.freq - (tmp.freq >> 1));
                        esc >>= 1;
                    } while (esc > 1);
                    _insert(stats, _u2i((num_stats + 1) >> 1));
                    *(_found = _one_state(_min)) = tmp;
                    return;
                }
                uint32_t n0 = (num_stats + 1) >> 1;
                uint32_t n1 = (_min->num_stats + 1u) >> 1;
                if (n0 != n1) {
                    _min->stats = _ref(_shrink_units(stats, n0, n1));
                }
            }
            _min->summ_freq = uint16_t(sum + esc - (esc >> 1));
            _found = _stats(_min);
        }

        ppmd::See* _esc_freq(uint32_t masked, uint32_t& esc) noexcept {
            uint32_t non_masked = _min->num_stats - masked;
            if (_min->num_stats != 256) {
                ppmd::See* see = _see[_ns_index[non_masked - 1]] +
                    (non_masked < uint32_t(_ctx(_min->suffix)->num_stats) - _min->num_stats) +
                    2 * (_min->summ_freq < 11 * _min->num_stats) +
                    4 * (masked > non_masked) +
                    _hi_bits;
                uint32_t r = see->summ >> see->shift;
                see->summ = uint16_t(see->summ - r);
                esc = r + (r == 0);
                return see;
            }
            esc = 1;
            return &_dummy_see;
        }

        static void _see_update(ppmd::See* see) noexcept {
            if (see->shift < ppmd::PeriodBits && --see->count == 0) {
                see->summ = uint16_t(see->summ << 1);
                see->count = uint8_t(3 << see->shift++);
            }
        }

        uint16_t* _bin_summ() noexcept {
            State* one = _one_state(_min);
            _hi_bits = _hb_flag[_found->symbol];
            return &_bin[one->freq - 1][_prev_success + _ns_bin[_ctx(_min->suffix)->num_stats - 1] + _hi_bits + 2 * _hb_flag[one->symbol] + ((_run_length >> 26) & 0x20)];
        }

        void _next_context() noexcept {
            uint32_t successor = _successor(_found);
            if (_order_fall == 0 && _base() + successor > _text) {
                _min = _max = _ctx(successor);
            } else {
                _update_model();
            }
        }

        void _update1() noexcept {
            State* s = _found;
            s->freq = uint8_t(s->freq + 4);
            _min->summ_freq = uint16_t(_min->summ_freq + 4);
            if (s[0].freq > s[-1].freq) {
                std::swap(s[0], s[-1]);
                _found = --s;
                if (s->freq > ppmd::MaxFreq) {
                    _rescale();
                }
            }
            _next_context();
        }

        void _update1_0() noexcept {
            _prev_success = 2u * _found->freq > _min->summ_freq;
            _run_length += int32_t(_prev_success);
            _min->summ_freq = uint16_t(_min->summ_freq + 4);
            if ((_found->freq = uint8_t(_found->freq + 4)) > ppmd::MaxFreq) {
                _rescale();
            }
            _next_context();
        }

        void _update_bin() noexcept {
            _found->freq = uint8_t(_found->freq + (_found->freq < 128 ? 1 : 0));
            _prev_success = 1;
            ++_run_length;
            _next_context();
        }

        void _update2() noexcept {
            _found->freq = uint8_t(_found->freq + 4);
            _min->summ_freq = uint16_t(_min->summ_freq + 4);
            if (_found->freq > ppmd::MaxFreq) {
                _rescale();
            }
            _run_length = _init_rl;
            _update_model();
        }

        std::unique_ptr<uint8_t[]> _memory;
        uint32_t _size = 0;
        uint32_t _align = 4;
        uint8_t* _text = nullptr;
        uint8_t* _units_start = nullptr;
        uint8_t* _lo = nullptr;
        uint8_t* _hi = nullptr;
        uint32_t _glue_count = 0;
        uint32_t _free[ppmd::Indexes] = {};
        Context* _min = nullptr;
        Context* _max = nullptr;
        State* _found = nullptr;
        uint32_t _order_fall = 0;
        uint32_t _init_esc = 0;
        uint32_t _prev_success = 0;
        uint32_t _max_order = 6;
        uint32_t _hi_bits = 0;
        int32_t _run_length = 0;
        int32_t _init_rl = 0;
        uint8_t _index_units[ppmd::Indexes];
        uint8_t _units_index[128];
        uint8_t _ns_index[256];
        uint8_t _ns_bin[256];
        uint8_t _hb_flag[256];
        ppmd::See _dummy_see{};
        ppmd::See _see[25][16];
        uint16_t _bin[128][64];
    };

    // The decoder: `size` bytes (UINT64_MAX: to the end marker) out of the
    // coder's data, a symbol at a time; a symbol needs at most two bytes a
    // context it escapes through, so the decoder waits for Margin bytes of
    // input before each one (or for the end of the input)
    class Ppmd7Decoder {
    public:
        static constexpr size_t Margin = 2 * (ppmd::MaxOrder + 4) + 8;

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        // The model's memory checked by the caller against its limits
        void reset(uint32_t order, uint32_t memory, uint64_t size) {
            _model.allocate(memory);
            _model.init(order);
            _size = size;
            _done = 0;
            _started = false;
            _failed = false;
            _ended = false;
            _taken = 0;
            _rc = Ppmd7RangeDecoder();
        }

        uint64_t taken() const noexcept {
            return _taken;
        }

        const Ppmd7& model() const noexcept {
            return _model;
        }

        // After the last symbol: the range coder at zero, as the encoder's
        // flush leaves it (7-Zip's check of a stream that ends where it should)
        bool finished_ok() const noexcept {
            return _rc.code == 0;
        }

        // Into out[pos, limit), from [in, end): need_input leaves in where
        // it was when fewer than Margin bytes are there and more may come
        LzmaStatus decode(uint8_t* out, size_t& pos, size_t limit, const uint8_t*& in, const uint8_t* end, bool input_ended) {
            if (_failed) {
                return LzmaStatus::failed;
            }
            if (_ended) {
                return LzmaStatus::done;
            }
            if (!_started) {
                if (size_t(end - in) < 5) {
                    return input_ended ? _fail(errc::unexpected_end, "ppmd: unexpected end of the compressed data") : LzmaStatus::need_input;
                }
                if (in[0] != 0) {
                    return _fail(errc::corrupt, "ppmd: the range coder's first byte is not 0");
                }
                _rc.code = uint32_t(in[1]) << 24 | uint32_t(in[2]) << 16 | uint32_t(in[3]) << 8 | in[4];
                _rc.range = 0xFFFFFFFF;
                if (_rc.code == 0xFFFFFFFF) {
                    return _fail(errc::corrupt, "ppmd: corrupt data");
                }
                in += 5;
                _taken = 5;
                _started = true;
            }
            while (_done < _size) {
                if (pos >= limit) {
                    return LzmaStatus::need_room;
                }
                if (size_t(end - in) < Margin && !input_ended) {
                    return LzmaStatus::need_input;
                }
                _rc.in = in;
                _rc.end = end;
                int symbol = _model.decode(_rc);
                _taken += uint64_t(_rc.in - in);
                in = _rc.in;
                if (_rc.overrun) {
                    return _fail(errc::unexpected_end, "ppmd: unexpected end of the compressed data");
                }
                if (symbol < 0) {
                    if (symbol == -1 && _size == UINT64_MAX) {
                        _ended = true;
                        return LzmaStatus::done;
                    }
                    return _fail(errc::corrupt, symbol == -1 ? "ppmd: the end marker before the size" : "ppmd: corrupt data");
                }
                out[pos++] = uint8_t(symbol);
                ++_done;
            }
            _ended = true;
            return LzmaStatus::done;
        }

    private:
        LzmaStatus _fail(errc code, const char* text) noexcept {
            _failed = true;
            error = code;
            error_text = text;
            return LzmaStatus::failed;
        }

        Ppmd7 _model;
        Ppmd7RangeDecoder _rc;
        uint64_t _size = 0;
        uint64_t _done = 0;
        uint64_t _taken = 0;
        bool _started = false;
        bool _failed = false;
        bool _ended = false;
    };

    class Ppmd7Encoder {
    public:
        explicit Ppmd7Encoder(uint32_t order, uint32_t memory)
        : _rc(_sink), _order(order) {
            _model.allocate(memory);
            _model.init(order);
        }

        const Ppmd7& model() const noexcept {
            return _model;
        }

        // A new stream with the same model's memory
        void restart() {
            _model.init(_order);
            _rc.reset();
        }

        void encode(const uint8_t* p, size_t n, std::vector<uint8_t>& out) {
            _rc.sink(out);
            for (size_t i = 0; i < n; ++i) {
                _model.encode(_rc, p[i]);
            }
        }

        // The coder's last bytes, the end marker first when asked (7z
        // knows the size and writes none)
        void finish(std::vector<uint8_t>& out, bool marker = false) {
            _rc.sink(out);
            if (marker) {
                _model.encode(_rc, -1);
            }
            _rc.finish();
        }

    private:
        std::vector<uint8_t> _sink;
        RangeEncoder _rc;
        uint32_t _order;
        Ppmd7 _model;
    };
}
