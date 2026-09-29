//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-KEM (FIPS 203). The ring — the field Z_q, the transform and
// its inverse, the product of transforms against the schoolbook product
// modulo X^256 + 1, Compress and Decompress on every value against the
// exact formula, the byte encodings; K-PKE and the key generation
// against Wycheproof's vectors of the three parameter sets; the public
// types, encapsulation and decapsulation against the rest of Wycheproof,
// their contract, and the key's memory zeroed.
#include "curve25519_common.h"

#include "sgcl/crypto/mlkem.h"
#include "sgcl/encoding/json.h"

#include <cstdint>
#include <random>
#include <vector>

namespace mlkem = sgcl::crypto::detail::mlkem;
using mlkem::Poly;
using mlkem::Q;

namespace {
    Poly random_poly(std::mt19937& rng) {
        Poly f;
        for (auto& c : f) {
            c = uint16_t(rng() % Q);
        }
        return f;
    }

    // The product modulo X^256 + 1 and q, by the school's method
    Poly schoolbook(const Poly& f, const Poly& g) {
        std::vector<int64_t> h(2 * mlkem::N, 0);
        for (size_t i = 0; i < mlkem::N; ++i) {
            for (size_t j = 0; j < mlkem::N; ++j) {
                h[i + j] += int64_t(f[i]) * g[j];
            }
        }
        Poly r;
        for (size_t i = 0; i < mlkem::N; ++i) {
            int64_t v = (h[i] - h[i + mlkem::N]) % Q;   // X^256 = −1
            r[i] = uint16_t(v < 0 ? v + Q : v);
        }
        return r;
    }
}

TEST(MlKem_Ring, TheFieldAgainstPlainArithmetic) {
    for (uint32_t a = 0; a < Q; a += 7) {
        for (uint32_t b = 0; b < Q; b += 11) {
            ASSERT_EQ(mlkem::add(uint16_t(a), uint16_t(b)), (a + b) % Q);
            ASSERT_EQ(mlkem::sub(uint16_t(a), uint16_t(b)), (a + Q - b) % Q);
            ASSERT_EQ(mlkem::mul(uint16_t(a), uint16_t(b)), (a * b) % Q);
        }
    }
    for (uint32_t a : {0u, 1u, Q - 1u, uint32_t(Q) * (Q - 1), (Q - 1u) * (Q - 1u), (1u << 24) - 1}) {
        EXPECT_EQ(mlkem::reduce(a), a % Q) << a;
    }
    std::mt19937 rng(1);
    for (int i = 0; i < 1000000; ++i) {
        uint32_t a = rng() & ((1u << 24) - 1);
        ASSERT_EQ(mlkem::reduce(a), a % Q) << a;
    }
}

// FIPS 203 Appendix A: the first and the last of each table
TEST(MlKem_Ring, TheTablesAreAppendixA) {
    EXPECT_EQ(mlkem::tables.zetas[0], 1);
    EXPECT_EQ(mlkem::tables.zetas[1], 1729);
    EXPECT_EQ(mlkem::tables.zetas[2], 2580);
    EXPECT_EQ(mlkem::tables.zetas[3], 3289);
    EXPECT_EQ(mlkem::tables.zetas[4], 2642);
    EXPECT_EQ(mlkem::tables.zetas[5], 630);
    EXPECT_EQ(mlkem::tables.zetas[6], 1897);
    EXPECT_EQ(mlkem::tables.zetas[7], 848);
    for (unsigned i = 0; i < 128; ++i) {   // and every one of them against plain powers
        unsigned r = 0;
        for (unsigned k = 0; k < 7; ++k) {
            r |= ((i >> k) & 1) << (6 - k);
        }
        uint32_t z = 1, g = 1;
        for (unsigned k = 0; k < r; ++k) {
            z = z * 17 % Q;
        }
        for (unsigned k = 0; k < 2 * r + 1; ++k) {
            g = g * 17 % Q;
        }
        ASSERT_EQ(mlkem::tables.zetas[i], z) << i;
        ASSERT_EQ(mlkem::tables.gammas[i], g) << i;
    }
    EXPECT_EQ(mlkem::tables.gammas[0], 17);
    EXPECT_EQ(mlkem::tables.gammas[1], Q - 17);
    EXPECT_EQ(mlkem::tables.gammas[2], 2761);
    EXPECT_EQ(mlkem::tables.gammas[3], Q - 2761);
    EXPECT_EQ(mlkem::mul(128, mlkem::InverseOf128), 1);
}

