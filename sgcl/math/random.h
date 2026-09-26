//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "detail/chacha8.h"
#include "detail/limb.h"
#include "detail/ziggurat.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <ranges>
#include <stdexcept>
#include <utility>

#if !defined(_WIN32)
#include <pthread.h>
#endif

// Random numbers: what Go's math/rand/v2 is, as one type. A random is a
// generator and its methods are the questions a program asks of one —
// a number below a bound, a die, a coin, a double, a normal or an
// exponential variate, a shuffle, one element of a list — with nothing
// global and no second road: make one and ask it.
//
// The generator is ChaCha8Rand (detail/chacha8.h), the one Go's ChaCha8
// is: a key of 32 bytes gives the same stream of 64-bit words here and in
// Go, on every platform, which random(seed) makes use of — its key is the
// seed's eight bytes, little-endian, and 24 zeros, so Go's
// rand.NewChaCha8 of that key draws the same Uint64s, and the same
// Int64N, Float64, Shuffle and Perm, which are computed from them the
// same way. The distributions are this library's own: next_normal and
// next_exponential are ziggurats (detail/ziggurat.h) and give neither
// Go's numbers nor the standard library's. Their tables are literals,
// so one seed gives the same numbers on every platform — which the
// distributions of <random> do not promise — but for the rare draw
// that falls outside the ziggurat's rectangles and goes through
// std::exp or std::log, whose last bit a platform's library may round
// otherwise.
//
// random() takes its key from a generator of the thread, which takes its
// own from the system (std::random_device, arc4random on macOS) the first
// time the thread asks and again in a child after fork(): two defaults
// never repeat each other, in two threads or across a fork. random(seed)
// is the one to write down when a run has to be replayed.
//
// A random is a value of about 300 bytes with no pointer in it, so it
// goes anywhere, and is not for sharing between threads: one to a thread
// or a task. A copy copies the stream (both draw the same numbers after).
// It is strong as a stream, but it is not the source of keys and secrets
// — a seed is a number and a copy repeats — that is crypto's.
namespace sgcl::math {
    class big_integer;
    class random;

    namespace detail {
        // Bumped in the child of every fork(): a thread's generator seeded
        // under another number seeds itself again
        inline std::atomic<uint64_t> fork_generation{0};

        inline void count_forks() noexcept {
#if !defined(_WIN32)
            static const bool registered = [] {
                ::pthread_atfork(nullptr, nullptr, +[] {
                    fork_generation.fetch_add(1, std::memory_order_relaxed);
                });
                return true;
            }();
            (void)registered;
#endif
        }

        // A key for a new default random: four words of the thread's own
        // ChaCha8Rand, which was keyed from the system
        inline void fresh_key(uint64_t (&key)[4]) {
            thread_local ChaCha8 source;
            thread_local uint64_t seeded_in = ~uint64_t(0);
            count_forks();
            uint64_t generation = fork_generation.load(std::memory_order_relaxed);
            if (seeded_in != generation) {
                std::random_device device;
                unsigned char bytes[32];
                for (int i = 0; i < 32; i += 4) {
                    auto w = uint32_t(device());
                    for (int b = 0; b < 4; ++b) {
                        bytes[i + b] = (unsigned char)(w >> (8 * b));
                    }
                }
                source.init(bytes);
                seeded_in = generation;
            }
            for (auto& w : key) {
                w = source.take();
            }
        }

        struct RandomAccess;
    }

    class random {
    public:
        // Unpredictable: a new stream, keyed from the thread's generator
        random() {
            uint64_t key[4];
            detail::fresh_key(key);
            _state.init(key);
        }

        // Repeatable: the same numbers in every run and on every platform,
        // the stream of Go's ChaCha8 whose key is the seed little-endian
        // followed by 24 zero bytes
        explicit random(uint64_t seed) noexcept {
            const uint64_t key[4] = {seed, 0, 0, 0};
            _state.init(key);
        }

