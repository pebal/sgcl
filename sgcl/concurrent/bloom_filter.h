//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/atomic_ref.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "detail/sketch.h"
#include "error.h"

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace sgcl::concurrent {
    // A Bloom filter: m bits and k hashes, an element added by setting its
    // k bits and asked by looking at them, so an answer of no is certain
    // and one of yes is wrong at the rate the shape gives. The k positions
    // are Kirsch and Mitzenmacher's double hashing of one XXH3-128 of the
    // key (h1 + i·h2, each reduced by multiply-shift over the whole m), the
    // classic filter whose m and k are the textbook optimum for n elements
    // and a rate p: m = -n ln p / ln²2, k = m/n ln 2. A bit is set by an
    // atomic OR of its word, after a look that finds it unset, so any
    // number of threads add and ask at once and a filter that is mostly
    // set is mostly read. A handle: one tracked word to the bits, copies
    // sharing them, clone() copying them.
    namespace detail {
        struct BloomState {
            uint64_t bits = 64;     // m, a multiple of 64
            unsigned hashes = 1;    // k
            dynamic_array<uint64_t> words;
        };

        // The bits set: popcount word by word (cnt on arm64), or NEON's
        // cnt over 16 bytes with the sums added in the lanes, measured the
        // faster on arm64 for a filter of more than a few words
        inline uint64_t bloom_popcount(const uint64_t* w, size_t n) noexcept {
            uint64_t total = 0;
            size_t i = 0;
#if defined(__aarch64__) && !defined(SGCL_CONCURRENT_PORTABLE) && !SGCL_SKETCH_TSAN
            for (; i + 8 <= n; i += 8) {   // 64 bytes: four cnt, the counts widened once; plain loads beside other threads' ORs, a count that is an estimate under them anyway (the scalar loop under TSan)
                uint8x16_t a = vcntq_u8(vreinterpretq_u8_u64(vld1q_u64(w + i)));
                uint8x16_t b = vcntq_u8(vreinterpretq_u8_u64(vld1q_u64(w + i + 2)));
                uint8x16_t c = vcntq_u8(vreinterpretq_u8_u64(vld1q_u64(w + i + 4)));
                uint8x16_t d = vcntq_u8(vreinterpretq_u8_u64(vld1q_u64(w + i + 6)));
                uint16x8_t s = vpaddlq_u8(vaddq_u8(vaddq_u8(a, b), vaddq_u8(c, d)));   // at most 32 per byte
                total += vaddlvq_u16(s);
            }
#endif
            for (; i < n; ++i) {
                total += uint64_t(std::popcount(detail::sketch_load(w[i])));
            }
            return total;
        }
    }

    class bloom_filter {
    public:
        // A filter of the optimal shape for `expected_items` elements and a
        // rate of false positives: invalid_argument for a rate outside
        // (0, 1); an expected count of zero is one
        explicit bloom_filter(size_t expected_items, double false_positive_rate = 0.01)
        : _s(make_tracked<detail::BloomState>()) {
            if (!(false_positive_rate > 0 && false_positive_rate < 1)) {
                throw std::invalid_argument("sgcl::concurrent::bloom_filter: a false positive rate outside (0, 1)");
            }
            double n = expected_items ? double(expected_items) : 1.0;
            double ln2 = 0.6931471805599453;
            double m = std::ceil(-n * std::log(false_positive_rate) / (ln2 * ln2));
            double k = std::round(m / n * ln2);
            _shape(m, k < 1 ? 1 : k > 64 ? 64 : unsigned(k));
        }

        // A filter of `bits` bits (rounded up to a multiple of 64, at least
        // 64) and `hashes` positions an element (1 to 64; invalid_argument
        // otherwise)
        static bloom_filter with_size(size_t bits, unsigned hashes) {
            if (hashes < 1 || hashes > 64) {
                throw std::invalid_argument("sgcl::concurrent::bloom_filter: hashes from 1 to 64");
            }
            return bloom_filter(double(bits), hashes, Shape{});
        }

        // Sets the key's bits: true when one of them was unset, the key
        // certainly not added before (a false answer may be a false
        // positive)
        SGCL_INLINE_HOT bool add(std::string_view key) noexcept {
            return _add(detail::sketch_hash128(key.data(), key.size()));
        }

        template<class Bytes>
        requires detail::SketchBytes<Bytes>
        SGCL_INLINE_HOT bool add(const Bytes& bytes) noexcept {
            const slice<const byte>& key = bytes;
            return _add(detail::sketch_hash128(key.data(), key.size()));
        }

        SGCL_INLINE_HOT bool add(uint64_t key) noexcept {
            unsigned char b[8];
            detail::sketch_le64(key, b);
            return _add(detail::sketch_hash128(b, 8));
        }

        // Whether every bit of the key is set: false is certain, true wrong
        // at the filter's rate
        SGCL_INLINE_HOT bool contains(std::string_view key) const noexcept {
            return _contains(detail::sketch_hash128(key.data(), key.size()));
        }

        template<class Bytes>
        requires detail::SketchBytes<Bytes>
        SGCL_INLINE_HOT bool contains(const Bytes& bytes) const noexcept {
            const slice<const byte>& key = bytes;
            return _contains(detail::sketch_hash128(key.data(), key.size()));
        }

        SGCL_INLINE_HOT bool contains(uint64_t key) const noexcept {
            unsigned char b[8];
            detail::sketch_le64(key, b);
            return _contains(detail::sketch_hash128(b, 8));
        }

        // m and k
        SGCL_INLINE_HOT size_t bit_count() const noexcept {
            return size_t(_s->bits);
        }

        SGCL_INLINE_HOT unsigned hash_count() const noexcept {
            return _s->hashes;
        }

        // The elements added, estimated from the bits set (Swamidass and
        // Baldi): -m/k ln(1 - X/m); infinity once every bit is set
        double approximate_count() const noexcept {
            double m = double(_s->bits);
            double x = double(detail::bloom_popcount(_s->words.data(), _s->words.size()));
            if (x >= m) {
                return std::numeric_limits<double>::infinity();
            }
            return -m / double(_s->hashes) * std::log1p(-x / m);
        }

        // The rate of false positives now: (X/m)^k
        double false_positive_rate() const noexcept {
            double x = double(detail::bloom_popcount(_s->words.data(), _s->words.size()));
            return std::pow(x / double(_s->bits), double(_s->hashes));
        }

        // The union: every bit of other set here too, as if every element
        // added to other had been added here. invalid_argument for a filter
        // of another shape. A filter merged into itself is left as it is
        void merge(const bloom_filter& other) {
            if (other._s->bits != _s->bits || other._s->hashes != _s->hashes) {
                throw std::invalid_argument("sgcl::concurrent::bloom_filter::merge: a filter of another shape");
            }
            uint64_t* mine = _s->words.data();
            const uint64_t* theirs = other._s->words.data();
            for (size_t i = 0, n = _s->words.size(); i < n; ++i) {
                uint64_t t = detail::sketch_load(theirs[i]);
                atomic_ref<uint64_t> w(mine[i]);
                if (t & ~w.load(std::memory_order_relaxed)) {
                    w.fetch_or(t, std::memory_order_relaxed);
                }
            }
        }

        // Every bit unset
        void clear() noexcept {
            uint64_t* w = _s->words.data();
            for (size_t i = 0, n = _s->words.size(); i < n; ++i) {
                atomic_ref<uint64_t>(w[i]).store(0, std::memory_order_relaxed);
            }
        }

        // A filter of its own with the same bits
        bloom_filter clone() const {
            bloom_filter c(double(_s->bits), _s->hashes, Shape{});
            c.merge(*this);
            return c;
        }

        // The filter as bytes: "SGBF", the version 1, k, two zero bytes, m
        // in 8 bytes, then the m/64 words, little-endian
        vector<byte> to_bytes() const {
            size_t n = _s->words.size();
            vector<byte> out(16 + 8 * n);
            unsigned char* p = reinterpret_cast<unsigned char*>(out.data());
            p[0] = 'S';
            p[1] = 'G';
            p[2] = 'B';
            p[3] = 'F';
            p[4] = detail::SketchVersion;
            p[5] = (unsigned char)_s->hashes;
            detail::sketch_le64(_s->bits, p + 8);
            const uint64_t* w = _s->words.data();
            for (size_t i = 0; i < n; ++i) {
                detail::sketch_le64(detail::sketch_load(w[i]), p + 16 + 8 * i);
            }
            return out;
        }

        // A filter from to_bytes' bytes: every field checked before the
        // bits are allocated, their count from the size of the input
        static expected<bloom_filter, error> from_bytes(const slice<const byte>& bytes) noexcept {
            if (auto e = detail::sketch_check_header(bytes, "SGBF", 16)) {
                return unexpected(*e);
            }
            const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
            unsigned k = p[5];
            if (k < 1 || k > 64) {
                return unexpected(error("a Bloom filter's hashes from 1 to 64", 5));
            }
            if (p[6] != 0 || p[7] != 0) {
                return unexpected(error("a reserved byte of a Bloom filter not zero", p[6] ? 6 : 7));
            }
            uint64_t m = detail::sketch_read64(p + 8);
            if (m < 64 || m % 64 != 0) {
                return unexpected(error("a Bloom filter's bits not a positive multiple of 64", 8));
            }
            if (m / 8 != bytes.size() - 16 || (bytes.size() - 16) % 8 != 0) {
                return unexpected(error("a Bloom filter's size does not match its bits", bytes.size()));
            }
            bloom_filter f(double(m), k, Shape{});
            uint64_t* w = f._s->words.data();
            for (size_t i = 0, n = f._s->words.size(); i < n; ++i) {
                w[i] = detail::sketch_read64(p + 16 + 8 * i);
            }
            return f;
        }

        SGCL_INLINE_HOT friend bool operator==(const bloom_filter& a, const bloom_filter& b) noexcept {
            return a._s == b._s;
        }

    private:
        struct Shape {};

        bloom_filter(double bits, unsigned hashes, Shape)
        : _s(make_tracked<detail::BloomState>()) {
            _shape(bits, hashes);
        }

        // m rounded up to whole words (at least one), the words zeroed
        void _shape(double bits, unsigned hashes) {
            constexpr double Most = double(uint64_t(1) << 47);   // 16 TB of bits: past anything a program allocates
            double m = bits < 64 ? 64 : bits > Most ? Most : bits;
            uint64_t words = (uint64_t(m) + 63) / 64;
            _s->bits = words * 64;
            _s->hashes = hashes;
            _s->words = dynamic_array<uint64_t>(size_t(words));
        }

        SGCL_INLINE_HOT bool _add(detail::SketchHash h) noexcept {
            const uint64_t m = _s->bits;
            uint64_t* w = _s->words.data();
            bool fresh = false;
            uint64_t x = h.low;
            for (unsigned i = 0, k = _s->hashes; i < k; ++i, x += h.high) {
                uint64_t bit = detail::sketch_reduce(x, m);
                uint64_t mask = uint64_t(1) << (bit & 63);
                atomic_ref<uint64_t> word(w[bit >> 6]);
                if (!(word.load(std::memory_order_relaxed) & mask)) {   // a look first: a set bit is not written
                    fresh |= !(word.fetch_or(mask, std::memory_order_relaxed) & mask);
                }
            }
            return fresh;
        }

        SGCL_INLINE_HOT bool _contains(detail::SketchHash h) const noexcept {
            const uint64_t m = _s->bits;
            const uint64_t* w = _s->words.data();
            uint64_t x = h.low;
            for (unsigned i = 0, k = _s->hashes; i < k; ++i, x += h.high) {
                uint64_t bit = detail::sketch_reduce(x, m);
                if (!(detail::sketch_load(w[bit >> 6]) >> (bit & 63) & 1)) {
                    return false;
                }
            }
            return true;
        }

        tracked_ptr<detail::BloomState> _s;
    };
}
