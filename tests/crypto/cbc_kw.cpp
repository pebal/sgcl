//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// AES-CBC and AES key wrap: SP 800-38A's CBC vectors (F.2), RFC 3394's and
// RFC 5649's examples, Wycheproof's aes_cbc_pkcs5, aes_wrap and aes_kwp
// files (when ~/Programming/oracles/wycheproof is there), OpenSSL's CBC with
// PKCS #7 padding, key wrap and key wrap with padding on random keys and
// lengths, and the edges: every length at its limit, a padding wrong in
// each way, a check register wrong in each way, the chain carried across
// calls and pieces, in-place use, overlaps, keys moved from.
#include "digest_common.h"

#include "sgcl/crypto/cbc.h"
#include "sgcl/crypto/kw.h"
#include "sgcl/encoding/json.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace crypto_test;

namespace {
    std::string read_wycheproof(const char* name) {
        const char* home = std::getenv("HOME");
        if (!home) {
            return {};
        }
        std::ifstream in(std::string(home) + "/Programming/oracles/wycheproof/testvectors_v1/" + name, std::ios::binary);
        if (!in) {
            return {};
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string field(const sgcl::encoding::json& j, const char* name) {
        auto s = j[sgcl::string(name)].as_string();
        return s ? std::string(s->data(), s->size()) : std::string();
    }

    struct WyCase {
        int64_t id;
        bytes_t key, iv, msg, ct;
        std::string result;
    };

    std::vector<WyCase> load(const std::string& text) {
        std::vector<WyCase> cases;
        auto doc = sgcl::encoding::json::parse(sgcl::string(text.data(), text.size()));
        if (!doc) {
            ADD_FAILURE() << "not JSON";
            return cases;
        }
        const auto& groups = (*doc)["testGroups"];
        for (size_t g = 0; g < groups.size(); ++g) {
            const auto& tests = groups[g]["tests"];
            for (size_t t = 0; t < tests.size(); ++t) {
                const auto& c = tests[t];
                cases.push_back({c["tcId"].as_int().value_or(-1), unhex(field(c, "key")), unhex(field(c, "iv")), unhex(field(c, "msg")),
                                 unhex(field(c, "ct")), field(c, "result")});
            }
        }
        return cases;
    }

    bytes_t ossl_cipher(const EVP_CIPHER* c, const bytes_t& key, const bytes_t& iv, const bytes_t& in, bool encrypt, bool pad = true) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        bytes_t out(in.size() + 32);
        int a = 0, b = 0;
        bool ok = EVP_CipherInit_ex(ctx, c, nullptr, key.data(), iv.empty() ? nullptr : iv.data(), encrypt ? 1 : 0) == 1
               && EVP_CIPHER_CTX_set_padding(ctx, pad ? 1 : 0) == 1
               && EVP_CipherUpdate(ctx, out.data(), &a, nonnull(in), int(in.size())) == 1
               && EVP_CipherFinal_ex(ctx, out.data() + a, &b) == 1;
        EVP_CIPHER_CTX_free(ctx);
        if (!ok) {
            throw std::runtime_error("EVP cipher");
        }
        out.resize(size_t(a + b));
        return out;
    }

    const EVP_CIPHER* ossl_cbc(size_t key_size) {
        return key_size == 16 ? EVP_aes_128_cbc() : key_size == 24 ? EVP_aes_192_cbc() : EVP_aes_256_cbc();
    }

    const EVP_CIPHER* ossl_wrap(size_t key_size, bool padded) {
        if (padded) {
            return key_size == 16 ? EVP_aes_128_wrap_pad() : key_size == 24 ? EVP_aes_192_wrap_pad() : EVP_aes_256_wrap_pad();
        }
        return key_size == 16 ? EVP_aes_128_wrap() : key_size == 24 ? EVP_aes_192_wrap() : EVP_aes_256_wrap();
    }

    bytes_t vec(const sgcl::vector<byte>& v) {
        return bytes_t(reinterpret_cast<const unsigned char*>(v.data()), reinterpret_cast<const unsigned char*>(v.data()) + v.size());
    }

    bytes_t vec(const crypto::secret_bytes& v) {
        auto s = v.as_slice();
        return bytes_t(reinterpret_cast<const unsigned char*>(s.data()), reinterpret_cast<const unsigned char*>(s.data()) + s.size());
    }

    sgcl::slice<const byte> view(const sgcl::vector<byte>& v) {
        return sgcl::slice<const byte>(v.data(), v.size());
    }

    using crypto_test::view;
}

TEST(Crypto_AesCbc, Sp800_38aVectors) {
    const bytes_t iv = unhex("000102030405060708090a0b0c0d0e0f");
    const bytes_t plain = unhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                                "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    const std::pair<const char*, const char*> vectors[] = {
        {"2b7e151628aed2a6abf7158809cf4f3c",
         "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7"},
        {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
         "4f021db243bc633d7178183a9fa071e8b4d9ada9ad7dedf4e5e738763f69145a571b242012fb7ae07fa9baac3df102e008b0e27988598881d920a9e64f5615cd"},
        {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
         "f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d39f23369a9d9bacfa530e26304231461b2eb05e2c39be9fcda6c19078c6a9d1b"},
    };
    for (auto& [key_hex, ct_hex] : vectors) {
        bytes_t key = unhex(key_hex), out(64);
        crypto::aes_cbc enc(view(key), view(iv));
        enc.encrypt_blocks(out_view(out), view(plain));
        EXPECT_EQ(hex(out), ct_hex);
        crypto::aes_cbc dec(view(key), view(iv));
        bytes_t back(64);
        dec.decrypt_blocks(out_view(back), view(out));
        EXPECT_EQ(hex(back), hex(plain));
        // in pieces of one, two and one block: the chain carried on
        crypto::aes_cbc pieces(view(key), view(iv));
        bytes_t p = plain;
        pieces.encrypt_blocks(out_view(p).subslice(0, 16), view(p.data(), 16));
        pieces.encrypt_blocks(sgcl::slice<byte>(reinterpret_cast<byte*>(p.data() + 16), 32), view(p.data() + 16, 32));
        pieces.encrypt_blocks(sgcl::slice<byte>(reinterpret_cast<byte*>(p.data() + 48), 16), view(p.data() + 48, 16));
        EXPECT_EQ(hex(p), ct_hex);
    }
}

