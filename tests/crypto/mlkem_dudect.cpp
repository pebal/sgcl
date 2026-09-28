//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-KEM under dudect's statistical look for timing leaks (Reparaz,
// Balasch and Verbauwhede, DATE 2017; ecc_dudect.cpp has the method): two
// classes of secret inputs run interleaved in a random order, each run
// timed, the slowest tenth dropped, Welch's t-test over the rest; |t|
// above 4.5 is where dudect starts to suspect a leak, the tests fail above
// 10 (a laptop is noisy). The classes, for each parameter set:
//
//   - decapsulation of a genuine ciphertext against a random one of the
//     same length, the key fixed: the implicit rejection must take the
//     time the genuine case takes (both keys are always computed, the
//     choice a mask);
//   - decapsulation of one ciphertext under a fixed key against keys whose
//     secret part is random (the expanded key given to the internal
//     algorithm, so that the expansion is not timed): the secret vector s
//     through the transform, the products and Compress, and z. The public
//     part — ek, whose ρ makes the matrix Â by rejection sampling, a loop
//     whose length depends on ρ and is meant to — is the fixed key's in
//     both classes: a first version drew whole random keys and measured ρ
//     (t of −320 to −560, the sampling's length, which is public);
//   - encapsulation of the message of all zeros (every coefficient of μ
//     zero) against random messages, the key fixed.
//
// Noise, so not run by ctest: DISABLED_, run by hand with
//   tests_crypto --gtest_also_run_disabled_tests --gtest_filter='*MlKemDudect*'
#include "tests/types.h"

