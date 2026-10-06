//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SLH-DSA (FIPS 205) against OpenSSL 3.6, which has the twelve parameter
// sets (Go has none): the public key of the same three seeds, the
// deterministic signature of a message under a context byte for byte, each
// side verifying the other's hedged signatures, OpenSSL's raw private key
// read back; base_2b and the address against their definitions; and the
// public types' contract at their boundaries.
#include "curve25519_common.h"

#include "sgcl/crypto/slhdsa.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace slh = sgcl::crypto::detail::slhdsa;
using bytes_t = curve_test::bytes_t;

namespace {
    sgcl::slice<const byte> view(const bytes_t& v) {
        return curve_test::view(v);
    }

    // OpenSSL's key of the three seeds, its public key, its signatures
    struct Ossl {
        EVP_PKEY* key = nullptr;
        std::string name;

        Ossl(const char* alg, const bytes_t& seeds) : name(alg) {
            EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_name(nullptr, alg, nullptr);
            OSSL_PARAM p[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_SLH_DSA_SEED, const_cast<uint8_t*>(seeds.data()), seeds.size()),
                              OSSL_PARAM_construct_end()};
            if (c && EVP_PKEY_keygen_init(c) == 1 && EVP_PKEY_CTX_set_params(c, p) == 1) {
                EVP_PKEY_generate(c, &key);
            }
            EVP_PKEY_CTX_free(c);
        }

        ~Ossl() {
            EVP_PKEY_free(key);
        }

        bytes_t public_key() const {
            size_t n = 0;
            EVP_PKEY_get_raw_public_key(key, nullptr, &n);
            bytes_t out(n);
            EVP_PKEY_get_raw_public_key(key, out.data(), &n);
            return out;
        }

        bytes_t private_key() const {
            size_t n = 0;
            EVP_PKEY_get_raw_private_key(key, nullptr, &n);
            bytes_t out(n);
            EVP_PKEY_get_raw_private_key(key, out.data(), &n);
            return out;
        }

        bytes_t sign(const bytes_t& msg, const bytes_t& ctx, bool deterministic) const {
            EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
            EVP_SIGNATURE* s = EVP_SIGNATURE_fetch(nullptr, name.c_str(), nullptr);
            int det = deterministic ? 1 : 0;
            OSSL_PARAM p[] = {OSSL_PARAM_construct_int(OSSL_SIGNATURE_PARAM_DETERMINISTIC, &det),
                              OSSL_PARAM_construct_octet_string(OSSL_SIGNATURE_PARAM_CONTEXT_STRING, const_cast<uint8_t*>(ctx.data()), ctx.size()),
                              OSSL_PARAM_construct_end()};
            bytes_t out;
            size_t n = 0;
            if (EVP_PKEY_sign_message_init(c, s, p) == 1 && EVP_PKEY_sign(c, nullptr, &n, msg.data(), msg.size()) == 1) {
                out.resize(n);
                if (EVP_PKEY_sign(c, out.data(), &n, msg.data(), msg.size()) != 1) {
                    out.clear();
                }
            }
            EVP_SIGNATURE_free(s);
            EVP_PKEY_CTX_free(c);
            return out;
        }

