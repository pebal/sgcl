//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// BLAKE2b and BLAKE2s (RFC 7693): the RFC's examples and its self-test of
// Appendix E (every digest length it names, keyed and not, through the
// state the five types share), OpenSSL's BLAKE2B-512, BLAKE2S-256 and its
// BLAKE2BMAC/BLAKE2SMAC (key, salt, personalization, size) on random data
// fed in pieces, Python's hashlib for the unkeyed salt and personalization,
// and the edges of every member: the longest key, salt and
// personalization and one byte past them, nothing hashed, the block's
// edges, reset keeping the key, a copy branching, verify, the destructor
// zeroing a keyed state, HMAC and HKDF over BLAKE2s.
#include "digest_common.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <new>
#include <stdexcept>

using namespace crypto_test;

namespace {
    bytes_t ossl_digest_named(const char* name, const bytes_t& msg) {
        static EVP_MD* b = ossl_md("BLAKE2B-512");
        static EVP_MD* s = ossl_md("BLAKE2S-256");
        unsigned char out[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        if (EVP_Digest(msg.data(), msg.size(), out, &len, name[6] == 'B' ? b : s, nullptr) != 1) {
            throw std::runtime_error("EVP_Digest");
        }
        return bytes_t(out, out + len);
    }

    template<class H> const char* blake2_mac_name();
    template<> const char* blake2_mac_name<crypto::blake2b_512>() { return "BLAKE2BMAC"; }
    template<> const char* blake2_mac_name<crypto::blake2b_384>() { return "BLAKE2BMAC"; }
    template<> const char* blake2_mac_name<crypto::blake2b_256>() { return "BLAKE2BMAC"; }
    template<> const char* blake2_mac_name<crypto::blake2s_256>() { return "BLAKE2SMAC"; }
    template<> const char* blake2_mac_name<crypto::blake2s_128>() { return "BLAKE2SMAC"; }

    // OpenSSL's keyed BLAKE2 (it has no unkeyed salt or personalization):
    // the key 1.. bytes, the salt, the personalization ("custom") and the size
    template<class H>
    bytes_t ossl_blake2_mac(const bytes_t& key, const bytes_t& salt, const bytes_t& personal, const bytes_t& msg) {
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, blake2_mac_name<H>(), nullptr);
        EVP_MAC_CTX* c = EVP_MAC_CTX_new(mac);
        size_t size = H::digest_size;
        OSSL_PARAM params[4];
        int n = 0;
        params[n++] = OSSL_PARAM_construct_size_t("size", &size);
        if (!salt.empty()) {
            params[n++] = OSSL_PARAM_construct_octet_string("salt", const_cast<unsigned char*>(salt.data()), salt.size());
        }
        if (!personal.empty()) {
            params[n++] = OSSL_PARAM_construct_octet_string("custom", const_cast<unsigned char*>(personal.data()), personal.size());
        }
        params[n] = OSSL_PARAM_construct_end();
        bytes_t out(H::digest_size);
        size_t len = 0;
        bool ok = EVP_MAC_init(c, key.data(), key.size(), params) == 1 && EVP_MAC_update(c, msg.data(), msg.size()) == 1
               && EVP_MAC_final(c, out.data(), &len, out.size()) == 1;
        EVP_MAC_CTX_free(c);
        EVP_MAC_free(mac);
        if (!ok || len != H::digest_size) {
            throw std::runtime_error("EVP_MAC BLAKE2");
        }
        return out;
    }

    template<class H>
    bytes_t got(const H& h) {
        auto v = h.value();
        return bytes_t(reinterpret_cast<const unsigned char*>(v.data()), reinterpret_cast<const unsigned char*>(v.data()) + v.size());
    }

    // RFC 7693 Appendix E: the sequence its self-test hashes
    bytes_t selftest_seq(size_t n, uint32_t seed) {
        bytes_t out(n);
        uint32_t a = 0xDEAD4BADu * seed, b = 1;
        for (size_t i = 0; i < n; ++i) {
            uint32_t t = a + b;
            a = b;
            b = t;
            out[i] = (unsigned char)(t >> 24);
        }
        return out;
    }

    // A digest of any length through the state the public types share
    template<class Tr>
    bytes_t blake2_any(size_t size, const bytes_t& key, const bytes_t& msg) {
        crypto::detail::Blake2State<Tr> s;
        s.init(size, key.data(), key.size(), nullptr, 0, nullptr, 0);
        s.update(msg.data(), msg.size());
        bytes_t out(size);
        s.finish(out.data());
        return out;
    }

