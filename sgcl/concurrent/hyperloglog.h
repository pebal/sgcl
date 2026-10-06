//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
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
#include <stdexcept>
#include <string_view>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace sgcl::concurrent {
    // HyperLogLog (Flajolet, Fusy, Gandouet, Meunier 2007): the count of
    // distinct elements estimated from 2^p registers, each the most
    // leading zeros (plus one) seen among the 64-bit XXH3 hashes routed to
    // it by their first p bits. A register is raised by an atomic maximum
    // (a load, and a compare-exchange only when the new rank is higher), so
    // any number of threads add at once and a warm sketch is mostly read.
    // The estimate is Ertl's improved estimator ("New cardinality
    // estimation algorithms for HyperLogLog sketches", 2017), which is
    // accurate from zero to the far end of 64 bits by one formula, where
    // HLL++ corrects the raw estimate by tables measured for each
    // precision; its relative standard error is 1.04/sqrt(2^p), 0.81% at
    // the default p of 14 (16 KB of registers). The registers are bytes,
    // dense from the start: no sparse form, whose conversion under
    // concurrent adds would need a lock. A handle: one tracked word to the
    // registers, copies sharing them, clone() copying them.
    namespace detail {
        struct HllState {
            unsigned precision = 14;
            dynamic_array<uint8_t> registers;
        };

        SGCL_INLINE_HOT uint8_t hll_load(const uint8_t& r) noexcept {
            return std::atomic_ref<uint8_t>(const_cast<uint8_t&>(r)).load(std::memory_order_relaxed);
        }

        // The register raised to at least rank
        SGCL_INLINE_HOT void hll_raise(uint8_t& r, uint8_t rank) noexcept {
            std::atomic_ref<uint8_t> a(r);
            uint8_t v = a.load(std::memory_order_relaxed);
            while (rank > v && !a.compare_exchange_weak(v, rank, std::memory_order_relaxed)) {
            }
        }

        // How many registers hold each rank: eight registers a load, four
        // tables of counts so that the increments of neighbours do not wait
        // for each other (4x the loop of a byte and one table: 5.7 us for
        // 2^14 registers against 23.9)
        inline void hll_histogram(const uint8_t* r, size_t m, uint32_t out[66]) noexcept {
            uint32_t c[4][66] = {};
            size_t i = 0;
            for (; i < m && (reinterpret_cast<uintptr_t>(r + i) & 7) != 0; ++i) {
                ++c[0][hll_load(r[i])];
            }
            for (; i + 8 <= m; i += 8) {
                uint64_t w = sketch_load(*reinterpret_cast<const uint64_t*>(r + i));   // the registers' bytes, little-endian
                ++c[0][w & 0xff];
                ++c[1][w >> 8 & 0xff];
                ++c[2][w >> 16 & 0xff];
                ++c[3][w >> 24 & 0xff];
                ++c[0][w >> 32 & 0xff];
                ++c[1][w >> 40 & 0xff];
                ++c[2][w >> 48 & 0xff];
                ++c[3][w >> 56];
            }
            for (; i < m; ++i) {
                ++c[0][hll_load(r[i])];
            }
            for (int k = 0; k < 66; ++k) {
                out[k] = c[0][k] + c[1][k] + c[2][k] + c[3][k];
            }
        }

        // Ertl's sigma and tau (2017, algorithm 6)
        inline double hll_sigma(double x) noexcept {
            if (x == 1) {
                return std::numeric_limits<double>::infinity();
            }
            double y = 1, z = x, previous;
            do {
                x *= x;
                previous = z;
                z += x * y;
                y += y;
            } while (z != previous);
            return z;
        }

        inline double hll_tau(double x) noexcept {
            if (x == 0 || x == 1) {
                return 0;
            }
            double y = 1, z = 1 - x, previous;
            do {
                x = std::sqrt(x);
                previous = z;
                y *= 0.5;
                z -= (1 - x) * (1 - x) * y;
            } while (z != previous);
            return z / 3;
        }
    }

    class hyperloglog {
    public:
        // 2^precision registers, precision from 4 to 18 (invalid_argument
        // otherwise): 14 by default, an error of 0.81% in 16 KB
        explicit hyperloglog(unsigned precision = 14)
        : _s(make_tracked<detail::HllState>()) {
            if (precision < 4 || precision > 18) {
                throw std::invalid_argument("sgcl::concurrent::hyperloglog: a precision from 4 to 18");
            }
            _s->precision = precision;
            _s->registers = dynamic_array<uint8_t>(size_t(1) << precision);
        }

        SGCL_INLINE_HOT void add(std::string_view key) noexcept {
            _add(detail::sketch_hash64(key.data(), key.size()));
        }

        template<class Bytes>
        requires detail::SketchBytes<Bytes>
        SGCL_INLINE_HOT void add(const Bytes& bytes) noexcept {
            const slice<const byte>& key = bytes;
            _add(detail::sketch_hash64(key.data(), key.size()));
        }

        SGCL_INLINE_HOT void add(uint64_t key) noexcept {
            unsigned char b[8];
            detail::sketch_le64(key, b);
            _add(detail::sketch_hash64(b, 8));
        }

        // The distinct elements added, estimated: zero for an empty sketch
        double estimate() const noexcept {
            const unsigned p = _s->precision;
            const unsigned q = 64 - p;
            uint32_t counts[66] = {};
            const size_t m = _s->registers.size();
            detail::hll_histogram(_s->registers.data(), m, counts);
            double dm = double(m);
            double z = dm * detail::hll_tau(1 - double(counts[q + 1]) / dm);
            for (unsigned k = q; k >= 1; --k) {
                z = 0.5 * (z + double(counts[k]));
            }
            z += dm * detail::hll_sigma(double(counts[0]) / dm);
            constexpr double AlphaInf = 0.7213475204444817;   // 1 / (2 ln 2)
            return AlphaInf * dm * dm / z;
        }

        SGCL_INLINE_HOT unsigned precision() const noexcept {
            return _s->precision;
        }

        // The union: each register the higher of the two, the estimate of
        // the elements added to either. invalid_argument for another
        // precision. A sketch merged into itself is left as it is
        void merge(const hyperloglog& other) {
            if (other._s->precision != _s->precision) {
                throw std::invalid_argument("sgcl::concurrent::hyperloglog::merge: a sketch of another precision");
            }
            uint8_t* mine = _s->registers.data();
            const uint8_t* theirs = other._s->registers.data();
            const size_t m = _s->registers.size();
            size_t i = 0;
#if defined(__aarch64__) && !defined(SGCL_CONCURRENT_PORTABLE) && !SGCL_SKETCH_TSAN
            for (; i + 16 <= m; i += 16) {   // the 16 registers compared at once, a CAS only where the other is higher (plain loads beside the adds: a register only ever rises, and the CAS below settles it)
                uint8x16_t t = vld1q_u8(theirs + i);
                uint8x16_t higher = vcgtq_u8(t, vld1q_u8(mine + i));
                if (vmaxvq_u8(higher) == 0) {
                    continue;
                }
                for (size_t j = i; j < i + 16; ++j) {
                    detail::hll_raise(mine[j], detail::hll_load(theirs[j]));
                }
            }
#endif
            for (; i < m; ++i) {
                detail::hll_raise(mine[i], detail::hll_load(theirs[i]));
            }
        }

        // Every register zero
        void clear() noexcept {
            uint8_t* r = _s->registers.data();
            for (size_t i = 0, m = _s->registers.size(); i < m; ++i) {
                std::atomic_ref<uint8_t>(r[i]).store(0, std::memory_order_relaxed);
            }
        }

        // A sketch of its own with the same registers
        hyperloglog clone() const {
            hyperloglog c(_s->precision);
            c.merge(*this);
            return c;
        }

        // The sketch as bytes: "SGHL", the version 1, p, two zero bytes,
        // then the 2^p registers
        vector<byte> to_bytes() const {
            size_t m = _s->registers.size();
            vector<byte> out(8 + m);
            unsigned char* o = reinterpret_cast<unsigned char*>(out.data());
            o[0] = 'S';
            o[1] = 'G';
            o[2] = 'H';
            o[3] = 'L';
            o[4] = detail::SketchVersion;
            o[5] = (unsigned char)_s->precision;
            const uint8_t* r = _s->registers.data();
            for (size_t i = 0; i < m; ++i) {
                o[8 + i] = detail::hll_load(r[i]);
            }
            return out;
        }

        // A sketch from to_bytes' bytes: the precision and the size checked
        // before the registers are allocated, every register's rank after
        static expected<hyperloglog, error> from_bytes(const slice<const byte>& bytes) noexcept {
            if (auto e = detail::sketch_check_header(bytes, "SGHL", 8)) {
                return unexpected(*e);
            }
            const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
            unsigned precision = p[5];
            if (precision < 4 || precision > 18) {
                return unexpected(error("a HyperLogLog's precision from 4 to 18", 5));
            }
            if (p[6] != 0 || p[7] != 0) {
                return unexpected(error("a reserved byte of a HyperLogLog not zero", p[6] ? 6 : 7));
            }
            size_t m = size_t(1) << precision;
            if (bytes.size() - 8 != m) {
                return unexpected(error("a HyperLogLog's size does not match its precision", bytes.size()));
            }
            unsigned most = 64 - precision + 1;
            for (size_t i = 0; i < m; ++i) {
                if (p[8 + i] > most) {
                    return unexpected(error("a HyperLogLog's register past the most a hash gives", 8 + i));
                }
            }
            hyperloglog h(precision);
            uint8_t* r = h._s->registers.data();
            for (size_t i = 0; i < m; ++i) {
                r[i] = p[8 + i];
            }
            return h;
        }

        SGCL_INLINE_HOT friend bool operator==(const hyperloglog& a, const hyperloglog& b) noexcept {
            return a._s == b._s;
        }

    private:
        // The register of the hash's first p bits raised to the leading
        // zeros of the rest, plus one (64 - p + 1 when the rest is zero)
        SGCL_INLINE_HOT void _add(uint64_t h) noexcept {
            const unsigned p = _s->precision;
            const uint64_t index = h >> (64 - p);
            const uint64_t rest = (h << p) | (uint64_t(1) << (p - 1));   // a stop bit: at most 64 - p + 1
            const uint8_t rank = uint8_t(std::countl_zero(rest) + 1);
            uint8_t& r = _s->registers.data()[index];
            if (rank > detail::hll_load(r)) {
                detail::hll_raise(r, rank);
            }
        }

        tracked_ptr<detail::HllState> _s;
    };
}