TEST(MlKem_Ring, TheTransformIsInvertible) {
    std::mt19937 rng(2);
    for (int i = 0; i < 10000; ++i) {
        Poly f = random_poly(rng);
        Poly g = f;
        mlkem::ntt(g);
        mlkem::ntt_inverse(g);
        ASSERT_EQ(g, f) << i;
    }
}

// Algorithm 11 is the product in the ring: NTT⁻¹(NTT(f) × NTT(g)) = f·g
TEST(MlKem_Ring, TheProductOfTransformsIsTheRingsProduct) {
    std::mt19937 rng(3);
    for (int i = 0; i < 300; ++i) {
        Poly f = random_poly(rng), g = random_poly(rng);
        Poly nf = f, ng = g, h;
        mlkem::ntt(nf);
        mlkem::ntt(ng);
        mlkem::multiply_ntts(h, nf, ng);
        mlkem::ntt_inverse(h);
        ASSERT_EQ(h, schoolbook(f, g)) << i;
        Poly acc = {};
        mlkem::multiply_ntts_add(acc, nf, ng);
        mlkem::multiply_ntts_add(acc, nf, ng);
        mlkem::ntt_inverse(acc);
        Poly twice = schoolbook(f, g);
        for (auto& c : twice) {
            c = uint16_t((2u * c) % Q);
        }
        ASSERT_EQ(acc, twice) << i;
    }
    // X · X^255 = X^256 = −1
    Poly x = {}, y = {};
    x[1] = 1;
    y[255] = 1;
    mlkem::ntt(x);
    mlkem::ntt(y);
    Poly h;
    mlkem::multiply_ntts(h, x, y);
    mlkem::ntt_inverse(h);
    Poly minus_one = {};
    minus_one[0] = Q - 1;
    EXPECT_EQ(h, minus_one);
}

// Compress_d on every x < q for every d the standard uses and every other
// d below 12, against ⌈(2^d/q)·x⌋ mod 2^d computed exactly: ⌊(x·2^(d+1) + q)
// / 2q⌋. Decompress_d on every y < 2^d against ⌊(2qy + 2^d) / 2^(d+1)⌋, and
// the round trip within ⌈q / 2^(d+1)⌋ (FIPS 203 §4.2.1)
TEST(MlKem_Ring, CompressOnEveryValue) {
    for (unsigned d = 1; d <= 11; ++d) {
        for (uint32_t x = 0; x < Q; ++x) {
            uint32_t exact = ((x << (d + 1)) + Q) / (2 * Q) % (1u << d);
            ASSERT_EQ(mlkem::compress(uint16_t(x), d), exact) << "d " << d << " x " << x;
            uint32_t back = mlkem::decompress(mlkem::compress(uint16_t(x), d), d);
            int32_t diff = int32_t(back) - int32_t(x);
            diff = ((diff % int32_t(Q)) + int32_t(Q)) % int32_t(Q);
            if (diff > int32_t(Q / 2)) {
                diff -= Q;
            }
            uint32_t bound = (Q + (1u << (d + 1)) - 1) >> (d + 1);   // ⌈q / 2^(d+1)⌉
            ASSERT_LE(uint32_t(diff < 0 ? -diff : diff), bound) << "d " << d << " x " << x;
        }
        for (uint32_t y = 0; y < (1u << d); ++y) {
            uint32_t exact = (2 * Q * y + (1u << d)) / (1u << (d + 1));
            ASSERT_EQ(mlkem::decompress(uint16_t(y), d), exact) << "d " << d << " y " << y;
            ASSERT_EQ(mlkem::compress(mlkem::decompress(uint16_t(y), d), d), y) << "d " << d << " y " << y;   // exact the other way
        }
    }
}