    template<class Tr>
    std::string selftest(std::initializer_list<size_t> md_len, std::initializer_list<size_t> in_len) {
        crypto::detail::Blake2State<Tr> grand;
        grand.init(32, nullptr, 0, nullptr, 0, nullptr, 0);
        for (size_t outlen : md_len) {
            for (size_t inlen : in_len) {
                bytes_t data = selftest_seq(inlen, uint32_t(inlen));
                bytes_t md = blake2_any<Tr>(outlen, {}, data);
                grand.update(md.data(), md.size());
                bytes_t key = selftest_seq(outlen, uint32_t(outlen));
                md = blake2_any<Tr>(outlen, key, data);
                grand.update(md.data(), md.size());
            }
        }
        unsigned char out[32];
        grand.finish(out);
        return hex(out, 32);
    }

    template<class H>
    void against_openssl(uint64_t seed) {
        random_source r(seed);
        for (size_t n : {0, 1, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1000, 4096, 4097}) {
            bytes_t msg = r.bytes(n);
            // keyed, with a salt and a personalization of every length up to the field
            size_t kk = 1 + r.below(H::max_key_size);
            size_t field = H::block_size == 128 ? 16 : 8;
            bytes_t key = r.bytes(kk), salt = r.bytes(r.below(field + 1)), personal = r.bytes(r.below(field + 1));
            bytes_t expected = ossl_blake2_mac<H>(key, salt, personal, msg);
            H h({.key = view(key), .salt = view(salt), .personalization = view(personal)});
            for (size_t i = 0; i < n;) {
                size_t take = std::min(n - i, 1 + r.below(300));
                h.update(view(msg.data() + i, take));
                i += take;
            }
            EXPECT_EQ(hex(got(h)), hex(expected)) << n;
            EXPECT_TRUE(h.verify(view(expected)));
            auto once = H::of(view(msg), {.key = view(key), .salt = view(salt), .personalization = view(personal)});
            EXPECT_EQ(hex(once), hex(expected));
        }
    }
}

TEST(Crypto_Blake2, Rfc7693Examples) {
    // Appendix A and B: "abc"
    EXPECT_EQ(hex(crypto::blake2b_512::of("abc")),
              "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
              "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923");
    EXPECT_EQ(hex(crypto::blake2s_256::of("abc")), "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982");
    // nothing hashed
    EXPECT_EQ(hex(crypto::blake2b_512::of("")),
              "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419"
              "d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce");
    EXPECT_EQ(hex(crypto::blake2s_256::of("")), "69217a3079908094e11121d042354a7c1f55b6482ca1a51e1b250dfd1ed0eef9");
}

TEST(Crypto_Blake2, Rfc7693SelfTest) {
    // Appendix E: every digest length of the test, unkeyed and keyed, over
    // inputs across the block's edges, hashed into one digest
    EXPECT_EQ(selftest<crypto::detail::Blake2bTraits>({20, 32, 48, 64}, {0, 3, 128, 129, 255, 1024}),
              "c23a7800d98123bd10f506c61e29da5603d763b8bbad2e737f5e765a7bccd475");
    EXPECT_EQ(selftest<crypto::detail::Blake2sTraits>({16, 20, 28, 32}, {0, 3, 64, 65, 255, 1024}),
              "6a411f08ce25adcdfb02aba641451cec53c598b24f4fc787fbdc88797f4c1dfe");
}

TEST(Crypto_Blake2, AgainstOpenSslInPieces) {
    random_source r(7693);
    for (size_t n = 0; n <= 1100; n += 1 + n / 50) {
        bytes_t msg = r.bytes(n);
        crypto::blake2b_512 b;
        crypto::blake2s_256 s;
        for (size_t i = 0; i < n;) {
            size_t take = std::min(n - i, 1 + r.below(200));
            b.update(view(msg.data() + i, take));
            s.update(view(msg.data() + i, take));
            i += take;
        }
        EXPECT_EQ(hex(got(b)), hex(ossl_digest_named("BLAKE2B-512", msg))) << n;
        EXPECT_EQ(hex(got(s)), hex(ossl_digest_named("BLAKE2S-256", msg))) << n;
        EXPECT_EQ(hex(crypto::blake2b_512::of(view(msg))), hex(got(b)));
    }
}

TEST(Crypto_Blake2, KeyedAgainstOpenSsl) {
    against_openssl<crypto::blake2b_512>(1);
    against_openssl<crypto::blake2b_384>(2);
    against_openssl<crypto::blake2b_256>(3);
    against_openssl<crypto::blake2s_256>(4);
    against_openssl<crypto::blake2s_128>(5);
}

