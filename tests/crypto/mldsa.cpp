//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-DSA (FIPS 204). The ring — the transform and its inverse against the
// schoolbook product modulo X^256 + 1, the reductions, Power2Round and
// Decompose against their definitions on every value of Z_q; the three
// parameter sets against Wycheproof (key generation from seeds, the
// deterministic signatures of messages with contexts and of μ given, the
// expanded keys without seeds, verification with its malformed hints, norms
// and lengths); against Go's crypto/mldsa both ways (the same public keys
// and deterministic signatures, each verifying the other's hedged ones);
// and the public types' contract at their boundaries.
#include "curve25519_common.h"

#include "tests/source_root.h"

#include "sgcl/core/thread.h"
#include "sgcl/crypto/mldsa.h"
#include "sgcl/encoding/json.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace dsa = sgcl::crypto::detail::mldsa;
using dsa::Poly;
using dsa::Q;

namespace {
    int32_t mod_q(int64_t v) {
        v %= Q;
        return int32_t(v < 0 ? v + Q : v);
    }

    Poly random_poly(std::mt19937& rng, int32_t bound) {
        Poly f;
        for (auto& c : f) {
            c = int32_t(rng() % uint32_t(2 * bound + 1)) - bound;
        }
        return f;
    }

    Poly schoolbook(const Poly& f, const Poly& g) {
        std::vector<int64_t> h(2 * dsa::N, 0);
        for (size_t i = 0; i < dsa::N; ++i) {
            for (size_t j = 0; j < dsa::N; ++j) {
                h[i + j] = (h[i + j] + int64_t(f[i]) * g[j]) % Q;
            }
        }
        Poly r;
        for (size_t i = 0; i < dsa::N; ++i) {
            r[i] = mod_q(h[i] - h[i + dsa::N]);
        }
        return r;
    }

    template<class F>
    size_t wycheproof(const char* file, F each) {
        auto text = curve_test::oracle_file(std::string("wycheproof/testvectors_v1/") + file);
        if (!text) {
            return 0;
        }
        auto doc = sgcl::encoding::json::parse(sgcl::string(*text));
        EXPECT_TRUE(doc.has_value());
        if (!doc) {
            return 0;
        }
        size_t n = 0;
        for (auto& group : (*doc)["testGroups"].elements()) {
            auto gfield = [&](const char* key) {
                auto s = group[sgcl::string(key)].as_string();
                return s ? std::string(s->data(), s->size()) : std::string();
            };
            for (auto& t : group["tests"].elements()) {
                auto field = [&](const char* key) {
                    auto s = t[sgcl::string(key)].as_string();
                    return s ? std::string(s->data(), s->size()) : std::string();
                };
                auto has = [&](const char* key) { return t.contains(sgcl::string(key)); };
                each(gfield, field, has);
                ++n;
            }
        }
        return n;
    }

    sgcl::slice<const byte> view(const curve_test::bytes_t& v) {
        return curve_test::view(v);
    }