#include "sgcl/crypto/mlkem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {
    namespace core = sgcl::crypto::detail::mlkem;
    using bytes_t = std::vector<unsigned char>;

    struct Welch {
        double n[2] = {}, mean[2] = {}, m2[2] = {};

        void add(int c, double x) {
            n[c] += 1;
            double d = x - mean[c];
            mean[c] += d / n[c];
            m2[c] += d * (x - mean[c]);
        }

        double t() const {
            double v0 = m2[0] / (n[0] - 1), v1 = m2[1] / (n[1] - 1);
            return (mean[0] - mean[1]) / std::sqrt(v0 / n[0] + v1 / n[1]);
        }
    };

    bytes_t random_bytes(std::mt19937_64& g, size_t n) {
        bytes_t v(n);
        for (auto& b : v) {
            b = (unsigned char)g();
        }
        return v;
    }

    // op(input) over inputs prepared for a random order of the classes;
    // the t statistic of the times below the 90th percentile
    template<class Prepare, class Op>
    double dudect(size_t samples, Prepare prepare, Op op) {
        std::mt19937_64 g(42);
        std::vector<int> cls(samples);
        std::vector<bytes_t> inputs(samples);
        for (size_t i = 0; i < samples; ++i) {
            cls[i] = int(g() % 2);
            inputs[i] = prepare(cls[i], g);
        }
        std::vector<double> times(samples);
        for (size_t i = 0; i < samples; ++i) {
            auto t0 = std::chrono::steady_clock::now();
            op(inputs[i]);
            times[i] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
        }
        std::vector<double> sorted = times;
        std::nth_element(sorted.begin(), sorted.begin() + samples * 9 / 10, sorted.end());
        double cut = sorted[samples * 9 / 10];
        Welch w;
        for (size_t i = 0; i < samples; ++i) {
            if (times[i] < cut) {
                w.add(cls[i], times[i]);
            }
        }
        return w.t();
    }

    template<class P>
    struct Key {
        bytes_t ek = bytes_t(core::Sizes<P>::ek), dk = bytes_t(core::Sizes<P>::dk);

        explicit Key(std::mt19937_64& g) {
            auto seed = random_bytes(g, 64);
            core::keygen_internal<P>(ek.data(), dk.data(), seed.data(), seed.data() + 32);
        }
    };

    template<class P>
    void genuine_against_random(const char* name) {
        std::mt19937_64 g(1);
        Key<P> key(g);
        auto m = random_bytes(g, 32);
        bytes_t genuine(core::Sizes<P>::ciphertext);
        unsigned char k[32];
        core::encaps_internal<P>(k, genuine.data(), key.ek.data(), m.data());
        volatile unsigned sink = 0;
        double t = dudect(
            40000, [&](int c, std::mt19937_64& r) {
                bytes_t other = random_bytes(r, core::Sizes<P>::ciphertext);   // both classes draw the same
                return c == 0 ? genuine : other;
            },
            [&](const bytes_t& c) {
                unsigned char out[32];
                core::decaps_internal<P>(out, key.dk.data(), c.data());
                sink = sink + out[0];
            });
        std::printf("dudect %s decapsulation, genuine / random ciphertext: t = %.2f\n", name, t);
        EXPECT_LT(std::fabs(t), 10.0);
    }

    template<class P>
    void fixed_against_random_key(const char* name) {
        std::mt19937_64 g(2);
        Key<P> fixed(g);
        bytes_t c(core::Sizes<P>::ciphertext);
        {
            auto m = random_bytes(g, 32);
            unsigned char k[32];
            core::encaps_internal<P>(k, c.data(), fixed.ek.data(), m.data());
        }
        // the random class: the fixed key with a random key's s (dk_PKE) and
        // a random z; ek and H(ek), the public part, the fixed key's
        auto secret_of = [&](std::mt19937_64& r) {
            bytes_t dk = fixed.dk;
            Key<P> other(r);
            std::memcpy(dk.data(), other.dk.data(), core::Sizes<P>::dk_pke);
            auto z = random_bytes(r, 32);
            std::memcpy(dk.data() + core::Sizes<P>::dk - 32, z.data(), 32);
            return dk;
        };
        volatile unsigned sink = 0;
        double t = dudect(
            40000, [&](int cls, std::mt19937_64& r) {
                bytes_t other = secret_of(r);   // both classes draw the same
                return cls == 0 ? fixed.dk : other;
            },
            [&](const bytes_t& dk) {
                unsigned char out[32];
                core::decaps_internal<P>(out, dk.data(), c.data());
                sink = sink + out[0];
            });
        std::printf("dudect %s decapsulation, fixed / random key: t = %.2f\n", name, t);
        EXPECT_LT(std::fabs(t), 10.0);
    }

    template<class P>
    void zero_against_random_message(const char* name) {
        std::mt19937_64 g(3);
        Key<P> key(g);
        volatile unsigned sink = 0;
        double t = dudect(
            40000, [&](int c, std::mt19937_64& r) {
                bytes_t other = random_bytes(r, 32);
                return c == 0 ? bytes_t(32, 0) : other;
            },
            [&](const bytes_t& m) {
                unsigned char k[32], c[core::Sizes<P>::ciphertext];
                core::encaps_internal<P>(k, c, key.ek.data(), m.data());
                sink = sink + k[0];
            });
        std::printf("dudect %s encapsulation, zero / random message: t = %.2f\n", name, t);
        EXPECT_LT(std::fabs(t), 10.0);
    }
}

TEST(Crypto_MlKemDudect, DISABLED_DecapsulationGenuineOrRejected) {
    genuine_against_random<core::Params512>("ML-KEM-512");
    genuine_against_random<core::Params768>("ML-KEM-768");
    genuine_against_random<core::Params1024>("ML-KEM-1024");
}

TEST(Crypto_MlKemDudect, DISABLED_DecapsulationKey) {
    fixed_against_random_key<core::Params512>("ML-KEM-512");
    fixed_against_random_key<core::Params768>("ML-KEM-768");
    fixed_against_random_key<core::Params1024>("ML-KEM-1024");
}

TEST(Crypto_MlKemDudect, DISABLED_EncapsulationMessage) {
    zero_against_random_message<core::Params512>("ML-KEM-512");
    zero_against_random_message<core::Params768>("ML-KEM-768");
    zero_against_random_message<core::Params1024>("ML-KEM-1024");
}