TEST(Crypto_Blake2, SaltAndPersonalizationAgainstPython) {
    // hashlib.blake2b(b"abc", salt=..., person=...), unkeyed
    EXPECT_EQ(hex(crypto::blake2b_512::of("abc", {.salt = "saltsaltsaltsalt", .personalization = "personal12345678"})),
              "68b96a6be7b05d012dc76690589e9e2f1b9f650e8d4faa5a93878513a7ad0c01"
              "ef1b2ca4004c0f0c0d35c9d03edeedef65f4b27ab406e0ffe2f0ca030c06b3c3");
    // shorter than the field: zero-padded, as hashlib pads
    EXPECT_EQ(hex(crypto::blake2b_256::of("abc", {.salt = "salt", .personalization = "me"})),
              "67eabcea5b99bc6678091443749327bd342afb727764c729745c40d498896f97");
    EXPECT_EQ(hex(crypto::blake2s_256::of("abc", {.salt = "saltsalt", .personalization = "personal"})),
              "c53092d1e407e687ca47e8a6662ec0b00fa2c1179476bac8f0f147740ef20278");
    bytes_t k(32, 'k');
    EXPECT_EQ(hex(crypto::blake2s_128::of("abc", {.key = view(k), .salt = "s", .personalization = "p"})),
              "6f3636b1428ec2fef4fdaf6c7bf24729");
    EXPECT_EQ(hex(crypto::blake2b_384::of("")),
              "b32811423377f52d7862286ee1a72ee540524380fda1724a6f25d7978c6fd3244a6caf0498812673c5e05ef583825100");
    bytes_t msg;
    for (int i = 0; i < 1024; ++i) {
        msg.push_back((unsigned char)i);
    }
    bytes_t key64;
    for (int i = 0; i < 64; ++i) {
        key64.push_back((unsigned char)i);
    }
    EXPECT_EQ(hex(crypto::blake2b_384::of(view(msg), {.key = view(key64)})),
              "77a7cde74e3dc5cf07b5ebaa197c0e2614065f44e7deff5768b4109e394d952288adb938e955b955df4842b6ad7330dd");
}

TEST(Crypto_Blake2, LimitsOfTheOptions) {
    bytes_t b64(64, 1), b65(65, 1), b16(16, 2), b17(17, 2), b32(32, 3), b33(33, 3), b8(8, 4), b9(9, 4);
    EXPECT_NO_THROW(crypto::blake2b_512({.key = view(b64), .salt = view(b16), .personalization = view(b16)}));
    EXPECT_THROW(crypto::blake2b_512({.key = view(b65)}), std::invalid_argument);
    EXPECT_THROW(crypto::blake2b_256({.salt = view(b17)}), std::invalid_argument);
    EXPECT_THROW(crypto::blake2b_384({.personalization = view(b17)}), std::invalid_argument);
    EXPECT_NO_THROW(crypto::blake2s_256({.key = view(b32), .salt = view(b8), .personalization = view(b8)}));
    EXPECT_THROW(crypto::blake2s_256({.key = view(b33)}), std::invalid_argument);
    EXPECT_THROW(crypto::blake2s_128({.salt = view(b9)}), std::invalid_argument);
    EXPECT_THROW(crypto::blake2s_128({.personalization = view(b9)}), std::invalid_argument);
    EXPECT_THROW(crypto::blake2s_128::of("x", {.key = view(b33)}), std::invalid_argument);
    // empty options: the plain hash
    EXPECT_EQ(hex(crypto::blake2b_512::of("abc", {})), hex(crypto::blake2b_512::of("abc")));
    crypto::blake2s_256 empty_key({.key = sgcl::slice<const byte>()});
    empty_key.update("abc");
    EXPECT_EQ(hex(got(empty_key)), hex(crypto::blake2s_256::of("abc")));
}

