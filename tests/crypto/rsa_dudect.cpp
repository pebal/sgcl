//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A statistical look for timing leaks in RSA's private side, in the style of
// dudect (Reparaz, Balasch and Verbauwhede, DATE 2017; the harness of
// ecc_dudect.cpp): two classes of inputs run interleaved in a random order,
// Welch's t-test on the times below the 90th percentile; |t| above 4.5 is
// where dudect starts to suspect a leak, the tests fail above 10 (a laptop
// is noisy).
//
//   ModPow          the exponentiation by a secret exponent: 1 (every window
//                   0 but the last) against random exponents, the base
//                   fixed; and a base of 1 against random bases
//   ModPowControl   the same classes through a square-and-multiply that
//                   skips the product for a zero window: the harness must
//                   see that leak (|t| above 10), or it sees nothing
//   Sign            a whole signature: one digest against random ones
//   OaepDecode      the decoding after the private operation: an encoding
//                   whose first byte is not 0 against one whose label hash
//                   is wrong (Manger's oracle tells these apart)
//   OaepControl     the same through a decoding that returns at the first
//                   byte: must leak
//   OaepDecrypt     the whole decryption, the two classes of ciphertext
//
// Noise, so not run by ctest: DISABLED_, run by hand with
//   tests_crypto --gtest_also_run_disabled_tests --gtest_filter='*RsaDudect*'
#include "rsa_common.h"

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace rsa_test;

namespace {
    namespace bn = crypto::detail::bn;
    using Access = crypto::detail::RsaAccess;

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

    // A modulus of k words and its constants
    struct Mod {
        size_t k;
        std::vector<uint64_t> m, rr, rrr, s;
        bn::Modulus mod;

        explicit Mod(size_t words)
        : k(words), m(words), rr(words), rrr(words), s(bn::pow_scratch(words) + 4 * words) {
            random_source r(5);
            for (auto& w : m) {
                w = r.g();
            }
            m[0] |= 1;
            m[k - 1] |= uint64_t(1) << 63;
            uint64_t m0 = bn::mont_m0inv(m[0]);
            bn::mont_constants(rr.data(), rrr.data(), m.data(), k, m0, s.data());
            mod = bn::Modulus{m.data(), rr.data(), rrr.data(), m0, k};
        }
    };

    // An exponent of k words: 1, or random; the class as bytes
    // Both classes are prepared alike — the same draws, the same copies —
    // and differ only in which bytes they hold: a preparation that does
    // more for one class (a memset, a draw) shows as a difference of its
    // own (|t| 23 on constant-time code, the auditor's review)
    bytes_t exponent_class(int c, random_source& r, size_t k) {
        bytes_t random = r.bytes(8 * k);
        bytes_t one(8 * k, 0);
        one.back() = 1;
        return bytes_t(c == 0 ? one : random);
    }

    std::vector<uint64_t> words_of(const bytes_t& b, size_t k) {
        std::vector<uint64_t> w(k);
        bn::from_be(w.data(), k, b.data(), b.size());
        return w;
    }

    // Square and multiply that skips the product of a zero window: the leak
    // the control must see
    void leaky_pow(uint64_t* r, const uint64_t* a, const uint64_t* e, size_t ebits, const bn::Modulus& mod, uint64_t* t) {
        size_t k = mod.k;
        std::vector<uint64_t> acc(k), one(k);
        one[0] = 1;
        bn::mont_mul(acc.data(), one.data(), mod.rr, mod, t);
        for (size_t i = ebits; i-- > 0;) {
            bn::mont_sqr(acc.data(), acc.data(), mod, t);
            if ((e[i / 64] >> (i % 64)) & 1) {
                bn::mont_mul(acc.data(), acc.data(), a, mod, t);
            }
        }
        bn::copy(r, acc.data(), k);
    }

    // OAEP encodings of the two classes under SHA-256 for k bytes: the first
    // byte not 0 (class 0), the label's hash wrong (class 1); both fail
    bytes_t bad_encoding(int c, random_source& r, size_t k) {
        size_t h = 32;
        bytes_t db(k - h - 1, 0);
        bytes_t lh = ossl_digest(hash_id::sha256, bytes_t{});
        lh[0] ^= (unsigned char)(c == 1);   // the same work for both classes
        std::copy(lh.begin(), lh.end(), db.begin());
        db[db.size() - 17] = 0x01;
        bytes_t seed = r.bytes(h);
        auto mgf = [&](const bytes_t& s, size_t n) {
            bytes_t out;
            for (uint32_t i = 0; out.size() < n; ++i) {
                bytes_t in = s;
                for (int j = 3; j >= 0; --j) {
                    in.push_back((unsigned char)(i >> (8 * j)));
                }
                bytes_t d = ossl_digest(hash_id::sha256, in);
                out.insert(out.end(), d.begin(), d.end());
            }
            out.resize(n);
            return out;
        };
        bytes_t dm = mgf(seed, db.size());
        for (size_t i = 0; i < db.size(); ++i) {
            db[i] ^= dm[i];
        }
        bytes_t sm = mgf(db, h);
        for (size_t i = 0; i < h; ++i) {
            seed[i] ^= sm[i];
        }
        unsigned char y = (unsigned char)(0x01 + r.below(0x7f));   // drawn for both classes
        bytes_t em = {(unsigned char)(c == 0 ? y : 0x00)};
        em.insert(em.end(), seed.begin(), seed.end());
        em.insert(em.end(), db.begin(), db.end());
        return em;
    }
}

