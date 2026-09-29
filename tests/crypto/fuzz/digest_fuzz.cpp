//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The digests, SHAKE, HMAC, HKDF and PBKDF2 on any bytes, against OpenSSL:
// the input's first bytes choose the piece sizes of the updates, the
// length of an HMAC key and of the output asked; the rest is the message.
// Every digest, fed in those pieces, must give OpenSSL's value; so must an
// HMAC, a SHAKE read in pieces, an HKDF and a PBKDF2 of one or two
// iterations. On arm64 the instructions' compression is held against the
// portable one on the same blocks too. A difference aborts.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/digest_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o digest_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./digest_fuzz <seconds> <seed files...>
#include "sgcl/crypto/crypto.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "digest_fuzz: %s differs from OpenSSL\n", what);
            std::abort();
        }
    }

    template<class A, class B>
    bool same(const A& a, const B& b, size_t n) {
        return std::memcmp(a.data(), b, n) == 0;
    }

    // SHAKE's output is a secret_bytes: its bytes through as_slice
    template<class B>
    bool same(const crypto::secret_bytes& a, const B& b, size_t n) {
        return std::memcmp(a.as_slice().data(), b, n) == 0;
    }

    // the piece sizes: the input's first four bytes, each 1..64
    struct pieces {
        size_t size[4];
        size_t at = 0;

        size_t next() {
            return size[at++ % 4];
        }
    };

    template<class H>
    void digest(const char* name, const uint8_t* p, size_t n, pieces pz) {
        H h;
        for (size_t i = 0; i < n;) {
            size_t take = std::min(pz.next(), n - i);
            h.update(view(p + i, take));
            i += take;
        }
        unsigned char expected[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        EVP_Digest(p, n, expected, &len, EVP_get_digestbyname(name), nullptr);
        check(len == H::digest_size && same(h.value(), expected, len), name);
        check(same(H::of(view(p, n)), expected, len), name);
    }

    template<class H>
    void mac(const char* name, const uint8_t* key, size_t key_size, const uint8_t* p, size_t n) {
        unsigned char expected[EVP_MAX_MD_SIZE];
        unsigned int len = 0;
        static const unsigned char none = 0;
        HMAC(EVP_get_digestbyname(name), key_size ? key : &none, int(key_size), p, n, expected, &len);
        crypto::hmac<H> m(view(key, key_size));
        m.update(view(p, n));
        check(same(m.value(), expected, len), "hmac");
        check(m.verify(view(expected, len)), "hmac verify");
        expected[n % len] ^= 1;
        check(!m.verify(view(expected, len)), "hmac verify of a wrong tag");
    }

    template<class X>
    void xof(const char* name, const uint8_t* p, size_t n, size_t out, pieces pz) {
        std::vector<unsigned char> expected(out);
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        EVP_DigestInit_ex(c, EVP_get_digestbyname(name), nullptr);
        EVP_DigestUpdate(c, p, n);
        EVP_DigestFinalXOF(c, expected.data(), out);
        EVP_MD_CTX_free(c);
        X x;
        x.update(view(p, n));
        std::vector<unsigned char> got(out);
        for (size_t i = 0; i < out;) {
            size_t take = std::min(pz.next() * 5, out - i);
            x.read_to(slice<byte>(reinterpret_cast<byte*>(got.data() + i), take));
            i += take;
        }
        check(got == expected, name);
    }

    void kdfs(const uint8_t* p, size_t n, size_t key_size, size_t out) {
        // HKDF-SHA256: salt the first key_size bytes, ikm the rest, info a piece of it
        size_t salt_size = std::min(key_size, n);
        const uint8_t* ikm = p + salt_size;
        size_t ikm_size = n - salt_size;
        size_t info_size = std::min<size_t>(ikm_size, 40);
        size_t length = 1 + out % crypto::hkdf_sha256::max_size;
        std::vector<unsigned char> expected(length);
        static unsigned char none = 0;
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr);
        EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
        OSSL_PARAM params[] = {
            OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>("SHA256"), 0),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, ikm_size ? const_cast<uint8_t*>(ikm) : &none, ikm_size),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt_size ? const_cast<uint8_t*>(p) : &none, salt_size),
            OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, info_size ? const_cast<uint8_t*>(ikm) : &none, info_size),
            OSSL_PARAM_construct_end(),
        };
        EVP_KDF_derive(c, expected.data(), length, params);
        EVP_KDF_CTX_free(c);
        EVP_KDF_free(kdf);
        auto got = crypto::hkdf_sha256::derive(view(p, salt_size), view(ikm, ikm_size), view(ikm, info_size), length);
        check(same(got, expected.data(), length), "hkdf");

        // PBKDF2-HMAC-SHA512, one or two iterations
        uint32_t iterations = 1 + uint32_t(out & 1);
        size_t dk = 1 + out % 150;
        std::vector<unsigned char> expected_dk(dk);
        PKCS5_PBKDF2_HMAC(reinterpret_cast<const char*>(ikm_size ? ikm : &none), int(ikm_size), salt_size ? p : &none,
                          int(salt_size), int(iterations), EVP_sha512(), int(dk), expected_dk.data());
        auto got_dk = crypto::pbkdf2<crypto::sha512>::derive(view(ikm, ikm_size), view(p, salt_size), iterations, dk);
        check(same(got_dk, expected_dk.data(), dk), "pbkdf2");
    }