    template<class Private, class Public>
    void sign_vectors(const char* file, bool seeded) {
        using P = typename Private::params;
        size_t checked = 0;
        size_t n = wycheproof(file, [&](auto group, auto field, auto has) {
            const std::string id = field("tcId");
            const bool valid = field("result") == "valid";
            optional<Private> key;
            if (seeded) {
                auto k = Private::from_seed(view(curve_test::unhex(group("privateSeed"))));
                if (k) {
                    key.emplace(std::move(*k));
                }
            } else {
                key = sgcl::crypto::detail::mldsa::Access::from_expanded<Private>(view(curve_test::unhex(group("privateKey"))));
            }
            if (!key) {
                EXPECT_FALSE(valid) << file << " " << id;
                return;
            }
            EXPECT_EQ(curve_test::hex(key->public_key().bytes()), group("publicKey")) << file << " " << id;
            auto ctx = curve_test::unhex(field("ctx"));
            auto rnd = curve_test::unhex(field("rnd"));
            if (rnd.empty()) {
                rnd.assign(32, 0);
            }
            std::string sig;
            if (has("msg") && rnd == curve_test::bytes_t(32, 0)) {
                if (ctx.size() > 255) {
                    EXPECT_THROW((void)key->sign(view(curve_test::unhex(field("msg"))), {.context = view(ctx), .deterministic = true}), std::invalid_argument);
                    EXPECT_FALSE(valid) << file << " " << id;
                    return;
                }
                sig = curve_test::hex(key->sign(view(curve_test::unhex(field("msg"))), {.context = view(ctx), .deterministic = true}));
            } else {
                auto mu = curve_test::unhex(field("mu"));
                ASSERT_EQ(mu.size(), 64u) << file << " " << id;
                sig = curve_test::hex(sgcl::crypto::detail::mldsa::Access::sign_mu(*key, mu.data(), rnd.data()));
            }
            if (valid) {
                EXPECT_EQ(sig, field("sig")) << file << " " << id;
                if (has("msg")) {
                    EXPECT_TRUE(key->public_key().verify(view(curve_test::unhex(field("msg"))), view(curve_test::unhex(sig)), {.context = view(ctx)}))
                        << file << " " << id;
                }
                ++checked;
            }
            (void)sizeof(P);
        });
        if (n == 0) {
            GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/" << file;
        }
        std::printf("[ wycheproof ] %s: %zu cases, %zu signatures compared\n", file, n, checked);
    }

    template<class Public>
    void verify_vectors(const char* file) {
        size_t n = wycheproof(file, [&](auto group, auto field, auto) {
            const std::string id = field("tcId");
            const bool valid = field("result") == "valid";
            auto pub = Public::from_bytes(view(curve_test::unhex(group("publicKey"))));
            if (!pub) {
                EXPECT_FALSE(valid) << file << " " << id;
                return;
            }
            auto ctx = curve_test::unhex(field("ctx"));
            const bool ok = pub->verify(view(curve_test::unhex(field("msg"))), view(curve_test::unhex(field("sig"))), {.context = view(ctx)});
            EXPECT_EQ(ok, valid) << file << " " << id << " " << field("comment");
        });
        if (n == 0) {
            GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/" << file;
        }
        std::printf("[ wycheproof ] %s: %zu cases\n", file, n);
    }

