//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ciphers against the vectors of their specifications: FIPS 197
// (Appendix C), SP 800-38A (F.5, CTR), the GCM paper of McGrew and Viega
// (the test cases SP 800-38D refers to), RFC 8439 (ChaCha20, Poly1305 with
// the edge cases of A.3, the AEAD) and draft-irtf-cfrg-xchacha-03. Every
// vector runs on the path this build compiles: the tests are built twice,
// tests_crypto_portable with SGCL_CRYPTO_PORTABLE (tests/CMakeLists.txt).
#include "cipher_test.h"

#include <random>

using namespace cipher_test;

namespace {
    const std::string sunscreen = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";

    template<size_t N>
    sgcl::array<std::byte, N> fixed(const bytes& v) {
        sgcl::array<std::byte, N> a;
        std::memcpy(a.data(), v.data(), N);
        return a;
    }
}

TEST(CryptoVectors_Tests, PathIsReported) {
    std::printf("[ path     ] %s\n", path_name());
}

// FIPS 197 Appendix C: the same plaintext under the three key sizes
TEST(CryptoVectors_Tests, AesFips197AppendixC) {
    struct V {
        const char* key;
        const char* ct;
    };
    const V vs[] = {
        {"000102030405060708090a0b0c0d0e0f", "69c4e0d86a7b0430d8cdb78070b4c55a"},
        {"000102030405060708090a0b0c0d0e0f1011121314151617", "dda97ca4864cdfe06eaf70a0ec0d7191"},
        {"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "8ea2b7ca516745bfeafc49904b496089"},
    };
    auto pt = fixed<16>(hex("00112233445566778899aabbccddeeff"));
    for (auto& v : vs) {
        crypto::aes a(hex(v.key));
        auto ct = a.encrypt_block(pt);
        EXPECT_EQ(to_hex(ct), v.ct);
        EXPECT_EQ(to_hex(a.decrypt_block(ct)), "00112233445566778899aabbccddeeff");
    }
}

// FIPS 197 Appendix B, the worked example of the key 2b7e1516...
TEST(CryptoVectors_Tests, AesFips197AppendixB) {
    crypto::aes a(hex("2b7e151628aed2a6abf7158809cf4f3c"));
    auto ct = a.encrypt_block(fixed<16>(hex("3243f6a8885a308d313198a2e0370734")));
    EXPECT_EQ(to_hex(ct), "3925841d02dc09fbdc118597196a0b32");
}

// The S-box of the key schedule (computed, never looked up) against
// FIPS 197 Figure 7 at a few places, and its being a permutation
TEST(CryptoVectors_Tests, SboxIsTheStandardOne) {
    EXPECT_EQ(crypto::detail::sbox_byte(0x00), 0x63);
    EXPECT_EQ(crypto::detail::sbox_byte(0x01), 0x7c);
    EXPECT_EQ(crypto::detail::sbox_byte(0x53), 0xed);
    EXPECT_EQ(crypto::detail::sbox_byte(0xff), 0x16);
    EXPECT_EQ(crypto::detail::sbox_byte(0x9a), 0xb8);
    bool seen[256] = {};
    for (int x = 0; x < 256; ++x) {
        seen[crypto::detail::sbox_byte(uint8_t(x))] = true;
    }
    for (bool s : seen) {
        EXPECT_TRUE(s);
    }
}

// SP 800-38A F.5.1 and F.5.5: CTR-AES128 and CTR-AES256, the counter's
// low bytes rolling over from ...feff
TEST(CryptoVectors_Tests, AesCtrSp80038a) {
    const std::string pt = "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710";
    struct V {
        const char* key;
        const char* ct;
    };
    const V vs[] = {
        {"2b7e151628aed2a6abf7158809cf4f3c", "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee"},
        {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "1abc932417521ca24f2b0459fe7e6e0b090339ec0aa6faefd5ccc2c6f4ce8e941e36b26bd1ebc670d1bd1d665620abf74f78a7f6d29809585a97daec58c6b050"},
        {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c52b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6"},
    };
    for (auto& v : vs) {
        crypto::aes_ctr c(hex(v.key), hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
        bytes in = hex(pt), out(in.size());
        c.xor_key_stream(out, in);
        EXPECT_EQ(to_hex(out), v.ct);
        // and back, from block 0 again
        c.seek(0);
        c.xor_key_stream(out, out);
        EXPECT_EQ(to_hex(out), pt);
    }
}

// SP 800-38A F.2.2, F.2.4, F.2.6: CBC decryption (the eight blocks at a
// time of aes_cbc_decrypt, 7z's 7zAES) under the three key sizes, whole and
// in pieces; and on 1000 random blocks, every count 0..20 and the rest,
// against the blocks decrypted one at a time and chained by hand
TEST(CryptoVectors_Tests, AesCbcDecryptionSp80038a) {
    const std::string pt = "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710";
    struct V {
        const char* key;
        const char* ct;
    };
    const V vs[] = {
        {"2b7e151628aed2a6abf7158809cf4f3c", "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7"},
        {"8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b", "4f021db243bc633d7178183a9fa071e8b4d9ada9ad7dedf4e5e738763f69145a571b242012fb7ae07fa9baac3df102e008b0e27988598881d920a9e64f5615cd"},
        {"603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", "f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d39f23369a9d9bacfa530e26304231461b2eb05e2c39be9fcda6c19078c6a9d1b"},
    };
    auto u = [](bytes& b) {
        return reinterpret_cast<unsigned char*>(b.data());
    };
    for (auto& v : vs) {
        bytes key = hex(v.key);
        crypto::detail::AesEncryptKey k;
        crypto::detail::AesDecryptKey d;
        crypto::detail::aes_setup(k, u(key), key.size());
        crypto::detail::aes_setup_decrypt(d, k);
        for (size_t cut : {size_t(4), size_t(1), size_t(3)}) {
            bytes iv = hex("000102030405060708090a0b0c0d0e0f"), in = hex(v.ct), out(in.size());
            crypto::detail::aes_cbc_decrypt(k, d, u(iv), u(in), u(out), cut);
            crypto::detail::aes_cbc_decrypt(k, d, u(iv), u(in) + 16 * cut, u(out) + 16 * cut, 4 - cut);
            EXPECT_EQ(to_hex(out), pt) << v.key << " " << cut;
            EXPECT_EQ(to_hex(iv), std::string(v.ct).substr(96)) << v.key;   // the last ciphertext
        }
        // in place
        bytes iv = hex("000102030405060708090a0b0c0d0e0f"), in = hex(v.ct);
        crypto::detail::aes_cbc_decrypt(k, d, u(iv), u(in), u(in), 4);
        EXPECT_EQ(to_hex(in), pt) << v.key;
        // eight at a time against one at a time
        std::mt19937 rng(unsigned(key.size()));
        bytes ct(16 * 1000);
        for (auto& b : ct) {
            b = std::byte(rng());
        }
        bytes start(16);
        for (auto& b : start) {
            b = std::byte(rng());
        }
        crypto::aes a(key);
        bytes expected(ct.size());
        for (size_t i = 0; i < 1000; ++i) {
            sgcl::array<std::byte, 16> block;
            std::memcpy(block.data(), ct.data() + 16 * i, 16);
            auto p = a.decrypt_block(block);
            const std::byte* prev = i ? ct.data() + 16 * (i - 1) : start.data();
            for (int j = 0; j < 16; ++j) {
                expected[16 * i + j] = p[j] ^ prev[j];
            }
        }
        for (size_t first = 0; first <= 20; ++first) {
            bytes iv2 = start, out(ct.size());
            crypto::detail::aes_cbc_decrypt(k, d, u(iv2), u(ct), u(out), first);
            crypto::detail::aes_cbc_decrypt(k, d, u(iv2), u(ct) + 16 * first, u(out) + 16 * first, 1000 - first);
            ASSERT_TRUE(out == expected) << v.key << " first " << first;
        }
    }
}

// McGrew and Viega, "The Galois/Counter Mode of Operation", test cases 1-4
// (AES-128), 7-10 (AES-192) and 13-16 (AES-256)
TEST(CryptoVectors_Tests, AesGcmMcGrewViega) {
    struct V {
        const char* key;
        const char* pt;
        const char* aad;
        const char* ct;
        const char* tag;
    };
    const char* p64 = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255";
    const char* p60 = "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
    const char* aad = "feedfacedeadbeeffeedfacedeadbeefabaddad2";
    const char* k128 = "feffe9928665731c6d6a8f9467308308";
    const char* k192 = "feffe9928665731c6d6a8f9467308308feffe9928665731c";
    const char* k256 = "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
    const V vs[] = {
        // 1, 2
        {"00000000000000000000000000000000", "", "", "", "58e2fccefa7e3061367f1d57a4e7455a"},
        {"00000000000000000000000000000000", "00000000000000000000000000000000", "", "0388dace60b6a392f328c2b971b2fe78", "ab6e47d42cec13bdf53a67b21257bddf"},
        // 3, 4
        {k128, p64, "", "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985", "4d5c2af327cd64a62cf35abd2ba6fab4"},
        {k128, p60, aad, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091", "5bc94fbc3221a5db94fae95ae7121a47"},
        // 7, 8
        {"000000000000000000000000000000000000000000000000", "", "", "", "cd33b28ac773f74ba00ed1f312572435"},
        {"000000000000000000000000000000000000000000000000", "00000000000000000000000000000000", "", "98e7247c07f0fe411c267e4384b0f600", "2ff58d80033927ab8ef4d4587514f0fb"},
        // 9, 10
        {k192, p64, "", "3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710acade256", "9924a7c8587336bfb118024db8674a14"},
        {k192, p60, aad, "3980ca0b3c00e841eb06fac4872a2757859e1ceaa6efd984628593b40ca1e19c7d773d00c144c525ac619d18c84a3f4718e2448b2fe324d9ccda2710", "2519498e80f1478f37ba55bd6d27618c"},
        // 13, 14
        {"0000000000000000000000000000000000000000000000000000000000000000", "", "", "", "530f8afbc74536b9a963b4f1c4cb738b"},
        {"0000000000000000000000000000000000000000000000000000000000000000", "00000000000000000000000000000000", "", "cea7403d4d606b6e074ec5d3baf39d18", "d0d1c8a799996bf0265b98b5d48ab919"},
        // 15, 16
        {k256, p64, "", "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad", "b094dac5d93471bdec1a502270e3cc6c"},
        {k256, p60, aad, "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662", "76fc6ece0f4e1768cddf8853bb2d551b"},
    };
    int index = 0;
    for (auto& v : vs) {
        SCOPED_TRACE(index++);
        const bool zero_iv = std::string(v.pt).size() <= 32 && std::string(v.key).find_first_not_of('0') == std::string::npos;
        bytes nonce = hex(zero_iv ? "000000000000000000000000" : "cafebabefacedbaddecaf888");
        crypto::aes_gcm g(hex(v.key));
        auto sealed = g.seal(nonce, hex(v.pt), hex(v.aad));
        EXPECT_EQ(to_hex(sealed), std::string(v.ct) + v.tag);
        auto opened = g.open(nonce, sealed, hex(v.aad));
        ASSERT_TRUE(opened.has_value());
        EXPECT_EQ(to_hex(*opened), v.pt);
    }
}

// RFC 8439 §2.3.2: the block function, key 00..1f, counter 1
TEST(CryptoVectors_Tests, ChachaBlockRfc8439) {
    auto key = hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    crypto::chacha20 c(key, hex("000000090000004a00000000"));
    c.seek(1);
    bytes zeros(64), out(64);
    c.xor_key_stream(out, zeros);
    EXPECT_EQ(to_hex(out), "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4ed2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e");
}

// RFC 8439 §2.4.2: the sunscreen text from counter 1
TEST(CryptoVectors_Tests, ChachaEncryptionRfc8439) {
    auto key = hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    crypto::chacha20 c(key, hex("000000000000004a00000000"));
    c.seek(1);
    bytes in = text(sunscreen), out(in.size());
    c.xor_key_stream(out, in);
    EXPECT_EQ(to_hex(out), "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b8eedf2785e42874d");
}

// RFC 8439 §2.5.2 and the edge cases of A.3 (#5-#11: the accumulator at
// and around p, the final reduction, the carries)
TEST(CryptoVectors_Tests, Poly1305Rfc8439) {
    struct V {
        const char* key;
        std::string msg;
        const char* tag;
    };
    const std::string forum = to_hex(text("Cryptographic Forum Research Group"));
    const V vs[] = {
        {"85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b", forum, "a8061dc1305136c6c22b8baf0c0127a9"},
        {"0200000000000000000000000000000000000000000000000000000000000000", "ffffffffffffffffffffffffffffffff", "03000000000000000000000000000000"},
        {"02000000000000000000000000000000ffffffffffffffffffffffffffffffff", "02000000000000000000000000000000", "03000000000000000000000000000000"},
        {"0100000000000000000000000000000000000000000000000000000000000000", "fffffffffffffffffffffffffffffffff0ffffffffffffffffffffffffffffff11000000000000000000000000000000", "05000000000000000000000000000000"},
        {"0100000000000000000000000000000000000000000000000000000000000000", "fffffffffffffffffffffffffffffffffbfefefefefefefefefefefefefefefe01010101010101010101010101010101", "00000000000000000000000000000000"},
        {"0200000000000000000000000000000000000000000000000000000000000000", "fdffffffffffffffffffffffffffffff", "faffffffffffffffffffffffffffffff"},
        {"0100000000000000040000000000000000000000000000000000000000000000", "e33594d7505e43b900000000000000003394d7505e4379cd01000000000000000000000000000000000000000000000001000000000000000000000000000000", "14000000000000005500000000000000"},
        {"0100000000000000040000000000000000000000000000000000000000000000", "e33594d7505e43b900000000000000003394d7505e4379cd010000000000000000000000000000000000000000000000", "13000000000000000000000000000000"},
    };
    for (auto& v : vs) {
        SCOPED_TRACE(v.tag);
        bytes key = hex(v.key), msg = hex(v.msg), tag(16);
        crypto::detail::Poly1305 p;
        p.init(u8(key));
        p.update(u8(msg), msg.size());
        p.finish(u8(tag));
        EXPECT_EQ(to_hex(tag), v.tag);
#if SGCL_TEST_OPENSSL
        // and against OpenSSL
        EXPECT_EQ(to_hex(ossl_poly1305(key, msg)), v.tag);
#endif
    }
}

// RFC 8439 §2.6.2: the one-time key is the first half of block 0
TEST(CryptoVectors_Tests, Poly1305KeyGenerationRfc8439) {
    auto key = hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    crypto::chacha20 c(key, hex("000000000001020304050607"));
    bytes zeros(32), out(32);
    c.xor_key_stream(out, zeros);
    EXPECT_EQ(to_hex(out), "8ad5a08b905f81cc815040274ab29471a833b637e3fd0da508dbb8e2fdd1a646");
}

// RFC 8439 §2.8.2: the AEAD
TEST(CryptoVectors_Tests, ChachaPolyAeadRfc8439) {
    auto key = hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    auto nonce = hex("070000004041424344454647");
    auto aad = hex("50515253c0c1c2c3c4c5c6c7");
    crypto::chacha20_poly1305 a(key);
    auto sealed = a.seal(nonce, text(sunscreen), aad);
    EXPECT_EQ(to_hex(sealed),
              "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116"
              "1ae10b594f09e26a7e902ecbd0600691");
    auto opened = a.open(nonce, sealed, aad);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(to_hex(*opened), to_hex(text(sunscreen)));
}

// RFC 8439 A.5: the AEAD decryption example (an Internet-Draft's text)
TEST(CryptoVectors_Tests, ChachaPolyAeadDecryptionRfc8439A5) {
    auto key = hex("1c9240a5eb55d38af333888604f6b5f0473917c1402b80099dca5cbc207075c0");
    auto nonce = hex("000000000102030405060708");
    auto aad = hex("f33388860000000000004e91");
    auto ct = hex(
        "64a0861575861af460f062c79be643bd5e805cfd345cf389f108670ac76c8cb24c6cfc18755d43eea09ee94e382d26b0bdb7b73c321b0100d4f03b7f355894cf332f830e710b97ce98c8a84abd0b948114ad176e008d33bd60f982b1ff37c8559797a06ef4f0ef61c186324e2b3506383606907b6a7c02b0f9f6157b53c867e4b9166c767b804d46a59b5216cde7a4e99040c5a40433225ee282a1b0a06c523eaf4534d7f83fa1155b0047718cbc546a0d072b04b3564eea1b422273f548271a0bb2316053fa76991955ebd63159434ecebb4e466dae5a1073a6727627097a1049e617d91d361094fa68f0ff77987130305beaba2eda04df997b714d6c6f2c29a6ad5cb4022b02709b"
        "eead9d67890cbb22392336fea1851f38");
    crypto::chacha20_poly1305 a(key);
    auto opened = a.open(nonce, ct, aad);
    ASSERT_TRUE(opened.has_value());
    std::string s(reinterpret_cast<const char*>(opened->as_slice().data()), opened->size());
    EXPECT_EQ(s.substr(0, 31), "Internet-Drafts are draft docum");
    EXPECT_EQ(opened->size(), 265u);
}

// draft-irtf-cfrg-xchacha-03 §2.2.1: HChaCha20
TEST(CryptoVectors_Tests, HChachaDraft) {
    auto key = hex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    auto nonce = hex("000000090000004a0000000031415927");
    uint32_t sub[8];
    crypto::detail::hchacha20(u8(key), u8(nonce), sub);
    bytes out(32);
    for (int i = 0; i < 8; ++i) {
        crypto::detail::store_le32(u8(out) + 4 * i, sub[i]);
    }
    EXPECT_EQ(to_hex(out), "82413b4227b27bfed30e42508a877d73a0f9e4d58a74a853c12ec41326d3ecdc");
}

// draft-irtf-cfrg-xchacha-03 A.3.1: XChaCha20-Poly1305
TEST(CryptoVectors_Tests, XChachaPolyDraft) {
    auto key = hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    auto nonce = hex("404142434445464748494a4b4c4d4e4f5051525354555657");
    auto aad = hex("50515253c0c1c2c3c4c5c6c7");
    crypto::xchacha20_poly1305 a(key);
    auto sealed = a.seal(nonce, text(sunscreen), aad);
    EXPECT_EQ(to_hex(sealed),
              "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b4522f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff921f9664c97637da9768812f615c68b13b52e"
              "c0875924c1c7987947deafd8780acf49");
    auto opened = a.open(nonce, sealed, aad);
    ASSERT_TRUE(opened.has_value());
    EXPECT_EQ(to_hex(*opened), to_hex(text(sunscreen)));
}

// XChaCha20 through chacha20 with a 24-byte nonce: the keystream of
// HChaCha20's key with the nonce's last 8 bytes (draft-irtf-cfrg-xchacha-03
// §2.3), the same in pieces of 7 bytes
TEST(CryptoVectors_Tests, XChachaStreamIsHChachaThenChacha) {
    auto key = hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
    auto nonce = hex("404142434445464748494a4b4c4d4e4f5051525354555658");
    crypto::chacha20 c(key, nonce);
    bytes in(64), out(64);
    c.xor_key_stream(out, in);
    crypto::chacha20 d(key, nonce);
    bytes out2(64);
    for (size_t i = 0; i < 64; i += 7) {
        size_t n = std::min<size_t>(7, 64 - i);
        d.xor_key_stream(slice<std::byte>(out2.data() + i, n), slice<const std::byte>(in.data() + i, n));
    }
    EXPECT_EQ(to_hex(out), to_hex(out2));
    // and that it is the keystream of HChaCha20's key with the nonce's tail
    uint32_t sub[8];
    crypto::detail::hchacha20(u8(key), u8(nonce), sub);
    bytes subkey(32);
    for (int i = 0; i < 8; ++i) {
        crypto::detail::store_le32(u8(subkey) + 4 * i, sub[i]);
    }
    bytes n12 = hex("00000000");
    n12.insert(n12.end(), nonce.begin() + 16, nonce.end());
    crypto::chacha20 e(subkey, n12);
    bytes out3(64);
    e.xor_key_stream(out3, in);
    EXPECT_EQ(to_hex(out), to_hex(out3));
}
