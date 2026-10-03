//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The private keys' PEM (from_pem, to_pem; detail/key_pem.h) and their
// exports as secret_bytes. For every type of key: OpenSSL's PEM (PKCS #8,
// and the traditional SEC 1 and PKCS #1) read; to_pem byte for byte the
// PKCS #8 PEM OpenSSL writes of the same key; a round trip; text around the
// block and another block before it passed over; encrypted keys and bad
// base64 refused. And what they leave: nothing in managed memory (no page
// taken over thousands of calls), every block they free zeroed.
#include "digest_common.h"
#include "tests/managed_pages.h"

#include <openssl/bio.h>
#include <openssl/pem.h>

#include <cstring>
#include <string>

using namespace crypto_test;

namespace {
    struct Key {
        EVP_PKEY* p = nullptr;
        ~Key() {
            EVP_PKEY_free(p);
        }
    };

    // OpenSSL's PEM of the key: PKCS #8, or its traditional form (SEC 1's
    // "EC PRIVATE KEY", PKCS #1's "RSA PRIVATE KEY")
    std::string ossl_pem(EVP_PKEY* k, bool traditional) {
        BIO* b = BIO_new(BIO_s_mem());
        if (traditional) {
            PEM_write_bio_PrivateKey_traditional(b, k, nullptr, nullptr, 0, nullptr, nullptr);
        } else {
            PEM_write_bio_PKCS8PrivateKey(b, k, nullptr, nullptr, 0, nullptr, nullptr);
        }
        char* p = nullptr;
        long n = BIO_get_mem_data(b, &p);
        std::string s(p, size_t(n));
        BIO_free(b);
        return s;
    }

