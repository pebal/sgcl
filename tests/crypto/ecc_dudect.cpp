//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A statistical look for timing leaks in the style of dudect (Reparaz,
// Balasch and Verbauwhede, "Dude, is my code constant time?", DATE 2017):
// two classes of secret inputs — one fixed, one random — are run
// interleaved in a random order, each run timed; Welch's t-test compares
// the two distributions of times, after the slowest tenth is dropped (the
// interrupts and the migrations between cores). |t| above 4.5 is where
// dudect starts to suspect a leak; the tests fail above 10, since a laptop
// is noisy.
//
// The fixed class is the worst case for a leak: a scalar of 1 (every
// window but the last a 0, the digit that would skip an addition in a
// variable-time multiplication) against random scalars — for ECDH's d, and
// for ECDSA's k (through the deterministic signer, the digest and the key
// fixed for the fixed class, random for the other).
//
// Noise, so not run by ctest: DISABLED_, run by hand with
//   tests_crypto --gtest_also_run_disabled_tests --gtest_filter='*Dudect*'
#include "ecc_common.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace ecc_test;

namespace {
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

    // Runs op(class, input) over prepared inputs in a random order of the
    // classes; the t statistic of the times below the 90th percentile
    template<class Prepare, class Op>
    double dudect(size_t samples, Prepare prepare, Op op) {
        random_source r(42);
        std::vector<int> cls(samples);
        std::vector<bytes_t> inputs(samples);
        for (size_t i = 0; i < samples; ++i) {
            cls[i] = int(r.below(2));
            inputs[i] = prepare(cls[i], r);
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

    template<class T>
    bytes_t fixed_or_random_scalar(int c, random_source& r) {
        if (c == 0) {
            bytes_t one(T::size, 0);
            one.back() = 1;
            return one;
        }
        return random_scalar<T>(r);
    }

    template<class T>
    class Crypto_EcDudect : public ::testing::Test {};

    TYPED_TEST_SUITE(Crypto_EcDudect, Curves);
}

// ECDH: d * Q for d = 1 against random d, the peer fixed
TYPED_TEST(Crypto_EcDudect, DISABLED_SharedSecret) {
    auto peer = TypeParam::ecdh_key::generate().public_key();
    volatile unsigned sink = 0;
    double t = dudect(
        TypeParam::size == 32 ? 40000 : TypeParam::size == 48 ? 16000 : 8000, [](int c, random_source& r) { return fixed_or_random_scalar<TypeParam>(c, r); },
        [&](const bytes_t& d) {
            auto key = TypeParam::ecdh_key::from_bytes(view(d));
            sink = sink + unsigned(to_bytes(key->shared_secret(peer)->bytes())[0]);
        });
    std::printf("dudect %s shared_secret: t = %.2f\n", TypeParam::group, t);
    EXPECT_LT(std::fabs(t), 10.0);
}

// ECDSA: the whole signature (HMAC-DRBG, k*G, the inverse of k, s) with
// a fixed key and digest — so a fixed k — against random ones
TYPED_TEST(Crypto_EcDudect, DISABLED_Sign) {
    using C = typename TypeParam::curve;
    using Ecdsa = crypto::detail::Ecdsa<C>;
    random_source seed(7);
    bytes_t fixed_d = random_scalar<TypeParam>(seed);
    const bytes_t fixed_digest(TypeParam::size, 0x5a);
    volatile unsigned sink = 0;
    double t = dudect(
        TypeParam::size == 32 ? 40000 : TypeParam::size == 48 ? 16000 : 8000,
        [&](int c, random_source& r) {
            // both classes draw and copy the same: only which bytes differs
            bytes_t random_d = random_scalar<TypeParam>(r), random_digest = r.bytes(TypeParam::size);
            bytes_t in = c == 0 ? fixed_d : random_d;
            const bytes_t& digest = c == 0 ? fixed_digest : random_digest;
            in.insert(in.end(), digest.begin(), digest.end());
            return in;
        },
        [&](const bytes_t& in) {
            unsigned char sig[2 * C::size];
            Ecdsa::template sign<typename TypeParam::hash>(sig, crypto::detail::ec_from_be<C>(in.data()), in.data() + C::size, C::size, nullptr, 0);
            sink = sink + sig[0];
        });
    std::printf("dudect %s sign: t = %.2f\n", TypeParam::group, t);
    EXPECT_LT(std::fabs(t), 10.0);
}

// The scalar inverse (Fermat) and the base multiplication alone
TYPED_TEST(Crypto_EcDudect, DISABLED_BaseMultAndInverse) {
    using E = crypto::detail::Curve<typename TypeParam::curve>;
    volatile uint64_t sink = 0;
    double t = dudect(
        TypeParam::size == 32 ? 60000 : TypeParam::size == 48 ? 30000 : 15000, [](int c, random_source& r) { return fixed_or_random_scalar<TypeParam>(c, r); },
        [&](const bytes_t& k) {
            auto p = E::base_mult(k.data());
            auto inv = E::S::inverse(E::S::to_mont(crypto::detail::ec_from_be<typename TypeParam::curve>(k.data())));
            sink = sink + p.x[0] + inv[0];
        });
    std::printf("dudect %s base_mult + inverse: t = %.2f\n", TypeParam::group, t);
    EXPECT_LT(std::fabs(t), 10.0);
}