TEST(Crypto_RsaDudect, DISABLED_ModPow) {
    Mod m(16);
    std::vector<uint64_t> base(16), out(16);
    random_source r(1);
    for (auto& w : base) {
        w = r.g();
    }
    base[15] >>= 1;
    bn::to_mont(base.data(), base.data(), m.mod, m.s.data());
    volatile uint64_t sink = 0;
    double t = dudect(
        20000, [&](int c, random_source& rs) { return exponent_class(c, rs, 16); },
        [&](const bytes_t& e) {
            auto ew = words_of(e, 16);
            bn::mont_pow(out.data(), base.data(), ew.data(), 1024, m.mod, m.s.data());
            sink = sink + out[0];
        });
    std::printf("dudect rsa mont_pow, exponent 1 against random: t = %.2f\n", t);
    EXPECT_LT(std::fabs(t), 10.0);
    std::vector<uint64_t> e(16);
    for (auto& w : e) {
        w = r.g();
    }
    double t2 = dudect(
        20000,
        [&](int c, random_source& rs) {
            if (c == 0) {
                bytes_t one(128, 0);
                one.back() = 1;
                return one;
            }
            bytes_t b = rs.bytes(128);
            b[0] &= 0x7f;
            return b;
        },
        [&](const bytes_t& b) {
            auto bw = words_of(b, 16);
            bn::to_mont(bw.data(), bw.data(), m.mod, m.s.data());
            bn::mont_pow(out.data(), bw.data(), e.data(), 1024, m.mod, m.s.data());
            sink = sink + out[0];
        });
    std::printf("dudect rsa mont_pow, base 1 against random: t = %.2f\n", t2);
    EXPECT_LT(std::fabs(t2), 10.0);
}

TEST(Crypto_RsaDudect, DISABLED_ModPowControl) {
    Mod m(16);
    std::vector<uint64_t> base(16), out(16);
    random_source r(1);
    for (auto& w : base) {
        w = r.g();
    }
    base[15] >>= 1;
    bn::to_mont(base.data(), base.data(), m.mod, m.s.data());
    volatile uint64_t sink = 0;
    double t = dudect(
        20000, [&](int c, random_source& rs) { return exponent_class(c, rs, 16); },
        [&](const bytes_t& e) {
            auto ew = words_of(e, 16);
            leaky_pow(out.data(), base.data(), ew.data(), 1024, m.mod, m.s.data());
            sink = sink + out[0];
        });
    std::printf("dudect rsa control (square and multiply): t = %.2f\n", t);
    EXPECT_GT(std::fabs(t), 10.0);
}

TEST(Crypto_RsaDudect, DISABLED_Sign) {
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    bytes_t fixed(32, 0x5a);
    volatile unsigned sink = 0;
    double t = dudect(
        20000, [&](int c, random_source& rs) { bytes_t random = rs.bytes(32); return bytes_t(c == 0 ? fixed : random); },
        [&](const bytes_t& d) {
            auto s = key.sign_digest(hash_id::sha256, view(d));
            sink = sink + unsigned(s[0]);
        });
    std::printf("dudect rsa sign, one digest against random: t = %.2f\n", t);
    EXPECT_LT(std::fabs(t), 10.0);
}

TEST(Crypto_RsaDudect, DISABLED_OaepDecode) {
    size_t k = 256;
    volatile uint64_t sink = 0;
    double t = dudect(
        200000, [&](int c, random_source& rs) { return bad_encoding(c, rs, k); },
        [&](const bytes_t& em) {
            unsigned char buf[256];
            std::memcpy(buf, em.data(), k);
            size_t off = 0;
            sink = sink + crypto::detail::rsa_pad::oaep_decode(hash_id::sha256, hash_id::sha256, buf, k, nullptr, 0, &off);
        });
    std::printf("dudect rsa oaep decode, Y != 0 against a wrong label hash: t = %.2f\n", t);
    EXPECT_LT(std::fabs(t), 10.0);
}

TEST(Crypto_RsaDudect, DISABLED_OaepControl) {
    size_t k = 256;
    volatile uint64_t sink = 0;
    double t = dudect(
        50000, [&](int c, random_source& rs) { return bad_encoding(c, rs, k); },
        [&](const bytes_t& em) {
            unsigned char buf[256];
            std::memcpy(buf, em.data(), k);
            if (buf[0] != 0) {   // the leak: Manger's oracle
                sink = sink + 1;
                return;
            }
            size_t off = 0;
            sink = sink + crypto::detail::rsa_pad::oaep_decode(hash_id::sha256, hash_id::sha256, buf, k, nullptr, 0, &off);
        });
    std::printf("dudect rsa oaep control (an early return): t = %.2f\n", t);
    EXPECT_GT(std::fabs(t), 10.0);
}

TEST(Crypto_RsaDudect, DISABLED_OaepDecrypt) {
    const TestKey& tk = test_key(2048);
    auto key = our_key(tk);
    volatile unsigned sink = 0;
    double t = dudect(
        6000, [&](int c, random_source& rs) { return ossl_raw_public(tk.ossl, bad_encoding(c, rs, 256)); },
        [&](const bytes_t& ct) {
            auto m = key.decrypt_oaep(hash_id::sha256, view(ct));
            sink = sink + unsigned(m.has_value());
        });
    std::printf("dudect rsa oaep decrypt, Y != 0 against a wrong label hash: t = %.2f\n", t);
    EXPECT_LT(std::fabs(t), 10.0);
}
