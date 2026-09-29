//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The record layer under dudect's statistical look for timing leaks
// (Reparaz, Balasch and Verbauwhede, DATE 2017; tests/crypto/ecc_dudect.cpp
// has the method): two classes of records opened by RecordProtection::open,
// interleaved in a random order, each open timed, the slowest tenth
// dropped, Welch's t-test over the rest. For AES-128-GCM and
// ChaCha20-Poly1305, records of 1 KB:
//
//   - a tag wrong in its first byte against one wrong in its last: both
//     refused, and the time must not tell where the tags part (the tag is
//     compared in constant time before a byte is decrypted); |t| above 10
//     fails;
//   - a good tag against a bad one: the good record is decrypted and the
//     bad one is not, so the times differ by design (that a record is
//     refused is public: the alert and the end of the connection say it);
//     the t is reported for the record, not held to a bound.
//
// Every record is sealed for the sequence number it is opened at (the good
// ones advance it, the refused ones do not), made beforehand by the AEAD
// with the direction's keys, so that only the open is timed.
//
// Noise, so not run by ctest: DISABLED_, run by hand with
//   tests_net --gtest_also_run_disabled_tests --gtest_filter='*TlsDudect*'
#include "tests/types.h"

#include "sgcl/net/tls/detail/record.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;

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

    // A protected record of `inner` at a sequence number, sealed with the
    // direction's keys (the AEAD's public seal)
    template<class Aead>
    bytes_t record_at(const Aead& aead, const uint8_t iv[12], uint64_t sequence, const bytes_t& inner) {
        uint8_t nonce[12];
        std::memcpy(nonce, iv, 12);
        for (int i = 0; i < 8; ++i) {
            nonce[11 - i] ^= uint8_t(sequence >> (8 * i));
        }
        const size_t length = inner.size() + 16;
        bytes_t out(5 + length);
        out[0] = 23;
        out[1] = 3;
        out[2] = 3;
        out[3] = uint8_t(length >> 8);
        out[4] = uint8_t(length);
        aead.seal_to(tls::room_of(out.data() + 5, length), tls::bytes_of(nonce, 12), tls::bytes_of(inner.data(), inner.size()), tls::bytes_of(out.data(), 5));
        return out;
    }

    // classify(i) says a record's class; make(c, sequence) makes it; the
    // opens timed in the order drawn, the t of the times below the 90th
    // percentile
    template<class Aead>
    double run(tls::Cipher cipher, size_t samples, uint64_t seed, int (*make_kind)(int)) {
        std::mt19937_64 g(seed);
        tls::Secret secret;
        secret.size = uint8_t(tls::hash_size(tls::hash_of(cipher)));
        for (size_t i = 0; i < secret.size; ++i) {
            secret.bytes[i] = uint8_t(g());
        }
        tls::TrafficKeys keys;
        tls::traffic_keys(tls::hash_of(cipher), keys, secret, tls::key_size(cipher));
        Aead aead(tls::bytes_of(keys.key, keys.key_size));
        tls::RecordProtection reader;
        reader.install(cipher, secret);
        bytes_t inner(1024 + 1);
        for (auto& b : inner) {
            b = uint8_t(g() | 1);
        }
        inner.back() = 23;
        // the records, in the order they are opened
        std::vector<int> cls(samples);
        std::vector<bytes_t> records(samples);
        uint64_t sequence = 0;
        for (size_t i = 0; i < samples; ++i) {
            cls[i] = int(g() % 2);
            bytes_t r = record_at(aead, keys.iv, sequence, inner);
            switch (make_kind(cls[i])) {
            case 0:   // good: opens, and takes the number
                ++sequence;
                break;
            case 1:   // the tag's first byte changed
                r[r.size() - 16] ^= 0x01;
                break;
            default:  // the tag's last byte changed
                r.back() ^= 0x01;
                break;
            }
            records[i] = std::move(r);
        }
        std::vector<double> times(samples);
        bytes_t work;
        volatile size_t sink = 0;
        for (size_t i = 0; i < samples; ++i) {
            work = records[i];   // open works in place: a copy, outside the timing
            auto t0 = std::chrono::steady_clock::now();
            auto o = reader.open(work.data(), work.size());
            times[i] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count();
            sink = sink + (o ? o->fragment.size() : 1);
        }
        std::vector<double> sorted = times;
        std::nth_element(sorted.begin(), sorted.begin() + samples * 9 / 10, sorted.end());
        const double cut = sorted[samples * 9 / 10];
        Welch w;
        for (size_t i = 0; i < samples; ++i) {
            if (times[i] < cut) {
                w.add(cls[i], times[i]);
            }
        }
        return w.t();
    }

    int first_against_last(int c) {
        return c == 0 ? 1 : 2;
    }

    int good_against_bad(int c) {
        return c == 0 ? 0 : 2;
    }
}

TEST(TlsDudect, DISABLED_RecordOpen) {
    const size_t samples = 100000;   // 100 MB of records per case, made beforehand
    double worst = 0;
    struct Case {
        const char* name;
        double t;
        bool bounded;
    };
    std::vector<Case> cases = {
        {"AES-128-GCM bad tag, first byte / last byte", run<crypto::aes_gcm>(tls::Cipher::aes_128_gcm_sha256, samples, 1, first_against_last), true},
        {"ChaCha20-Poly1305 bad tag, first byte / last byte", run<crypto::chacha20_poly1305>(tls::Cipher::chacha20_poly1305_sha256, samples, 2, first_against_last), true},
        {"AES-128-GCM good tag / bad tag (differ by design)", run<crypto::aes_gcm>(tls::Cipher::aes_128_gcm_sha256, samples, 3, good_against_bad), false},
        {"ChaCha20-Poly1305 good tag / bad tag (differ by design)", run<crypto::chacha20_poly1305>(tls::Cipher::chacha20_poly1305_sha256, samples, 4, good_against_bad), false},
    };
    for (auto& c : cases) {
        std::printf("dudect record open, %s: t = %.2f\n", c.name, c.t);
        if (c.bounded) {
            worst = std::max(worst, std::fabs(c.t));
            EXPECT_LT(std::fabs(c.t), 10.0) << c.name;
        }
    }
    std::printf("dudect record open: max |t| over the bounded cases = %.2f\n", worst);
}