TEST(MlKem_Ring, TheByteEncodings) {
    std::mt19937 rng(4);
    for (unsigned d = 1; d <= 12; ++d) {
        for (int i = 0; i < 200; ++i) {
            Poly f;
            for (auto& c : f) {
                c = uint16_t(d == 12 ? rng() % Q : rng() & ((1u << d) - 1));
            }
            std::vector<uint8_t> bytes(32 * d);
            mlkem::byte_encode(bytes.data(), f, d);
            // bit j of value i is bit i·d + j of the bytes, little-endian
            for (size_t k = 0; k < mlkem::N; ++k) {
                for (unsigned j = 0; j < d; ++j) {
                    size_t bit = k * d + j;
                    ASSERT_EQ((bytes[bit / 8] >> (bit % 8)) & 1, (f[k] >> j) & 1);
                }
            }
            Poly g;
            mlkem::byte_decode(g, bytes.data(), d);
            ASSERT_EQ(g, f) << "d " << d;
        }
    }
    // d = 12: a value of 12 bits at q or above is taken mod q (§7.2
    // compares the encoding again to find such keys)
    std::vector<uint8_t> high(32 * 12, 0xFF);
    Poly g;
    mlkem::byte_decode(g, high.data(), 12);
    for (auto c : g) {
        ASSERT_EQ(c, 4095 % Q);
    }
}

namespace {
    namespace core = sgcl::crypto::detail::mlkem;

    // A Wycheproof file's tests, each asked for its string fields by name
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
            for (auto& t : group["tests"].elements()) {
                auto field = [&](const char* key) {
                    auto s = t[sgcl::string(key)].as_string();
                    return s ? std::string(s->data(), s->size()) : std::string();
                };
                each(field);
                ++n;
            }
        }
        return n;
    }

    template<class P>
    size_t keygen_vectors(const char* file) {
        return wycheproof(file, [](auto field) {
            auto seed = curve_test::unhex(field("seed"));
            ASSERT_EQ(seed.size(), 64u) << field("tcId");
            std::vector<uint8_t> ek(core::Sizes<P>::ek), dk(core::Sizes<P>::dk);
            core::keygen_internal<P>(ek.data(), dk.data(), seed.data(), seed.data() + 32);
            EXPECT_EQ(curve_test::bytes_t(ek.begin(), ek.end()), curve_test::unhex(field("ek"))) << field("comment");
            EXPECT_EQ(curve_test::bytes_t(dk.begin(), dk.end()), curve_test::unhex(field("dk"))) << field("comment");
        });
    }

    // K-PKE alone: what is encrypted decrypts to itself, for random keys
    // and messages
    template<class P>
    void pke_round_trips(unsigned seed) {
        std::mt19937 rng(seed);
        for (int i = 0; i < 200; ++i) {
            uint8_t d[32], m[32], r[32], back[32];
            for (auto* a : {d, m, r}) {
                for (int k = 0; k < 32; ++k) {
                    a[k] = uint8_t(rng());
                }
            }
            std::vector<uint8_t> ek(core::Sizes<P>::ek), dk(core::Sizes<P>::dk_pke), c(core::Sizes<P>::ciphertext);
            core::pke_keygen<P>(ek.data(), dk.data(), d);
            core::pke_encrypt<P>(c.data(), ek.data(), m, r);
            core::pke_decrypt<P>(back, dk.data(), c.data());
            ASSERT_EQ(std::memcmp(back, m, 32), 0) << i;
        }
    }
}

TEST(MlKem_Pke, WhatIsEncryptedDecrypts) {
    pke_round_trips<core::Params512>(5);
    pke_round_trips<core::Params768>(6);
    pke_round_trips<core::Params1024>(7);
}