        bool verify(const bytes_t& msg, const bytes_t& ctx, const sgcl::vector<byte>& sig) const {
            EVP_PKEY_CTX* c = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
            EVP_SIGNATURE* s = EVP_SIGNATURE_fetch(nullptr, name.c_str(), nullptr);
            OSSL_PARAM p[] = {OSSL_PARAM_construct_octet_string(OSSL_SIGNATURE_PARAM_CONTEXT_STRING, const_cast<uint8_t*>(ctx.data()), ctx.size()),
                              OSSL_PARAM_construct_end()};
            bool ok = EVP_PKEY_verify_message_init(c, s, p) == 1 &&
                      EVP_PKEY_verify(c, reinterpret_cast<const uint8_t*>(sig.data()), sig.size(), msg.data(), msg.size()) == 1;
            EVP_SIGNATURE_free(s);
            EVP_PKEY_CTX_free(c);
            return ok;
        }
    };

    template<class Private>
    void against_openssl(const char* alg, bool cross) {
        using P = typename Private::params;
        std::mt19937 rng(uint32_t(P::n * 1000 + P::h + (P::sha2 ? 1 : 0)));
        bytes_t seeds(3 * P::n), msg(rng() % 300), ctx(rng() % 40);
        for (auto* v : {&seeds, &msg, &ctx}) {
            for (auto& b : *v) {
                b = uint8_t(rng());
            }
        }
        Ossl o(alg, seeds);
        ASSERT_NE(o.key, nullptr) << alg << ": OpenSSL without SLH-DSA";
        auto key = slh::Access::from_seeds<Private>(seeds.data());
        auto pub = key.public_key();
        EXPECT_EQ(curve_test::hex(pub.bytes()), curve_test::hex(o.public_key())) << alg;
        EXPECT_EQ(curve_test::hex(key.bytes()), curve_test::hex(o.private_key())) << alg;
        // OpenSSL's raw private key read back is the same key
        auto read = Private::from_bytes(view(o.private_key()));
        ASSERT_TRUE(read) << alg;
        EXPECT_TRUE(*read == key) << alg;
        // the deterministic signature, byte for byte
        auto mine = key.sign(view(msg), {.context = view(ctx), .deterministic = true});
        EXPECT_EQ(curve_test::hex(mine), curve_test::hex(o.sign(msg, ctx, true))) << alg;
        EXPECT_TRUE(pub.verify(view(msg), mine, {.context = view(ctx)})) << alg;
        if (cross) {
            // hedged both ways
            auto hedged = key.sign(view(msg), {.context = view(ctx)});
            EXPECT_TRUE(o.verify(msg, ctx, hedged)) << alg << ": OpenSSL does not verify our hedged signature";
            auto theirs = o.sign(msg, ctx, false);
            sgcl::vector<byte> t(reinterpret_cast<const byte*>(theirs.data()), reinterpret_cast<const byte*>(theirs.data()) + theirs.size());
            EXPECT_TRUE(pub.verify(view(msg), t, {.context = view(ctx)})) << alg;
            EXPECT_FALSE(pub.verify(view(msg), t)) << alg;
        }
    }
}

TEST(SlhDsa_Parts, Base2bAndTheAddress) {
    // base_2b against the bits read one by one, for every b of the sets
    std::mt19937 rng(1);
    for (unsigned b : {4u, 6u, 8u, 9u, 12u, 14u}) {
        bytes_t x(64);
        for (auto& c : x) {
            c = uint8_t(rng());
        }
        const size_t count = x.size() * 8 / b;
        std::vector<uint32_t> out(count);
        slh::base_2b(out.data(), x.data(), b, count);
        for (size_t i = 0; i < count; ++i) {
            uint32_t v = 0;
            for (unsigned j = 0; j < b; ++j) {
                const size_t bit = i * b + j;
                v = (v << 1) | ((x[bit / 8] >> (7 - bit % 8)) & 1);
            }
            ASSERT_EQ(out[i], v) << b << " " << i;
        }
    }
    // the address: big-endian words, type_and_clear clears the last three, ADRSc of §11.2
    slh::Adrs a;
    a.layer(0x01020304);
    a.tree(0x1112131415161718ull);
    a.type_and_clear(slh::ForsTree);
    a.key_pair(0x21222324);
    a.height(0x31323334);
    a.index(0x41424344);
    EXPECT_EQ(curve_test::hex(a.b, 32), "01020304000000001112131415161718000000032122232431323334" "41424344");
    uint8_t c[22];
    a.compressed(c);
    EXPECT_EQ(curve_test::hex(c, 22), "0411121314151617180321222324313233344142434" "4");
    a.type_and_clear(slh::Tree);
    EXPECT_EQ(curve_test::hex(a.b + 16, 16), "00000002000000000000000000000000");
}

TEST(SlhDsa_OpenSsl, Sha2) {
    against_openssl<sgcl::crypto::slhdsa_sha2_128s::private_key>("SLH-DSA-SHA2-128s", true);
    against_openssl<sgcl::crypto::slhdsa_sha2_128f::private_key>("SLH-DSA-SHA2-128f", true);
    against_openssl<sgcl::crypto::slhdsa_sha2_192s::private_key>("SLH-DSA-SHA2-192s", false);
    against_openssl<sgcl::crypto::slhdsa_sha2_192f::private_key>("SLH-DSA-SHA2-192f", true);
    against_openssl<sgcl::crypto::slhdsa_sha2_256s::private_key>("SLH-DSA-SHA2-256s", false);
    against_openssl<sgcl::crypto::slhdsa_sha2_256f::private_key>("SLH-DSA-SHA2-256f", true);
}