    std::string run(const std::string& command) {
        std::string out;
        FILE* p = popen(command.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t got;
        while ((got = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, got);
        }
        pclose(p);
        return out;
    }
}

TEST(MlDsa_Ring, TheTransformAgainstTheSchoolbookProduct) {
    std::mt19937 rng(7);
    for (int round = 0; round < 20; ++round) {
        Poly f = random_poly(rng, Q - 1), g = random_poly(rng, round % 2 ? 4 : (1 << 19));
        Poly fh = f, gh = g;
        dsa::ntt(fh);
        dsa::ntt(gh);
        for (auto c : fh) {
            ASSERT_LT(std::abs(c), 9 * Q);
        }
        Poly h;
        dsa::pointwise(h, fh, gh);
        dsa::inverse_ntt(h);
        Poly expected = schoolbook(f, g);
        for (size_t i = 0; i < dsa::N; ++i) {
            ASSERT_LT(std::abs(h[i]), Q);
            ASSERT_EQ(dsa::add_q(h[i]), expected[i]) << i;
        }
    }
    // the inverse of the transform alone: a pointwise product by the transform of 1
    Poly one{};
    one[0] = 1;
    Poly oh = one;
    dsa::ntt(oh);
    Poly f = random_poly(rng, Q - 1), fh = f;
    dsa::ntt(fh);
    Poly back;
    dsa::pointwise(back, fh, oh);
    dsa::inverse_ntt(back);
    for (size_t i = 0; i < dsa::N; ++i) {
        ASSERT_EQ(dsa::add_q(back[i]), mod_q(f[i]));
    }
}

// The NEON path (where it is compiled) against the portable one, value for value
TEST(MlDsa_Ring, BothPathsOfTheTransform) {
    std::mt19937 rng(5);
    for (int round = 0; round < 2000; ++round) {
        Poly f = random_poly(rng, round % 3 ? Q - 1 : (1 << 19));
        Poly a = f, b = f;
        dsa::ntt(a);
        dsa::ntt_portable(b);
        ASSERT_EQ(a, b) << round;
        Poly c = f, d = f;
        dsa::inverse_ntt(c);
        dsa::inverse_ntt_portable(d);
        ASSERT_EQ(c, d) << round;
    }
}

TEST(MlDsa_Ring, TheReductionsAndTheRounding) {
    std::mt19937 rng(3);
    for (int i = 0; i < 1000000; ++i) {
        const int32_t a = int32_t(rng());
        ASSERT_EQ(dsa::freeze(a), mod_q(a)) << a;
        const int64_t m = (int64_t(int32_t(rng())) * int32_t(rng() % Q)) % (int64_t(Q) << 31);
        const int32_t r = dsa::montgomery(m);
        ASSERT_LT(std::abs(r), Q);
        ASSERT_EQ(mod_q(int64_t(r) * ((int64_t(1) << 32) % Q)), mod_q(m)) << m;
    }
    for (int32_t r = 0; r < Q; ++r) {
        int32_t r1, r0;
        dsa::power2round(r, r1, r0);
        ASSERT_EQ(r1 * (1 << 13) + r0, r);
        ASSERT_TRUE(r0 > -(1 << 12) && r0 <= (1 << 12)) << r;
        // Decompose, both γ2, against FIPS 204's definition
        for (int32_t g2 : {(Q - 1) / 88, (Q - 1) / 32}) {
            int32_t e0 = r % (2 * g2);
            if (e0 > g2) {
                e0 -= 2 * g2;
            }
            int32_t e1;
            if (r - e0 == Q - 1) {
                e1 = 0;
                e0 -= 1;
            } else {
                e1 = (r - e0) / (2 * g2);
            }
            if (g2 == (Q - 1) / 88) {
                dsa::decompose<dsa::Params44>(r, r1, r0);
            } else {
                dsa::decompose<dsa::Params65>(r, r1, r0);
            }
            ASSERT_EQ(r1, e1) << r << " " << g2;
            ASSERT_EQ(r0, e0) << r << " " << g2;
        }
        ASSERT_EQ(dsa::centered_abs(r), uint32_t(r > (Q - 1) / 2 ? Q - r : r));
    }
}

TEST(MlDsa_Wycheproof, KeysAndSignaturesFromSeeds) {
    sign_vectors<sgcl::crypto::mldsa44::private_key, sgcl::crypto::mldsa44::public_key>("mldsa_44_sign_seed_test.json", true);
    sign_vectors<sgcl::crypto::mldsa65::private_key, sgcl::crypto::mldsa65::public_key>("mldsa_65_sign_seed_test.json", true);
    sign_vectors<sgcl::crypto::mldsa87::private_key, sgcl::crypto::mldsa87::public_key>("mldsa_87_sign_seed_test.json", true);
}

TEST(MlDsa_Wycheproof, SignaturesOfExpandedKeys) {
    sign_vectors<sgcl::crypto::mldsa44::private_key, sgcl::crypto::mldsa44::public_key>("mldsa_44_sign_noseed_test.json", false);
    sign_vectors<sgcl::crypto::mldsa65::private_key, sgcl::crypto::mldsa65::public_key>("mldsa_65_sign_noseed_test.json", false);
    sign_vectors<sgcl::crypto::mldsa87::private_key, sgcl::crypto::mldsa87::public_key>("mldsa_87_sign_noseed_test.json", false);
}

TEST(MlDsa_Wycheproof, Verification) {
    verify_vectors<sgcl::crypto::mldsa44::public_key>("mldsa_44_verify_test.json");
    verify_vectors<sgcl::crypto::mldsa65::public_key>("mldsa_65_verify_test.json");
    verify_vectors<sgcl::crypto::mldsa87::public_key>("mldsa_87_verify_test.json");
}

namespace {
    template<class Private, class Public>
    void against_go(const std::string& tool, const char* set) {
        std::mt19937 rng(11);
        for (int round = 0; round < 8; ++round) {
            curve_test::bytes_t seed(32), msg(rng() % 200), ctx(round % 2 ? rng() % 256 : 0);
            for (auto* v : {&seed, &msg, &ctx}) {
                for (auto& b : *v) {
                    b = uint8_t(rng());
                }
            }
            auto key = Private::from_seed(view(seed)).value();
            auto hedged = key.sign(view(msg), {.context = view(ctx)});
            std::istringstream out(run(tool + " " + set + " " + curve_test::hex(seed) + " " + (msg.empty() ? "-" : curve_test::hex(msg)) + " " +
                                       (ctx.empty() ? "-" : curve_test::hex(ctx)) + " " + curve_test::hex(hedged)));
            std::string pk, sig, verified;
            out >> pk >> sig >> verified;
            ASSERT_FALSE(verified.empty()) << "the Go oracle did not answer";
            EXPECT_EQ(curve_test::hex(key.public_key().bytes()), pk) << set;
            EXPECT_EQ(curve_test::hex(key.sign(view(msg), {.context = view(ctx), .deterministic = true})), sig) << set;
            EXPECT_EQ(verified, "true") << set << ": Go does not verify our hedged signature";
            // Go's deterministic signature verifies here, and not under another context
            auto pub = Public::from_bytes(view(curve_test::unhex(pk))).value();
            EXPECT_TRUE(pub.verify(view(msg), view(curve_test::unhex(sig)), {.context = view(ctx)}));
            curve_test::bytes_t other = ctx;
            other.push_back(1);
            EXPECT_FALSE(pub.verify(view(msg), view(curve_test::unhex(sig)), {.context = view(other)}));
        }
    }
}

TEST(MlDsa_Go, BothWays) {
    // built in a directory of its own, a module of the standard library alone (nothing downloaded)
    auto dir = std::filesystem::temp_directory_path() / ("sgcl_mldsa_go_" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    std::filesystem::copy_file(source_root() / "tests/crypto/go_mldsa/main.go", dir / "main.go", std::filesystem::copy_options::overwrite_existing);
    std::ofstream(dir / "go.mod") << "module mldsaoracle\n\ngo 1.27\n";
    const std::string tool = (dir / "go_mldsa").string();
    if (std::system(("cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off go build -o '" + tool + "' . > build.log 2>&1").c_str()) != 0) {
        GTEST_SKIP() << "no Go toolchain with crypto/mldsa";
    }
    against_go<sgcl::crypto::mldsa44::private_key, sgcl::crypto::mldsa44::public_key>(tool, "44");
    against_go<sgcl::crypto::mldsa65::private_key, sgcl::crypto::mldsa65::public_key>(tool, "65");
    against_go<sgcl::crypto::mldsa87::private_key, sgcl::crypto::mldsa87::public_key>(tool, "87");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(MlDsa, Boundaries) {
    using namespace sgcl::crypto;
    auto key = mldsa65::private_key::generate();
    auto pub = key.public_key();
    // the empty message, a megabyte, a context of 255 bytes; a context of 256 refused on both sides
    for (size_t n : {size_t(0), size_t(1), size_t(1) << 20}) {
        sgcl::string m(std::string(n, 'm'));
        auto sig = key.sign(m);
        EXPECT_EQ(sig.size(), mldsa65::signature_size);
        EXPECT_TRUE(pub.verify(m, sig));
        EXPECT_FALSE(pub.verify(m + sgcl::string("x"), sig));
    }
    sgcl::string c255(std::string(255, 'c')), c256(std::string(256, 'c'));
    auto sc = key.sign("m", {.context = c255});
    EXPECT_TRUE(pub.verify("m", sc, {.context = c255}));
    EXPECT_FALSE(pub.verify("m", sc));
    EXPECT_THROW((void)key.sign("m", {.context = c256}), std::invalid_argument);
    EXPECT_FALSE(pub.verify("m", sc, {.context = c256}));
    // hedged signatures differ, deterministic ones do not; both verify
    EXPECT_FALSE(key.sign("m") == key.sign("m"));
    EXPECT_TRUE(key.sign("m", {.deterministic = true}) == key.sign("m", {.deterministic = true}));
    // every flipped bit of a signature, a short or a long one, refused
    auto sig = key.sign("m");
    for (size_t i = 0; i < sig.size(); i += 7) {
        auto bad = sig;
        bad[i] = byte(uint8_t(bad[i]) ^ (1 << (i % 8)));
        EXPECT_FALSE(pub.verify("m", bad)) << i;
    }
    EXPECT_FALSE(pub.verify("m", slice<const byte>(sig.data(), sig.size() - 1)));
    auto longer = sig;
    longer.push_back(byte(0));
    EXPECT_FALSE(pub.verify("m", longer));
    EXPECT_FALSE(pub.verify("m", slice<const byte>()));
    // keys: from_seed and seed() round trip, clone, equality, the public key's bytes
    auto again = mldsa65::private_key::from_seed(key.seed().bytes()).value();
    EXPECT_TRUE(again == key);
    EXPECT_TRUE(again.public_key() == pub);
    auto copy = key.clone();
    EXPECT_TRUE(copy == key);
    EXPECT_FALSE(mldsa65::private_key::generate() == key);
    EXPECT_FALSE(mldsa65::private_key::from_seed(slice<const byte>(sig.data(), 31)));
    EXPECT_FALSE(mldsa65::private_key::from_seed(slice<const byte>(sig.data(), 33)));
    EXPECT_FALSE(mldsa65::public_key::from_bytes(slice<const byte>(sig.data(), mldsa65::public_key_size - 1)));
    EXPECT_FALSE(mldsa65::public_key::from_bytes(slice<const byte>()));
    auto pb = pub.bytes();
    EXPECT_EQ(pb.size(), mldsa65::public_key_size);
    EXPECT_TRUE(mldsa65::public_key::from_bytes(pb).value() == pub);
    // another parameter set's signature does not verify
    auto k44 = mldsa44::private_key::generate();
    EXPECT_FALSE(pub.verify("m", k44.sign("m")));
    // a key moved from: std::logic_error; equal only to another one moved from
    auto moved = std::move(copy);
    EXPECT_THROW((void)copy.sign("m"), std::logic_error);
    EXPECT_THROW((void)copy.public_key(), std::logic_error);
    EXPECT_THROW((void)copy.seed(), std::logic_error);
    EXPECT_THROW((void)copy.clone(), std::logic_error);
    EXPECT_THROW((void)(copy == key), std::logic_error);
    EXPECT_THROW((void)(key == copy), std::logic_error);
    EXPECT_TRUE(moved == key);
    // a public key copied is the same handle
    auto pub2 = pub;
    EXPECT_TRUE(pub2 == pub);
    EXPECT_TRUE(pub2.verify("m", sig));
}

TEST(MlDsa, ThreadsSignAndVerifyWithOneKey) {
    using namespace sgcl::crypto;
    auto key = mldsa44::private_key::generate();
    auto pub = key.public_key();
    std::atomic<int> good{0};
    auto work = [&](int t) {
        for (int i = 0; i < 20; ++i) {
            sgcl::string m(std::to_string(t) + ":" + std::to_string(i));
            auto s = key.sign(m);
            good += pub.verify(m, s);
        }
    };
    sgcl::thread a(work, 0), b(work, 1), c(work, 2), d(work, 3);
    a.join();
    b.join();
    c.join();
    d.join();
    EXPECT_EQ(good.load(), 80);
}
