//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A statistical test of constant time in the manner of dudect (Reparaz,
// Balasch, Verbauwhede, 2017) for the operations on secrets of X25519 and
// Ed25519: two classes of secret inputs — one fixed (a scalar of a single
// bit, the most regular there is), one random — the class of each
// measurement drawn at random so that drift in the machine falls on both,
// the time of each call taken alone, and Welch's t between the classes,
// whole and with the slowest tails cut at several percentiles. |t| above
// 4.5 says the time depends on the secret; above 10, beyond doubt.
//
// Disabled: the numbers are noise on a shared machine and take a while.
// Run by hand, on an idle machine, in a release build:
//
//   tests_crypto --gtest_also_run_disabled_tests --gtest_filter='Crypto_Curve25519Timing.*'
//
// SGCL_DUDECT_N=<count> takes more measurements (40000 by default): a real
// dependence grows |t| with the square root of the count, noise does not.
#include "curve25519_common.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

using namespace curve_test;

namespace {
    struct welch {
        double n[2] = {0, 0}, mean[2] = {0, 0}, m2[2] = {0, 0};

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

    // The largest |t| over the whole sample and the crops, printed
    double max_t(const char* name, const std::vector<int>& cls, const std::vector<double>& ns) {
        std::vector<double> sorted = ns;
        std::sort(sorted.begin(), sorted.end());
        double worst = 0;
        std::printf("  %s: %zu measurements, median %.0f ns\n", name, ns.size(), sorted[sorted.size() / 2]);
        for (double pct : {1.0, 0.99, 0.95, 0.9, 0.75, 0.5}) {
            double cut = sorted[std::min(sorted.size() - 1, size_t(pct * double(sorted.size())))];
            welch w;
            for (size_t i = 0; i < ns.size(); ++i) {
                if (ns[i] <= cut) {
                    w.add(cls[i], ns[i]);
                }
            }
            double t = w.t();
            std::printf("    crop %3.0f%%: t = %+.2f (means %.1f / %.1f ns)\n", pct * 100, t, w.mean[0], w.mean[1]);
            worst = std::max(worst, std::fabs(t));
        }
        return worst;
    }

    // SGCL_DUDECT_N measurements a test in place of the default
    template<class Prepare, class Run>
    double measure(const char* name, size_t count, Prepare prepare, Run run) {
        if (const char* n = std::getenv("SGCL_DUDECT_N")) {
            count = size_t(std::atoll(n));
        }
        random_source r(2017);
        std::vector<int> cls(count);
        std::vector<double> ns(count);
        for (size_t i = 0; i < count; ++i) {
            cls[i] = int(r.g() & 1);
        }
        // every input made before anything is measured, so that making a
        // random one (which the fixed class does not need) leaves no trace
        // in the caches or the predictors that the measurement would see
        std::vector<decltype(prepare(0, r))> inputs;
        inputs.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            inputs.push_back(prepare(cls[i], r));
        }
        // a warm-up, not counted
        for (size_t i = 0; i < std::min<size_t>(count, 200); ++i) {
            run(inputs[i]);
        }
        for (size_t i = 0; i < count; ++i) {
            auto t0 = std::chrono::steady_clock::now();
            run(inputs[i]);
            auto t1 = std::chrono::steady_clock::now();
            ns[i] = double(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }
        return max_t(name, cls, ns);
    }

    volatile unsigned char sink;
}

TEST(Crypto_Curve25519Timing, DISABLED_X25519SharedSecret) {
    auto peer = crypto::x25519::private_key::generate().public_key();
    static const bytes_t zero(32, 0);   // clamps to 2^254
    auto prepare = [](int c, random_source& r) {
        bytes_t random = r.bytes(32);   // drawn for both classes: the two are prepared alike
        return std::move(*crypto::x25519::private_key::from_bytes(view(c == 0 ? zero : random)));
    };
    auto run = [&](const crypto::x25519::private_key& k) {
        sink = (unsigned char)k.shared_secret(peer)->bytes()[0];
    };
    double t = measure("x25519 shared_secret", 40000, prepare, run);
    EXPECT_LT(t, 4.5);
}

TEST(Crypto_Curve25519Timing, DISABLED_X25519PublicKey) {
    static const bytes_t zero(32, 0);
    auto prepare = [](int c, random_source& r) {
        bytes_t random = r.bytes(32);
        return bytes_t(c == 0 ? zero : random);
    };
    auto run = [](const bytes_t& k) {
        sink = (unsigned char)crypto::x25519::private_key::from_bytes(view(k))->public_key().bytes()[0];
    };
    double t = measure("x25519 public key (fixed-base)", 40000, prepare, run);
    EXPECT_LT(t, 4.5);
}

TEST(Crypto_Curve25519Timing, DISABLED_Ed25519Sign) {
    bytes_t msg(64, 0x5a);
    static const bytes_t zero(32, 0);
    auto prepare = [](int c, random_source& r) {
        bytes_t random = r.bytes(32);
        return std::move(*crypto::ed25519::private_key::from_seed(view(c == 0 ? zero : random)));
    };
    auto run = [&](const crypto::ed25519::private_key& k) {
        sink = (unsigned char)k.sign(view(msg))[0];
    };
    double t = measure("ed25519 sign", 40000, prepare, run);
    EXPECT_LT(t, 4.5);
}

// The control: the verification is variable time by design (its inputs are
// public), and the harness must see it — one fixed signature against
// signatures of random messages. |t| far above 10 says the test has the
// power to find a dependence where there is one.
TEST(Crypto_Curve25519Timing, DISABLED_ControlVerifyIsVariableTime) {
    auto k = crypto::ed25519::private_key::generate();
    auto pub = k.public_key();
    struct input {
        bytes_t msg;
        sgcl::array<byte, 64> sig;
    };
    bytes_t fixed(64, 1);
    auto fixed_sig = k.sign(view(fixed));
    auto prepare = [&](int c, random_source& r) {
        if (c == 0) {
            return input{fixed, fixed_sig};
        }
        bytes_t m = r.bytes(64);
        return input{m, k.sign(view(m))};
    };
    auto run = [&](const input& in) {
        sink = (unsigned char)pub.verify(view(in.msg), in.sig);
    };
    double t = measure("ed25519 verify (control)", 40000, prepare, run);
    EXPECT_GT(t, 10.0);
}