// Wycheproof's mlkem_*_keygen_seed_test.json: the seed d‖z gives the
// encapsulation key and the decapsulation key byte for byte
TEST(MlKem_Pke, WycheproofKeyGeneration) {
    size_t n512 = keygen_vectors<core::Params512>("mlkem_512_keygen_seed_test.json");
    size_t n768 = keygen_vectors<core::Params768>("mlkem_768_keygen_seed_test.json");
    size_t n1024 = keygen_vectors<core::Params1024>("mlkem_1024_keygen_seed_test.json");
    if (n512 + n768 + n1024 == 0) {
        GTEST_SKIP() << "the Wycheproof files are not on disk";
    }
    EXPECT_GT(n512, 0u);
    EXPECT_EQ(n768, 100u);
    EXPECT_GT(n1024, 0u);
}

#include <new>
#include <type_traits>

namespace {
    using curve_test::bytes_t;
    using curve_test::unhex;

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    template<class T>
    bytes_t to_bytes(const T& r) {
        auto p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    template<size_t N>
    bytes_t secret_bytes(const sgcl::crypto::secret<N>& s) {
        return to_bytes(s.bytes());
    }

    // mlkem_*_encaps_test.json: m and ek give c and K; an invalid key
    // (a coefficient not below q, a wrong length) is refused when read
    template<class Ek>
    size_t encaps_vectors(const char* file) {
        return wycheproof(file, [](auto field) {
            auto ek = Ek::from_bytes(view(unhex(field("ek"))));
            if (field("result") == "invalid") {
                ASSERT_FALSE(ek) << field("tcId");
                EXPECT_EQ(ek.error().code(), sgcl::crypto::errc::invalid_key);
                return;
            }
            ASSERT_TRUE(ek) << field("tcId");
            auto m = unhex(field("m"));
            ASSERT_EQ(m.size(), 32u);
            auto e = sgcl::crypto::detail::mlkem::Access::encapsulate_with(*ek, m.data());
            EXPECT_EQ(to_bytes(e.ciphertext), unhex(field("c"))) << field("tcId");
            EXPECT_EQ(secret_bytes(e.shared_key), unhex(field("K"))) << field("tcId");
        });
    }

    // mlkem_*_test.json: the seed's key decapsulates c to K (genuine or
    // implicitly rejected); a seed or a ciphertext of a wrong length is
    // refused, and the encapsulation key is the one given
    template<class Dk>
    size_t decaps_vectors(const char* file) {
        return wycheproof(file, [](auto field) {
            auto dk = Dk::from_seed(view(unhex(field("seed"))));
            if (!dk) {
                EXPECT_EQ(field("result"), "invalid") << field("tcId");
                EXPECT_EQ(dk.error().code(), sgcl::crypto::errc::invalid_key);
                return;
            }
            if (!field("ek").empty()) {
                EXPECT_EQ(to_bytes(dk->encapsulation_key().bytes()), unhex(field("ek"))) << field("tcId");
            }
            auto k = dk->decapsulate(view(unhex(field("c"))));
            if (field("result") == "invalid") {
                ASSERT_FALSE(k) << field("tcId");
                EXPECT_EQ(k.error().code(), sgcl::crypto::errc::malformed);
                return;
            }
            ASSERT_TRUE(k) << field("tcId");
            EXPECT_EQ(secret_bytes(*k), unhex(field("K"))) << field("tcId");
        });
    }

    // mlkem_*_semi_expanded_decaps_test.json: an expanded key given from
    // outside, through detail (the public type keeps a seed): §7.3's
    // checks, then decapsulation
    template<class P>
    size_t expanded_vectors(const char* file) {
        return wycheproof(file, [](auto field) {
            auto dk = unhex(field("dk"));
            auto c = unhex(field("c"));
            bool valid = core::decapsulation_key_valid<P>(dk.data(), dk.size()) && c.size() == core::Sizes<P>::ciphertext;
            EXPECT_EQ(valid, field("result") == "valid") << field("tcId") << " " << field("comment");
            if (valid) {
                uint8_t k[32];
                core::decaps_internal<P>(k, dk.data(), c.data());
                EXPECT_EQ(bytes_t(k, k + 32), unhex(field("K"))) << field("tcId");
            }
        });
    }

    template<class Dk, class Ek, size_t EkSize, size_t CtSize>
    void contract() {
        static_assert(!std::is_copy_constructible_v<Dk>);
        static_assert(!std::is_copy_assignable_v<Dk>);
        static_assert(std::is_nothrow_move_constructible_v<Dk>);
        static_assert(std::is_copy_constructible_v<Ek>);
        auto dk = Dk::generate();
        auto ek = dk.encapsulation_key();
        EXPECT_EQ(ek.bytes().size(), EkSize);
        auto e = ek.encapsulate();
        EXPECT_EQ(e.ciphertext.size(), CtSize);
        auto k = dk.decapsulate(e.ciphertext.as_slice());
        ASSERT_TRUE(k);
        EXPECT_EQ(secret_bytes(*k), secret_bytes(e.shared_key));
        auto again = ek.encapsulate();                         // a fresh m each time
        EXPECT_NE(to_bytes(again.ciphertext), to_bytes(e.ciphertext));
        // a ciphertext changed in one bit: another key, pseudorandom, no error
        auto bad = to_bytes(e.ciphertext);
        bad[7] ^= 1;
        auto rejected = dk.decapsulate(view(bad));
        ASSERT_TRUE(rejected);
        EXPECT_NE(secret_bytes(*rejected), secret_bytes(e.shared_key));
        auto rejected_again = dk.decapsulate(view(bad));
        EXPECT_EQ(secret_bytes(*rejected_again), secret_bytes(*rejected));   // deterministic: J(z‖c)
        // lengths
        bad.pop_back();
        EXPECT_EQ(error_of(dk.decapsulate(view(bad))).code(), sgcl::crypto::errc::malformed);
        EXPECT_EQ(error_of(Dk::from_seed(view(bytes_t(63)))).code(), sgcl::crypto::errc::invalid_key);
        EXPECT_EQ(error_of(Ek::from_bytes(view(bytes_t(EkSize - 1)))).code(), sgcl::crypto::errc::invalid_key);
        // a coefficient of q: 0xFFF in the first twelve bits
        auto over = to_bytes(ek.bytes());
        over[0] = 0xFF;
        over[1] |= 0x0F;
        EXPECT_EQ(error_of(Ek::from_bytes(view(over))).code(), sgcl::crypto::errc::invalid_key);
        // the key read back from its bytes and from its seed is the same key
        auto read = Ek::from_bytes(ek.bytes().as_slice());
        ASSERT_TRUE(read);
        EXPECT_TRUE(*read == ek);
        auto same = Dk::from_seed(dk.seed().bytes());
        ASSERT_TRUE(same);
        EXPECT_TRUE(*same == dk);
        EXPECT_TRUE(same->encapsulation_key() == ek);
        auto c = dk.clone();
        EXPECT_TRUE(c == dk);
        // moved from: zeroed, and no operation of it runs
        auto moved = std::move(c);
        EXPECT_TRUE(moved == dk);
        EXPECT_THROW((void)c.decapsulate(e.ciphertext.as_slice()), std::logic_error);
        EXPECT_THROW((void)c.encapsulation_key(), std::logic_error);
        EXPECT_THROW((void)c.seed(), std::logic_error);
    }

    template<class T>
    bool all_zero(const T& object) {
        auto p = reinterpret_cast<const unsigned char*>(&object);
        for (size_t i = 0; i < sizeof(T); ++i) {
            if (p[i] != 0) {
                return false;
            }
        }
        return true;
    }

    // The key's memory after a move and after its destructor: zeros (a
    // probe reads the storage afterwards, as for x25519)
    template<class Dk>
    void zeroed() {
        auto k = Dk::generate();
        EXPECT_FALSE(all_zero(k));
        auto moved = std::move(k);
        EXPECT_TRUE(all_zero(k));
        Dk assigned = Dk::generate();
        assigned = std::move(moved);
        EXPECT_TRUE(all_zero(moved));
        alignas(Dk) unsigned char storage[sizeof(Dk)];
        auto* probe = new (storage) Dk(Dk::generate());
        EXPECT_FALSE(all_zero(*probe));
        probe->~Dk();
        for (unsigned char b : storage) {
            ASSERT_EQ(b, 0);
        }
    }
}

TEST(MlKem, WycheproofEncapsulation) {
    size_t a = encaps_vectors<sgcl::crypto::mlkem512::encapsulation_key>("mlkem_512_encaps_test.json");
    size_t b = encaps_vectors<sgcl::crypto::mlkem768::encapsulation_key>("mlkem_768_encaps_test.json");
    size_t c = encaps_vectors<sgcl::crypto::mlkem1024::encapsulation_key>("mlkem_1024_encaps_test.json");
    if (a + b + c == 0) {
        GTEST_SKIP() << "the Wycheproof files are not on disk";
    }
    EXPECT_GT(a, 0u);
    EXPECT_EQ(b, 265u);
    EXPECT_GT(c, 0u);
}

TEST(MlKem, WycheproofDecapsulation) {
    size_t a = decaps_vectors<sgcl::crypto::mlkem512::decapsulation_key>("mlkem_512_test.json");
    size_t b = decaps_vectors<sgcl::crypto::mlkem768::decapsulation_key>("mlkem_768_test.json");
    size_t c = decaps_vectors<sgcl::crypto::mlkem1024::decapsulation_key>("mlkem_1024_test.json");
    if (a + b + c == 0) {
        GTEST_SKIP() << "the Wycheproof files are not on disk";
    }
    EXPECT_GT(a, 0u);
    EXPECT_EQ(b, 201u);
    EXPECT_GT(c, 0u);
}

TEST(MlKem, WycheproofExpandedKeys) {
    size_t a = expanded_vectors<core::Params512>("mlkem_512_semi_expanded_decaps_test.json");
    size_t b = expanded_vectors<core::Params768>("mlkem_768_semi_expanded_decaps_test.json");
    size_t c = expanded_vectors<core::Params1024>("mlkem_1024_semi_expanded_decaps_test.json");
    if (a + b + c == 0) {
        GTEST_SKIP() << "the Wycheproof files are not on disk";
    }
    EXPECT_GT(a, 0u);
    EXPECT_EQ(b, 9u);
    EXPECT_GT(c, 0u);
}

// A key is made only of bytes the library has checked, and one parameter
// set's result is not another's
static_assert(!std::is_constructible_v<sgcl::crypto::mlkem768::encapsulation_key, sgcl::crypto::detail::mlkem::Made, const uint8_t*>);
static_assert(!std::is_constructible_v<sgcl::crypto::mlkem768::decapsulation_key, sgcl::crypto::detail::mlkem::Made>);
static_assert(!std::is_same_v<sgcl::crypto::mlkem512::encapsulation, sgcl::crypto::mlkem768::encapsulation>);
static_assert(!std::is_convertible_v<sgcl::crypto::mlkem512::encapsulation, sgcl::crypto::mlkem768::encapsulation>);
static_assert(!std::is_convertible_v<sgcl::crypto::mlkem768::encapsulation, sgcl::crypto::mlkem1024::encapsulation>);
static_assert(!std::is_constructible_v<sgcl::crypto::mlkem768::encapsulation, sgcl::crypto::mlkem512::encapsulation&&>);

TEST(MlKem, TheContractOfTheTypes) {
    contract<sgcl::crypto::mlkem512::decapsulation_key, sgcl::crypto::mlkem512::encapsulation_key, 800, 768>();
    contract<sgcl::crypto::mlkem768::decapsulation_key, sgcl::crypto::mlkem768::encapsulation_key, 1184, 1088>();
    contract<sgcl::crypto::mlkem1024::decapsulation_key, sgcl::crypto::mlkem1024::encapsulation_key, 1568, 1568>();
}

TEST(MlKem, TheDecapsulationKeyIsZeroed) {
    zeroed<sgcl::crypto::mlkem512::decapsulation_key>();
    zeroed<sgcl::crypto::mlkem768::decapsulation_key>();
    zeroed<sgcl::crypto::mlkem1024::decapsulation_key>();
}