        // [0, bound); a bound of zero or below is domain_error.
        // Lemire's multiplication with the rejection that makes it exact,
        // a mask for a power of two — what Go's Int64N does, so from the
        // same stream the same numbers
        int64_t next_int(int64_t bound) {
            if (bound <= 0) {
                throw domain_error("sgcl::math::random::next_int: a bound of zero or below");
            }
            return int64_t(_below(uint64_t(bound)));
        }

        // [first, last), as range(first, last): a die is next_int(1, 7);
        // an empty range is domain_error. Any two int64_t, the span
        // between them up to 2^64 - 1.
        int64_t next_int(int64_t first, int64_t last) {
            if (first >= last) {
                throw domain_error("sgcl::math::random::next_int: an empty range");
            }
            return int64_t(uint64_t(first) + _below(uint64_t(last) - uint64_t(first)));
        }

        // [0, bound) for a bound of any size (big_integer.h, where it is
        // defined; including this header alone does not bring it): as many
        // bits as the bound has, drawn again while the value is not below
        // it, which takes fewer than two draws on the average. A bound
        // within int64_t is next_int(int64_t)'s, so the same stream gives
        // the same numbers either way; zero or below is domain_error.
        big_integer next_int(const big_integer& bound);

        // All 64 bits
        uint64_t next_uint64() noexcept {
            return _state.take();
        }

        // [0, 1) in steps of 2^-53, as Go's Float64: the low 53 bits of a
        // draw over 2^53
        double next_double() noexcept {
            return double(_state.take() << 11 >> 11) * 0x1p-53;
        }

        bool next_bool() noexcept {
            return (_state.take() >> 63) != 0;
        }

        // A normal variate of the mean and the standard deviation; a
        // negative or NaN deviation is domain_error
        double next_normal(double mean = 0, double stddev = 1) {
            if (!(stddev >= 0)) {
                throw domain_error("sgcl::math::random::next_normal: a negative standard deviation");
            }
            return mean + stddev * _normal();
        }

        // An exponential variate of the rate (the mean is 1 / rate); a
        // rate of zero, below or NaN is domain_error
        double next_exponential(double rate = 1) {
            if (!(rate > 0)) {
                throw domain_error("sgcl::math::random::next_exponential: a rate of zero or below");
            }
            return _exponential() / rate;
        }

        // The bytes filled from the stream, eight to a draw, little-endian;
        // the bytes of a last draw not wanted are dropped, so every call
        // starts on a new draw
        void next_bytes(const slice<byte>& out) noexcept {
            byte* p = out.data();
            size_t n = out.size();
            for (; n >= 8; n -= 8, p += 8) {
                uint64_t w = _state.take();
                for (int b = 0; b < 8; ++b) {
                    p[b] = byte(w >> (8 * b));
                }
            }
            if (n) {
                uint64_t w = _state.take();
                for (size_t b = 0; b < n; ++b) {
                    p[b] = byte(w >> (8 * b));
                }
            }
        }

        // The elements in a random order, every order as likely: Fisher
        // and Yates from the back, as Go's Shuffle, so with Go's stream
        // the same order. Any range of random access.
        template<std::ranges::random_access_range R>
        void shuffle(R&& range) {
            auto first = std::ranges::begin(range);
            auto n = std::ranges::distance(range);
            for (auto i = n - 1; i > 0; --i) {
                auto j = decltype(i)(_below(uint64_t(i) + 1));
                std::ranges::iter_swap(first + i, first + j);
            }
        }

        // One element, each as likely; an empty range is out_of_range.
        // The element itself, not a copy: the range has to be one that
        // outlives the call, which is why a temporary is not taken.
        template<std::ranges::random_access_range R>
        requires std::is_lvalue_reference_v<std::ranges::range_reference_t<R>>
        decltype(auto) pick(R& range) {
            auto n = std::ranges::distance(range);
            if (n <= 0) {
                throw out_of_range("sgcl::math::random::pick: an empty range");
            }
            return *(std::ranges::begin(range) + decltype(n)(_below(uint64_t(n))));
        }

