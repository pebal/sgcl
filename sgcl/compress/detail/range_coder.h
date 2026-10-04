//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define SGCL_LZMA_INLINE inline __attribute__((always_inline))
#define SGCL_LZMA_INLINE_LAMBDA __attribute__((always_inline))
#define SGCL_LZMA_LIKELY(x) __builtin_expect(!!(x), 1)
#define SGCL_LZMA_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define SGCL_LZMA_INLINE inline
#define SGCL_LZMA_INLINE_LAMBDA
#define SGCL_LZMA_LIKELY(x) (x)
#define SGCL_LZMA_UNLIKELY(x) (x)
#endif

namespace sgcl::compress::detail {
    // The range coder of LZMA, as lzma-specification.txt of the LZMA SDK
    // describes it: a probability is 11 bits (of 2048) in a uint16_t,
    // moved a 32nd of the way towards the bit coded; the range is 32 bits
    // and is renormalised a byte at a time when it falls under 2^24. The
    // encoder carries into bytes already made through a cache byte and a
    // count of 0xFF bytes waiting behind it.
    namespace rc {
        constexpr uint32_t ProbBits = 11;
        constexpr uint32_t ProbOne = uint32_t(1) << ProbBits;
        constexpr uint16_t ProbInit = uint16_t(ProbOne / 2);
        constexpr uint32_t MoveBits = 5;
        constexpr uint32_t Top = uint32_t(1) << 24;

        // What coding a bit costs, in 16ths of a bit: price[p >> 4] for a
        // bit whose probability is p (of 2048)
        constexpr uint32_t PriceShift = 4;
        constexpr uint32_t Infinity = uint32_t(1) << 30;

        struct PriceTable {
            uint32_t v[ProbOne >> PriceShift];

            PriceTable() noexcept {
                for (uint32_t i = 0; i < (ProbOne >> PriceShift); ++i) {
                    // the middle of the 16 probabilities the entry stands for
                    double p = (double(i << PriceShift) + double(1 << (PriceShift - 1))) / double(ProbOne);
                    v[i] = uint32_t(std::lround(-std::log2(p) * double(1 << PriceShift)));
                }
            }
        };

        inline const PriceTable& prices() noexcept {
            static const PriceTable table;
            return table;
        }

        SGCL_LZMA_INLINE uint32_t price0(const uint32_t* t, uint32_t p) noexcept {
            return t[p >> PriceShift];
        }

        SGCL_LZMA_INLINE uint32_t price1(const uint32_t* t, uint32_t p) noexcept {
            return t[(ProbOne - p) >> PriceShift];
        }

        SGCL_LZMA_INLINE uint32_t price(const uint32_t* t, uint32_t p, uint32_t bit) noexcept {
            return bit ? price1(t, p) : price0(t, p);
        }

        // The price of `bits` bits of `symbol` through a tree of probabilities
        // from the top bit down, and from the bottom bit up (the reverse tree)
        inline uint32_t tree_price(const uint32_t* t, const uint16_t* probs, uint32_t bits, uint32_t symbol) noexcept {
            uint32_t price = 0;
            symbol |= uint32_t(1) << bits;
            while (symbol > 1) {
                uint32_t bit = symbol & 1;
                symbol >>= 1;
                price += rc::price(t, probs[symbol], bit);
            }
            return price;
        }

        inline uint32_t reverse_tree_price(const uint32_t* t, const uint16_t* probs, uint32_t bits, uint32_t symbol) noexcept {
            uint32_t price = 0;
            uint32_t m = 1;
            for (uint32_t i = 0; i < bits; ++i) {
                uint32_t bit = symbol & 1;
                symbol >>= 1;
                price += rc::price(t, probs[m], bit);
                m = (m << 1) | bit;
            }
            return price;
        }
    }

    class RangeEncoder {
    public:
        SGCL_INLINE_HOT explicit RangeEncoder(std::vector<uint8_t>& out) noexcept
        : _out(&out) {
        }