TEST(Crypto_AesCbc, AgainstOpenSsl) {
    random_source r(3826);
    for (size_t ks : {16, 24, 32}) {
        for (size_t n = 0; n < 700; n += 1 + n / 10) {
            bytes_t key = r.bytes(ks), iv = r.bytes(16), msg = r.bytes(n);
            crypto::aes_cbc enc(view(key), view(iv));
            bytes_t ct = vec(enc.encrypt(view(msg)));
            EXPECT_EQ(hex(ct), hex(ossl_cipher(ossl_cbc(ks), key, iv, msg, true))) << ks << " " << n;
            crypto::aes_cbc dec(view(key), view(iv));
            auto back = dec.decrypt(view(ct));
            ASSERT_TRUE(back.has_value());
            EXPECT_EQ(hex(vec(*back)), hex(msg));
            // whole blocks, eight and more at a time, in place
            bytes_t blocks = r.bytes(n / 16 * 16);
            bytes_t expected = ossl_cipher(ossl_cbc(ks), key, iv, blocks, false, false);
            crypto::aes_cbc raw(view(key), view(iv));
            raw.decrypt_blocks(out_view(blocks), view(blocks));
            EXPECT_EQ(hex(blocks), hex(expected));
        }
    }
}

TEST(Crypto_AesCbc, Wycheproof) {
    std::string text = read_wycheproof("aes_cbc_pkcs5_test.json");
    if (text.empty()) {
        GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/aes_cbc_pkcs5_test.json";
    }
    auto cases = load(text);
    EXPECT_GT(cases.size(), 200u);
    for (auto& c : cases) {
        crypto::aes_cbc dec(view(c.key), view(c.iv));
        auto opened = dec.decrypt(view(c.ct));
        if (c.result == "valid") {
            ASSERT_TRUE(opened.has_value()) << c.id;
            EXPECT_EQ(hex(vec(*opened)), hex(c.msg)) << c.id;
            crypto::aes_cbc enc(view(c.key), view(c.iv));
            EXPECT_EQ(hex(vec(enc.encrypt(view(c.msg)))), hex(c.ct)) << c.id;
        } else {
            EXPECT_FALSE(opened.has_value()) << c.id;
        }
    }
}