#if defined(SGCL_CRYPTO_ARM64)
    void paths(const uint8_t* p, size_t n) {
        namespace d = crypto::detail;
        size_t blocks = n / 168;
        namespace cpu = sgcl::detail::cpu;
        if (blocks == 0 || !cpu::crypto() || !cpu::sha512() || !cpu::sha3()) {
            return;
        }
        uint32_t a1[5] = {1, 2, 3, 4, 5}, b1[5] = {1, 2, 3, 4, 5};
        d::sha1_compress_portable(a1, p, blocks);
        d::sha1_compress_arm64(b1, p, blocks);
        check(std::memcmp(a1, b1, sizeof a1) == 0, "sha1 paths");
        uint32_t a2[8], b2[8];
        std::memcpy(a2, d::sha256_iv, sizeof a2);
        std::memcpy(b2, d::sha256_iv, sizeof b2);
        d::sha256_compress_portable(a2, p, blocks);
        d::sha256_compress_arm64(b2, p, blocks);
        check(std::memcmp(a2, b2, sizeof a2) == 0, "sha256 paths");
        uint64_t a5[8], b5[8];
        std::memcpy(a5, d::sha512_iv, sizeof a5);
        std::memcpy(b5, d::sha512_iv, sizeof b5);
        d::sha512_compress_portable(a5, p, blocks * 168 / 128);
        d::sha512_compress_arm64(b5, p, blocks * 168 / 128);
        check(std::memcmp(a5, b5, sizeof a5) == 0, "sha512 paths");
        uint64_t ak[25] = {}, bk[25] = {};
        d::keccak_absorb_portable(ak, p, blocks, 168);
        d::keccak_absorb_arm64<168>(bk, p, blocks);
        check(std::memcmp(ak, bk, sizeof ak) == 0, "keccak paths");
    }
#endif
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return 0;
    }
    pieces pz;
    for (int i = 0; i < 4; ++i) {
        pz.size[i] = 1 + data[i] % 64;
    }
    size_t key_size = data[4] % 200;
    size_t out = size_t(data[5]) << 8 | data[6];
    const uint8_t* p = data + 8;
    size_t n = size - 8;

    digest<crypto::sha1>("SHA1", p, n, pz);
    digest<crypto::sha224>("SHA224", p, n, pz);
    digest<crypto::sha256>("SHA256", p, n, pz);
    digest<crypto::sha384>("SHA384", p, n, pz);
    digest<crypto::sha512>("SHA512", p, n, pz);
    digest<crypto::sha512_256>("SHA512-256", p, n, pz);
    digest<crypto::sha3_224>("SHA3-224", p, n, pz);
    digest<crypto::sha3_256>("SHA3-256", p, n, pz);
    digest<crypto::sha3_384>("SHA3-384", p, n, pz);
    digest<crypto::sha3_512>("SHA3-512", p, n, pz);

    size_t k = std::min(key_size, n);
    mac<crypto::sha256>("SHA256", p, k, p + k, n - k);
    mac<crypto::sha512>("SHA512", p, k, p + k, n - k);
    mac<crypto::sha3_256>("SHA3-256", p, k, p + k, n - k);

    xof<crypto::shake128>("SHAKE128", p, n, out % 1000, pz);
    xof<crypto::shake256>("SHAKE256", p, n, out % 1000, pz);

    kdfs(p, n, key_size, out);
#if defined(SGCL_CRYPTO_ARM64)
    paths(p, n);
#endif
    return 0;
}