TEST(SlhDsa_OpenSsl, Shake) {
    against_openssl<sgcl::crypto::slhdsa_shake_128s::private_key>("SLH-DSA-SHAKE-128s", false);
    against_openssl<sgcl::crypto::slhdsa_shake_128f::private_key>("SLH-DSA-SHAKE-128f", true);
    against_openssl<sgcl::crypto::slhdsa_shake_192s::private_key>("SLH-DSA-SHAKE-192s", false);
    against_openssl<sgcl::crypto::slhdsa_shake_192f::private_key>("SLH-DSA-SHAKE-192f", true);
    against_openssl<sgcl::crypto::slhdsa_shake_256s::private_key>("SLH-DSA-SHAKE-256s", false);
    against_openssl<sgcl::crypto::slhdsa_shake_256f::private_key>("SLH-DSA-SHAKE-256f", true);
}

TEST(SlhDsa, Boundaries) {
    using namespace sgcl::crypto;
    using sgcl::slice;
    auto key = slhdsa_sha2_128f::private_key::generate();
    auto pub = key.public_key();
    // the empty message and a megabyte; a context of 255 bytes, 256 refused on both sides
    for (size_t n : {size_t(0), size_t(1) << 20}) {
        sgcl::string m(std::string(n, 'm'));
        auto sig = key.sign(m);
        EXPECT_EQ(sig.size(), slhdsa_sha2_128f::signature_size);
        EXPECT_TRUE(pub.verify(m, sig));
        EXPECT_FALSE(pub.verify(m + sgcl::string("x"), sig));
    }
    sgcl::string c255(std::string(255, 'c')), c256(std::string(256, 'c'));
    auto sc = key.sign("m", {.context = c255});
    EXPECT_TRUE(pub.verify("m", sc, {.context = c255}));
    EXPECT_FALSE(pub.verify("m", sc));
    EXPECT_THROW((void)key.sign("m", {.context = c256}), std::invalid_argument);
    EXPECT_FALSE(pub.verify("m", sc, {.context = c256}));
    // hedged signatures differ, deterministic ones do not
    EXPECT_FALSE(key.sign("m") == key.sign("m"));
    EXPECT_TRUE(key.sign("m", {.deterministic = true}) == key.sign("m", {.deterministic = true}));
    // changed bytes across the signature: R, FORS, every layer of the hypertree
    auto sig = key.sign("m");
    for (size_t i = 0; i < sig.size(); i += 97) {
        auto bad = sig;
        bad[i] = byte(uint8_t(bad[i]) ^ 1);
        EXPECT_FALSE(pub.verify("m", bad)) << i;
    }
    EXPECT_FALSE(pub.verify("m", slice<const byte>(sig.data(), sig.size() - 1)));
    EXPECT_FALSE(pub.verify("m", slice<const byte>()));
    // keys: bytes round trip, a changed PK.root refused, lengths, clone, equality
    auto sk = key.bytes();
    static_assert(decltype(sk)::size == slhdsa_sha2_128f::private_key_size);
    auto again = slhdsa_sha2_128f::private_key::from_bytes(sk).value();
    EXPECT_TRUE(again == key);
    EXPECT_TRUE(again.public_key() == pub);
    bytes_t wrong(reinterpret_cast<const uint8_t*>(sk.bytes().data()), reinterpret_cast<const uint8_t*>(sk.bytes().data()) + sk.size);
    wrong.back() ^= 1;
    EXPECT_FALSE(slhdsa_sha2_128f::private_key::from_bytes(view(wrong)));
    EXPECT_FALSE(slhdsa_sha2_128f::private_key::from_bytes(slice<const byte>(sk.bytes().data(), 63)));
    EXPECT_FALSE(slhdsa_sha2_128f::public_key::from_bytes(slice<const byte>(sk.bytes().data(), 31)));
    EXPECT_TRUE(slhdsa_sha2_128f::public_key::from_bytes(pub.bytes()).value() == pub);
    auto copy = key.clone();
    EXPECT_TRUE(copy == key);
    EXPECT_FALSE(slhdsa_sha2_128f::private_key::generate() == key);
    // another set's signature (the same sizes, SHAKE) does not verify
    auto shake = slhdsa_shake_128f::private_key::generate();
    EXPECT_FALSE(pub.verify("m", shake.sign("m")));
    // a public key is a value: copies verify
    auto pub2 = pub;
    EXPECT_TRUE(pub2.verify("m", sig));
    // a key moved from: std::logic_error; equal only to another one moved from
    auto moved = std::move(copy);
    EXPECT_THROW((void)copy.sign("m"), std::logic_error);
    EXPECT_THROW((void)copy.public_key(), std::logic_error);
    EXPECT_THROW((void)copy.bytes(), std::logic_error);
    EXPECT_THROW((void)copy.clone(), std::logic_error);
    EXPECT_THROW((void)(copy == key), std::logic_error);
    EXPECT_THROW((void)(key == copy), std::logic_error);
    EXPECT_TRUE(moved == key);
}