TEST(Crypto_Blake2, ResetValueCopyAndVerify) {
    bytes_t key(32, 9);
    crypto::blake2b_256 h({.key = view(key)});
    auto empty = h.value();   // a keyed hash of nothing: the key's block alone, final
    EXPECT_EQ(hex(empty), hex(ossl_blake2_mac<crypto::blake2b_256>(key, {}, {}, {})));
    h.update("hello ");
    crypto::blake2b_256 branch = h;   // a copy branches
    h.update("world");
    branch.update("there");
    EXPECT_EQ(hex(got(h)), hex(ossl_blake2_mac<crypto::blake2b_256>(key, {}, {}, text("hello world"))));
    EXPECT_EQ(hex(got(branch)), hex(ossl_blake2_mac<crypto::blake2b_256>(key, {}, {}, text("hello there"))));
    EXPECT_EQ(hex(h.value()), hex(h.digest()));   // value goes on: twice the same
    h.reset();   // the key kept
    EXPECT_EQ(hex(got(h)), hex(empty));
    h.update("hello world");
    auto tag = h.value();
    EXPECT_TRUE(h.verify(tag));
    bytes_t wrong(tag.size());
    std::memcpy(wrong.data(), tag.data(), tag.size());
    wrong[31] ^= 1;
    EXPECT_FALSE(h.verify(view(wrong)));
    EXPECT_FALSE(h.verify(view(wrong.data(), 31)));   // another length
    EXPECT_FALSE(h.verify(sgcl::slice<const byte>()));
    // assignment over a keyed one
    crypto::blake2b_256 plain;
    plain = h;
    EXPECT_TRUE(plain.verify(tag));
    plain = crypto::blake2b_256();
    plain.update("hello world");
    EXPECT_EQ(hex(got(plain)), hex(crypto::blake2b_256::of("hello world")));
    // copied onto itself
    auto& self = plain;
    plain = self;
    EXPECT_EQ(hex(got(plain)), hex(crypto::blake2b_256::of("hello world")));
}

TEST(Crypto_Blake2, KeyedStateZeroedByTheDestructor) {
    alignas(crypto::blake2s_256) unsigned char storage[sizeof(crypto::blake2s_256)];
    bytes_t key(32, 0xa5);
    auto* h = new (storage) crypto::blake2s_256({.key = view(key)});
    h->update("some message");
    h->~blake2s_256();
    bool zero = true;
    for (unsigned char c : storage) {
        zero = zero && c == 0;
    }
    EXPECT_TRUE(zero);
}

TEST(Crypto_Blake2, HmacAndHkdfOverBlake2s) {
    // Noise's and WireGuard's HMAC-BLAKE2s
    random_source r(11);
    for (size_t kn : {0, 1, 32, 64, 65, 200}) {
        bytes_t key = r.bytes(kn), msg = r.bytes(100 + kn);
        unsigned char out[64];
        unsigned int len = 0;
        static const unsigned char none = 0;
        HMAC(EVP_MD_fetch(nullptr, "BLAKE2S-256", nullptr), key.empty() ? &none : key.data(), int(key.size()), msg.data(),
             msg.size(), out, &len);
        EXPECT_EQ(hex(crypto::hmac<crypto::blake2s_256>::of(view(msg), view(key))), hex(out, len));
        HMAC(EVP_MD_fetch(nullptr, "BLAKE2B-512", nullptr), key.empty() ? &none : key.data(), int(key.size()), msg.data(),
             msg.size(), out, &len);
        EXPECT_EQ(hex(crypto::hmac<crypto::blake2b_512>::of(view(msg), view(key))), hex(out, len));
    }
    bytes_t salt = r.bytes(32), ikm = r.bytes(32), info = r.bytes(10);
    auto okm = crypto::hkdf<crypto::blake2s_256>::derive(view(salt), view(ikm), view(info), 64);
    // HKDF by hand: PRK = HMAC(salt, ikm); T1 = HMAC(PRK, info | 1); T2 = HMAC(PRK, T1 | info | 2)
    auto prk = crypto::hmac<crypto::blake2s_256>::of(view(ikm), view(salt));
    crypto::hmac<crypto::blake2s_256> t1(prk);
    t1.update(view(info));
    t1.update("\x01");
    auto a = t1.value();
    crypto::hmac<crypto::blake2s_256> t2(prk);
    t2.update(a);
    t2.update(view(info));
    t2.update("\x02");
    auto b = t2.value();
    EXPECT_EQ(hex(okm), hex(a) + hex(b));
}

TEST(Crypto_Blake2, TextAndFileForms) {
    // the mixin's forms: text, a C string, a file
    const char* c = "abc";
    crypto::blake2b_512 h;
    h.update(c);
    EXPECT_EQ(hex(got(h)), hex(crypto::blake2b_512::of("abc")));
    std::string path = testing::TempDir() + "blake2_file.bin";
    bytes_t data(100000, 0x5a);
    FILE* f = std::fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    auto d = crypto::blake2s_256::of_file(sgcl::string(path.c_str()));
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(hex(*d), hex(ossl_digest_named("BLAKE2S-256", data)));
    std::remove(path.c_str());
    EXPECT_FALSE(crypto::blake2s_256::of_file(sgcl::string(path.c_str())).has_value());
}