TEST(Crypto_AesCbc, PaddingAndLengths) {
    bytes_t key(16, 1), iv(16, 2);
    crypto::aes_cbc enc(view(key), view(iv));
    EXPECT_EQ(enc.encrypt(sgcl::slice<const byte>()).size(), 16u);   // a whole block of padding
    enc.reset(view(iv));
    EXPECT_EQ(enc.encrypt(view(bytes_t(16, 3))).size(), 32u);
    crypto::aes_cbc dec(view(key), view(iv));
    for (size_t n : {0, 15, 17, 31}) {
        dec.reset(view(iv));
        auto r = dec.decrypt(view(bytes_t(n, 0)));
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), crypto::errc::malformed) << n;
    }
    // a last block decrypting to each wrong padding: 0, 17, a byte off inside it
    for (const char* last : {"000102030405060708090a0b0c0d0e00", "000102030405060708090a0b0c0d0e11",
                             "00010203040506070809030303030403", "0001020304050607080910101010100f"}) {
        bytes_t block = unhex(last), ct(16);
        crypto::aes_cbc raw(view(key), view(iv));
        raw.encrypt_blocks(out_view(ct), view(block));
        dec.reset(view(iv));
        auto r = dec.decrypt(view(ct));
        ASSERT_FALSE(r.has_value()) << last;
        EXPECT_EQ(r.error().code(), crypto::errc::authentication) << last;
    }
    // every good padding, 1 to 16
    for (size_t n = 0; n <= 16; ++n) {
        enc.reset(view(iv));
        bytes_t msg(n, 0x55);
        sgcl::vector<byte> ct = enc.encrypt(view(msg));
        dec.reset(view(iv));
        auto r = dec.decrypt(view(ct));
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(r->size(), n);
    }
}

TEST(Crypto_AesCbc, Contracts) {
    bytes_t key(16, 1), iv(16, 2), k15(15, 1), iv15(15, 2);
    EXPECT_THROW(crypto::aes_cbc(view(k15), view(iv)), std::invalid_argument);
    EXPECT_THROW(crypto::aes_cbc(view(key), view(iv15)), std::invalid_argument);
    auto bad = crypto::aes_cbc::from_key(view(k15), view(iv));
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().code(), crypto::errc::invalid_key);
    ASSERT_TRUE(crypto::aes_cbc::from_key(view(key), view(iv)).has_value());
    crypto::aes_cbc c(view(key), view(iv));
    bytes_t buf(32), small(16), odd(20);
    EXPECT_THROW(c.encrypt_blocks(out_view(odd), view(odd)), std::invalid_argument);
    EXPECT_THROW(c.encrypt_blocks(out_view(small), view(buf)), std::length_error);
    EXPECT_THROW(c.decrypt_blocks(sgcl::slice<byte>(reinterpret_cast<byte*>(buf.data() + 1), 16), view(buf.data(), 16)),
                 std::invalid_argument);
    EXPECT_THROW(c.reset(view(iv15)), std::invalid_argument);
    c.encrypt_blocks(sgcl::slice<byte>(), sgcl::slice<const byte>());   // nothing: nothing
    // clone at the same point of the chain
    crypto::aes_cbc a(view(key), view(iv));
    a.encrypt_blocks(out_view(buf), view(buf));
    crypto::aes_cbc b = a.clone();
    bytes_t x(16, 7), y(16, 7);
    a.encrypt_blocks(out_view(x), view(x));
    b.encrypt_blocks(out_view(y), view(y));
    EXPECT_EQ(hex(x), hex(y));
    EXPECT_EQ(b.key_size(), 16u);
    // moved from: every use a logic_error; moved onto itself: the same key
    crypto::aes_cbc moved = std::move(a);
    EXPECT_THROW(a.encrypt(view(x)), std::logic_error);
    EXPECT_THROW(a.clone(), std::logic_error);
    EXPECT_EQ(a.key_size(), 0u);
    auto& self = moved;
    moved = std::move(self);
    EXPECT_EQ(moved.key_size(), 16u);
}