        // Where the bytes go from now on
        SGCL_INLINE_HOT void sink(std::vector<uint8_t>& out) noexcept {
            _out = &out;
        }

        SGCL_INLINE_HOT void reset() noexcept {
            _low = 0;
            _range = 0xFFFFFFFF;
            _cache = 0;
            _pending = 1;
        }

        SGCL_LZMA_INLINE void bit(uint16_t& prob, uint32_t bit) noexcept {
            uint32_t bound = (_range >> rc::ProbBits) * prob;
            if (bit == 0) {
                _range = bound;
                prob = uint16_t(prob + ((rc::ProbOne - prob) >> rc::MoveBits));
            } else {
                _low += bound;
                _range -= bound;
                prob = uint16_t(prob - (prob >> rc::MoveBits));
            }
            while (_range < rc::Top) {
                _range <<= 8;
                _shift();
            }
        }

        // The form PPMd uses in 7z: a symbol of frequency size at start
        // of total, and a bit whose 0 has size0 of 2^shift (the range
        // divided first, where LZMA's bit multiplies a probability)
        void encode(uint32_t start, uint32_t size, uint32_t total) noexcept {
            _range /= total;
            _low += uint64_t(start) * _range;
            _range *= size;
            while (_range < rc::Top) {
                _range <<= 8;
                _shift();
            }
        }

        void encode_bit(uint32_t bit, uint32_t size0, uint32_t shift) noexcept {
            uint32_t bound = (_range >> shift) * size0;
            if (bit == 0) {
                _range = bound;
            } else {
                _low += bound;
                _range -= bound;
            }
            while (_range < rc::Top) {
                _range <<= 8;
                _shift();
            }
        }

        // Bits at a probability of one half, top bit first
        void direct(uint32_t value, uint32_t bits) noexcept {
            while (bits--) {
                _range >>= 1;
                _low += _range & (0 - ((value >> bits) & 1));
                while (_range < rc::Top) {
                    _range <<= 8;
                    _shift();
                }
            }
        }

        void tree(uint16_t* probs, uint32_t bits, uint32_t symbol) noexcept {
            uint32_t m = 1;
            while (bits--) {
                uint32_t b = (symbol >> bits) & 1;
                bit(probs[m], b);
                m = (m << 1) | b;
            }
        }

        void reverse_tree(uint16_t* probs, uint32_t bits, uint32_t symbol) noexcept {
            uint32_t m = 1;
            while (bits--) {
                uint32_t b = symbol & 1;
                symbol >>= 1;
                bit(probs[m], b);
                m = (m << 1) | b;
            }
        }

        // The last bytes: every bit of low out
        void finish() noexcept {
            for (int i = 0; i < 5; ++i) {
                _shift();
            }
        }

        // What finish() would still write
        SGCL_INLINE_HOT uint64_t pending() const noexcept {
            return _pending + 4;
        }

    private:
        // The top byte of low goes out, unless it may still take a carry:
        // a 0xFF waits (counted) until a byte under it settles whether
        // the carry comes
        SGCL_LZMA_INLINE void _shift() noexcept {
            if (uint32_t(_low) < 0xFF000000u || (_low >> 32) != 0) {
                uint8_t carry = uint8_t(_low >> 32);
                uint8_t b = _cache;
                do {
                    _out->push_back(uint8_t(b + carry));
                    b = 0xFF;
                } while (--_pending != 0);
                _cache = uint8_t(_low >> 24);
            }
            ++_pending;
            _low = (_low & 0x00FFFFFF) << 8;
        }

        std::vector<uint8_t>* _out;
        uint64_t _low = 0;
        uint32_t _range = 0xFFFFFFFF;
        uint8_t _cache = 0;
        uint64_t _pending = 1;   // the cache byte and the 0xFF bytes behind it, not yet written
    };
}