        // A const temporary would bind to the one above (R deduced const)
        // and leave the reference hanging; a non-const one does not bind
        template<class R>
        void pick(const R&&) = delete;

        // 0 … n - 1 in a random order, as Go's Perm
        vector<size_t> permutation(size_t n) {
            vector<size_t> p;
            p.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                p.push_back(i);
            }
            shuffle(p);
            return p;
        }

        // A uniform random bit generator of the standard library's, so
        // that std::normal_distribution, std::ranges::sample and the rest
        // take a random as they take mt19937_64
        using result_type = uint64_t;

        static constexpr result_type min() noexcept {
            return 0;
        }

        static constexpr result_type max() noexcept {
            return std::numeric_limits<result_type>::max();
        }

        result_type operator()() noexcept {
            return _state.take();
        }

    private:
        // Uniform in [0, n) for n > 0
        uint64_t _below(uint64_t n) noexcept {
            if (!(n & (n - 1))) {
                return _state.take() & (n - 1);
            }
            uint64_t lo;
            uint64_t hi = detail::mul_wide(_state.take(), n, lo);
            if (lo < n) {
                uint64_t threshold = (0 - n) % n;
                while (lo < threshold) {
                    hi = detail::mul_wide(_state.take(), n, lo);
                }
            }
            return hi;
        }

        // (0, 1), never zero, for the logarithms of the slow paths
        double _open_unit() noexcept {
            return (double(_state.take() >> 11) + 0.5) * 0x1p-53;
        }

        // The ziggurat of 128 layers: the low 7 bits of a draw choose the
        // layer, the 57 above them are the value and its sign. Inside the
        // layer's rectangle — nearly always — the value is the answer;
        // otherwise the wedge is tried against the density, and the
        // bottom layer's tail sampled by Marsaglia's method.
        double _normal() noexcept {
            for (;;) {
                uint64_t u = _state.take();
                size_t i = size_t(u & 127);
                int64_t j = int64_t(u) >> 7;
                uint64_t m = j < 0 ? uint64_t(0) - uint64_t(j) : uint64_t(j);
                double x = double(j) * detail::NormalW[i];
                if (m < detail::NormalK[i]) {
                    return x;
                }
                if (i == 0) {
                    double r = detail::NormalTail;
                    double tx;
                    double ty;
                    do {
                        tx = -std::log(_open_unit()) / r;
                        ty = -std::log(_open_unit());
                    } while (ty + ty < tx * tx);
                    return j < 0 ? -(r + tx) : r + tx;
                }
                double f0 = detail::NormalF[i];
                double f1 = detail::NormalF[i - 1];
                if (f0 + _open_unit() * (f1 - f0) < std::exp(-0.5 * x * x)) {
                    return x;
                }
            }
        }

        // The ziggurat of 256 layers: the low 8 bits the layer, the 56
        // above them the value
        double _exponential() noexcept {
            for (;;) {
                uint64_t u = _state.take();
                size_t i = size_t(u & 255);
                uint64_t j = u >> 8;
                double x = double(j) * detail::ExponentialW[i];
                if (j < detail::ExponentialK[i]) {
                    return x;
                }
                if (i == 0) {
                    return detail::ExponentialTail - std::log(_open_unit());
                }
                double f0 = detail::ExponentialF[i];
                double f1 = detail::ExponentialF[i - 1];
                if (f0 + _open_unit() * (f1 - f0) < std::exp(-x)) {
                    return x;
                }
            }
        }

        detail::ChaCha8 _state;

        friend struct detail::RandomAccess;
    };

    namespace detail {
        // A random over a key of 32 bytes, for the tests that check the
        // stream against the specification's vectors and against Go
        struct RandomAccess {
            static random from_key(const unsigned char (&key)[32]) noexcept {
                random r(0);
                r._state.init(key);
                return r;
            }
        };
    }
}