TEST(Crypto_AesKw, Rfc3394And5649) {
    crypto::aes_kw kek(view(unhex("000102030405060708090A0B0C0D0E0F")));
    EXPECT_EQ(hex(vec(kek.wrap(view(unhex("00112233445566778899AABBCCDDEEFF"))))), "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5");
    auto un = kek.unwrap(view(unhex("1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5")));
    ASSERT_TRUE(un.has_value());
    EXPECT_EQ(hex(vec(*un)), "00112233445566778899aabbccddeeff");
    // §4.6: 256 bits of key data under a 256-bit KEK
    crypto::aes_kw kek256(view(unhex("000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F")));
    EXPECT_EQ(hex(vec(kek256.wrap(view(unhex("00112233445566778899AABBCCDDEEFF000102030405060708090A0B0C0D0E0F"))))),
              "28c9f404c4b810f4cbccb35cfb87f8263f5786e2d80ed326cbc7f0e71a99f43bfb988b9b7a02dd21");
    // RFC 5649 §6
    crypto::aes_kw kwp(view(unhex("5840df6e29b02af1ab493b705bf16ea1ae8338f4dcc176a8")));
    EXPECT_EQ(hex(vec(kwp.wrap_padded(view(unhex("c37b7e6492584340bed12207808941155068f738"))))),
              "138bdeaa9b8fa7fc61f97742e72248ee5ae6ae5360d1ae6a5f54f373fa543b6a");
    EXPECT_EQ(hex(vec(kwp.wrap_padded(view(unhex("466f7250617369"))))), "afbeb0f07dfbf5419200f2ccb50bb24f");
    auto p = kwp.unwrap_padded(view(unhex("afbeb0f07dfbf5419200f2ccb50bb24f")));
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(hex(vec(*p)), "466f7250617369");
}

TEST(Crypto_AesKw, AgainstOpenSsl) {
    random_source r(5649);
    for (size_t ks : {16, 24, 32}) {
        bytes_t key = r.bytes(ks);
        crypto::aes_kw kek(view(key));
        for (size_t n = 1; n < 200; n += 1 + n / 8) {
            bytes_t k = r.bytes(n);
            if (n >= 16 && n % 8 == 0) {
                bytes_t w = vec(kek.wrap(view(k)));
                EXPECT_EQ(hex(w), hex(ossl_cipher(ossl_wrap(ks, false), key, {}, k, true))) << n;
                auto u = kek.unwrap(view(w));
                ASSERT_TRUE(u.has_value());
                EXPECT_EQ(hex(vec(*u)), hex(k));
            }
            bytes_t wp = vec(kek.wrap_padded(view(k)));
            EXPECT_EQ(hex(wp), hex(ossl_cipher(ossl_wrap(ks, true), key, {}, k, true))) << n;
            auto up = kek.unwrap_padded(view(wp));
            ASSERT_TRUE(up.has_value());
            EXPECT_EQ(hex(vec(*up)), hex(k));
        }
    }
}

TEST(Crypto_AesKw, Wycheproof) {
    for (auto [file, padded] : {std::pair{"aes_wrap_test.json", false}, std::pair{"aes_kwp_test.json", true}}) {
        std::string text = read_wycheproof(file);
        if (text.empty()) {
            GTEST_SKIP() << "no ~/Programming/oracles/wycheproof/testvectors_v1/" << file;
        }
        auto cases = load(text);
        EXPECT_GT(cases.size(), 100u);
        size_t checked = 0;
        for (auto& c : cases) {
            auto kek = crypto::aes_kw::from_key(view(c.key));
            if (!kek) {
                EXPECT_NE(c.result, "valid") << file << " " << c.id;
                continue;
            }
            auto u = padded ? kek->unwrap_padded(view(c.ct)) : kek->unwrap(view(c.ct));
            if (c.result == "valid") {
                ASSERT_TRUE(u.has_value()) << file << " " << c.id;
                EXPECT_EQ(hex(vec(*u)), hex(c.msg)) << file << " " << c.id;
                EXPECT_EQ(hex(vec(padded ? kek->wrap_padded(view(c.msg)) : kek->wrap(view(c.msg)))), hex(c.ct)) << file << " " << c.id;
            } else if (c.result == "invalid") {
                EXPECT_FALSE(u.has_value()) << file << " " << c.id;
            }
            ++checked;
        }
        EXPECT_GT(checked, 100u);
    }
}