    slice<const std::byte> text_of(const std::string& s) {
        return slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    std::string text_of(const crypto::secret_bytes& s) {
        auto v = s.as_slice();
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // What the allocator's probe saw
    size_t blocks_freed = 0;
    bool freed_zero = true;

    void record(const void* block, size_t n) noexcept {
        ++blocks_freed;
        const auto* p = static_cast<const unsigned char*>(block);
        for (size_t i = 0; i < n; ++i) {
            if (p[i]) {
                freed_zero = false;
            }
        }
    }

    // A key of each type through OpenSSL's PEM and back through ours
    template<class K>
    void round_trip(const char* name, EVP_PKEY* k, bool has_traditional) {
        const std::string pkcs8 = ossl_pem(k, false);
        auto ours = K::from_pem(text_of(pkcs8));
        ASSERT_TRUE(ours) << name << ": " << ours.error().message();
        EXPECT_EQ(text_of(ours->to_pem()), pkcs8) << name << ": to_pem is not OpenSSL's PKCS #8 PEM";
        auto again = K::from_pem(ours->to_pem());
        ASSERT_TRUE(again) << name;
        EXPECT_TRUE(text_of(again->to_pkcs8_der()) == text_of(ours->to_pkcs8_der())) << name;
        if (has_traditional) {
            const std::string traditional = ossl_pem(k, true);
            auto t = K::from_pem(text_of(traditional));
            ASSERT_TRUE(t) << name << " traditional: " << t.error().message();
            EXPECT_TRUE(text_of(t->to_pkcs8_der()) == text_of(ours->to_pkcs8_der())) << name;
        }
        // text around the block, and a block of another label before it
        const std::string around = "a key, for the tests\n-----BEGIN EC PARAMETERS-----\nBggqhkjOPQMBBw==\n-----END EC PARAMETERS-----\n" +
                                   pkcs8 + "and after it\n";
        EXPECT_TRUE(K::from_pem(text_of(around))) << name;
        // what it leaves: no managed memory, every freed block zeroed
        blocks_freed = 0;
        freed_zero = true;
        crypto::detail::WipingPolicy::probe = record;
        const size_t pages = heap_count::pages_of(2000, [&] {
            auto pem = ours->to_pem();
            auto k2 = K::from_pem(pem);
            (void)k2;
        });
        crypto::detail::WipingPolicy::probe = nullptr;
        EXPECT_EQ(pages, 0u) << name << ": managed pages taken by to_pem and from_pem";
        EXPECT_GT(blocks_freed, 0u) << name;
        EXPECT_TRUE(freed_zero) << name << ": a block freed with bytes left in it";
    }
}

TEST(Crypto_KeyPem, Ed25519) {
    Key k{EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519")};
    round_trip<crypto::ed25519::private_key>("Ed25519", k.p, false);
}

TEST(Crypto_KeyPem, X25519) {
    Key k{EVP_PKEY_Q_keygen(nullptr, nullptr, "X25519")};
    round_trip<crypto::x25519::private_key>("X25519", k.p, false);
}

TEST(Crypto_KeyPem, EcKeys) {
    Key p256{EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256")};
    round_trip<crypto::p256::private_key>("P-256", p256.p, true);
    round_trip<crypto::p256::ecdh_key>("P-256 ECDH", p256.p, false);
    Key p384{EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-384")};
    round_trip<crypto::p384::private_key>("P-384", p384.p, true);
    round_trip<crypto::p384::ecdh_key>("P-384 ECDH", p384.p, false);
}

TEST(Crypto_KeyPem, Rsa) {
    Key k{EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(2048))};
    round_trip<crypto::rsa::private_key>("RSA 2048", k.p, true);
}

TEST(Crypto_KeyPem, WhatIsRefused) {
    Key k{EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519")};
    const std::string pem = ossl_pem(k.p, false);
    auto code = [](const auto& r) { return r ? crypto::errc{} : r.error().code(); };
    EXPECT_EQ(code(crypto::ed25519::private_key::from_pem(text_of(std::string("no key here")))), crypto::errc::malformed);
    // a key of another type
    EXPECT_EQ(code(crypto::p256::private_key::from_pem(text_of(pem))), crypto::errc::unsupported);
    // encrypted, as PKCS #8 and as RFC 1421's headers
    BIO* b = BIO_new(BIO_s_mem());
    PEM_write_bio_PKCS8PrivateKey(b, k.p, EVP_aes_128_cbc(), nullptr, 0, nullptr, const_cast<char*>("password"));
    char* p = nullptr;
    long n = BIO_get_mem_data(b, &p);
    const std::string encrypted(p, size_t(n));
    BIO_free(b);
    EXPECT_EQ(code(crypto::ed25519::private_key::from_pem(text_of(encrypted))), crypto::errc::unsupported);
    const std::string headers = "-----BEGIN RSA PRIVATE KEY-----\nProc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00\n\nAAAA\n-----END RSA PRIVATE KEY-----\n";
    EXPECT_EQ(code(crypto::rsa::private_key::from_pem(text_of(headers))), crypto::errc::unsupported);
    // base64 that is not
    std::string bad = pem;
    bad[40] = '*';
    EXPECT_EQ(code(crypto::ed25519::private_key::from_pem(text_of(bad))), crypto::errc::malformed);
    // no END line
    EXPECT_EQ(code(crypto::ed25519::private_key::from_pem(text_of(pem.substr(0, pem.size() - 20)))), crypto::errc::malformed);
}

// The PEM errors of RSA and the NIST curves start as their other errors
// do, with the type's namespace (they once began "PEM:" alone)
TEST(Crypto_KeyPem, MessagesNameTheKeyType) {
    auto message = [](const auto& r) { return r ? std::string() : std::string(r.error().message().view()); };
    const std::string none = "no key here";
    EXPECT_EQ(message(crypto::rsa::private_key::from_pem(text_of(none))), "sgcl::crypto::rsa: PEM: no private key block");
    EXPECT_EQ(message(crypto::p256::private_key::from_pem(text_of(none))), "sgcl::crypto::p256: PEM: no private key block");
    EXPECT_EQ(message(crypto::p384::ecdh_key::from_pem(text_of(none))), "sgcl::crypto::p384: PEM: no private key block");
    Key ec{EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256")};
    const std::string sec1 = ossl_pem(ec.p, true);
    EXPECT_EQ(message(crypto::rsa::private_key::from_pem(text_of(sec1))), "sgcl::crypto::rsa: PEM: a block of another key's type");
    EXPECT_EQ(message(crypto::p256::ecdh_key::from_pem(text_of(sec1))), "sgcl::crypto::p256: PEM: a block of another key's type");
    const std::string headers = "-----BEGIN RSA PRIVATE KEY-----\nProc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,00\n\nAAAA\n-----END RSA PRIVATE KEY-----\n";
    EXPECT_EQ(message(crypto::rsa::private_key::from_pem(text_of(headers))),
              "sgcl::crypto::rsa: PEM: a key encrypted with RFC 1421 headers (Proc-Type, DEK-Info)");
    // X25519 and Ed25519 name no type in any of their errors
    EXPECT_EQ(message(crypto::ed25519::private_key::from_pem(text_of(none))), "PEM: no private key block");
}

TEST(Crypto_KeyPem, ExportsAreSecretBytes) {
    // to_pkcs8_der and to_sec1_der give secret_bytes: 48 bytes of a 25519 key
    // in the object itself, a larger key's in a wiped block
    auto ed = crypto::ed25519::private_key::generate();
    crypto::secret_bytes der = ed.to_pkcs8_der();
    EXPECT_EQ(der.size(), 48u);
    auto ec = crypto::p256::private_key::generate();
    crypto::secret_bytes sec1 = ec.to_sec1_der();
    crypto::secret_bytes pkcs8 = ec.to_pkcs8_der();
    EXPECT_TRUE(crypto::p256::private_key::from_sec1_der(sec1));
    EXPECT_TRUE(crypto::p256::private_key::from_pkcs8_der(pkcs8));
}

// The edges of the PEM text (DESIGN 408): no text at all, a BEGIN at the
// very end, a block of no label or of no body, another label's block
// before the key, line breaks of any kind and none, the key's block cut
// at its last character (covered: no key, another key's type, encrypted,
// bad base64, no END line: Crypto_KeyPem.WhatIsRefused)
TEST(Crypto_KeyPem, TheEdgesOfTheText) {
    auto code = [](const auto& r) { return r ? crypto::errc{} : r.error().code(); };
    using K = crypto::ed25519::private_key;
    EXPECT_EQ(code(K::from_pem(slice<const std::byte>())), crypto::errc::malformed);
    EXPECT_EQ(code(K::from_pem(text_of(std::string("-----BEGIN ")))), crypto::errc::malformed);
    EXPECT_EQ(code(K::from_pem(text_of(std::string("-----BEGIN PRIVATE KEY-----")))), crypto::errc::malformed);
    EXPECT_EQ(code(K::from_pem(text_of(std::string("-----BEGIN PRIVATE KEY----------END PRIVATE KEY-----")))), crypto::errc::malformed);
    EXPECT_EQ(code(K::from_pem(text_of(std::string("-----BEGIN PRIVATE KEY-----\n\n-----END PRIVATE KEY-----\n")))), crypto::errc::malformed);
    Key k{EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519")};
    const std::string pem = ossl_pem(k.p, false);
    auto want = K::from_pem(text_of(pem));
    ASSERT_TRUE(want);
    // after a block of no label and one of another label
    std::string before = "-----BEGIN -----\nAAAA\n-----END -----\n-----BEGIN EC PARAMETERS-----\nBggqhkjOPQMBBw==\n-----END EC PARAMETERS-----\n";
    auto after = K::from_pem(text_of(before + pem));
    ASSERT_TRUE(after);
    EXPECT_TRUE(*after == *want);
    // CRLF line breaks, and the base64 on one line with none at the end
    std::string crlf;
    for (char c : pem) {
        if (c == '\n') {
            crlf += '\r';
        }
        crlf += c;
    }
    auto windows = K::from_pem(text_of(crlf));
    ASSERT_TRUE(windows);
    EXPECT_TRUE(*windows == *want);
    std::string flat = pem;
    while (!flat.empty() && flat.back() == '\n') {
        flat.pop_back();
    }
    auto unterminated = K::from_pem(text_of(flat));
    ASSERT_TRUE(unterminated);
    EXPECT_TRUE(*unterminated == *want);
    EXPECT_EQ(code(K::from_pem(text_of(flat.substr(0, flat.size() - 1)))), crypto::errc::malformed);
}