TEST(Crypto_AesKw, Edges) {
    bytes_t key(16, 9);
    crypto::aes_kw kek(view(key));
    EXPECT_THROW(kek.wrap(view(bytes_t(8, 1))), std::invalid_argument);
    EXPECT_THROW(kek.wrap(view(bytes_t(20, 1))), std::invalid_argument);
    EXPECT_THROW(kek.wrap_padded(sgcl::slice<const byte>()), std::invalid_argument);
    for (size_t n : {0, 8, 16, 25}) {
        auto u = kek.unwrap(view(bytes_t(n, 0)));
        ASSERT_FALSE(u.has_value());
        EXPECT_EQ(u.error().code(), n == 16 ? crypto::errc::malformed : crypto::errc::malformed) << n;
    }
    for (size_t n : {0, 8, 15, 17}) {
        auto u = kek.unwrap_padded(view(bytes_t(n, 0)));
        ASSERT_FALSE(u.has_value());
        EXPECT_EQ(u.error().code(), crypto::errc::malformed) << n;
    }
    // a changed byte anywhere: authentication
    bytes_t w = vec(kek.wrap(view(bytes_t(24, 5))));
    for (size_t i = 0; i < w.size(); ++i) {
        bytes_t t = w;
        t[i] ^= 0x40;
        auto u = kek.unwrap(view(t));
        ASSERT_FALSE(u.has_value());
        EXPECT_EQ(u.error().code(), crypto::errc::authentication);
    }
    // KWP registers wrong in each way, wrapped by hand with W: the constant,
    // a length past the data, a length a whole half short, a padding byte set
    auto craft = [&](uint64_t a, bytes_t r) {
        crypto::detail::AesEncryptKey k;
        crypto::detail::aes_setup(k, key.data(), 16);
        crypto::detail::kw_wrap(k, a, r.data(), r.size() / 8);
        bytes_t out(8);
        crypto::detail::store_be64(out.data(), a);
        out.insert(out.end(), r.begin(), r.end());
        return out;
    };
    const uint64_t aiv = uint64_t(0xA65959A6) << 32;
    EXPECT_TRUE(kek.unwrap_padded(view(craft(aiv | 20, unhex("0102030405060708090a0b0c0d0e0f101112131400000000")))).has_value());
    for (auto [a, r] : {std::pair{(uint64_t(0xA65959A7) << 32) | 20, std::string("0102030405060708090a0b0c0d0e0f101112131400000000")},
                        std::pair{aiv | 25, std::string("0102030405060708090a0b0c0d0e0f101112131400000000")},
                        std::pair{aiv | 16, std::string("0102030405060708090a0b0c0d0e0f101112131400000000")},
                        std::pair{aiv | 0, std::string("0102030405060708090a0b0c0d0e0f101112131400000000")},
                        std::pair{aiv | 20, std::string("0102030405060708090a0b0c0d0e0f101112131400000100")}}) {
        auto u = kek.unwrap_padded(view(craft(a, unhex(r))));
        ASSERT_FALSE(u.has_value()) << std::hex << a << " " << r;
        EXPECT_EQ(u.error().code(), crypto::errc::authentication);
    }
    // moved from
    crypto::aes_kw moved = std::move(kek);
    EXPECT_THROW(kek.wrap(view(bytes_t(16, 1))), std::logic_error);
    EXPECT_THROW((void)kek.unwrap(view(bytes_t(24, 1))), std::logic_error);
    EXPECT_EQ(moved.clone().key_size(), 16u);
}
