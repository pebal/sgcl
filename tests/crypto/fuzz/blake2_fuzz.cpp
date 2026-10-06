//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The five BLAKE2 hashers on any bytes, against OpenSSL: the input's first
// bytes choose the piece sizes of the updates and the lengths of a key, a
// salt and a personalization taken from the input's start; the rest is the
// message. Unkeyed, each must give OpenSSL's BLAKE2B-512 or BLAKE2S-256 (the
// two lengths it has as a digest); keyed, with the salt and the
// personalization, its BLAKE2BMAC or BLAKE2SMAC of the type's size, fed in
// those pieces, one-shot, after a copy and after a reset. A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/blake2_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <openssl/evp.h>
#include <openssl/params.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "blake2_fuzz: %s differs from OpenSSL\n", what);
            std::abort();
        }
    }

    struct pieces {
        size_t size[4];
        size_t at = 0;

        size_t next() {
            return size[at++ % 4];
        }
    };

    void ossl_mac(const char* name, size_t size, const uint8_t* key, size_t kk, const uint8_t* salt, size_t sn,
                  const uint8_t* personal, size_t pn, const uint8_t* p, size_t n, unsigned char* out) {
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, name, nullptr);
        EVP_MAC_CTX* c = EVP_MAC_CTX_new(mac);
        OSSL_PARAM params[4];
        int i = 0;
        params[i++] = OSSL_PARAM_construct_size_t("size", &size);
        if (sn) {
            params[i++] = OSSL_PARAM_construct_octet_string("salt", const_cast<uint8_t*>(salt), sn);
        }
        if (pn) {
            params[i++] = OSSL_PARAM_construct_octet_string("custom", const_cast<uint8_t*>(personal), pn);
        }
        params[i] = OSSL_PARAM_construct_end();
        size_t len = 0;
        bool ok = EVP_MAC_init(c, key, kk, params) == 1 && EVP_MAC_update(c, p, n) == 1 && EVP_MAC_final(c, out, &len, size) == 1;
        EVP_MAC_CTX_free(c);
        EVP_MAC_free(mac);
        check(ok && len == size, "EVP_MAC");
    }

    template<class H>
    void one(const char* mac_name, const char* md_name, const uint8_t* head, const uint8_t* p, size_t n, pieces pz) {
        constexpr size_t field = H::block_size == 128 ? 16 : 8;
        size_t kk = head[0] % (H::max_key_size + 1);
        size_t sn = head[1] % (field + 1);
        size_t pn = head[2] % (field + 1);
        // the key, the salt and the personalization from the message's start
        size_t take = std::min(n, kk + sn + pn);
        kk = std::min(kk, take);
        sn = std::min(sn, take - kk);
        pn = std::min(pn, take - kk - sn);
        const uint8_t* key = p;
        const uint8_t* salt = p + kk;
        const uint8_t* personal = p + kk + sn;
        crypto::blake2_options o{.key = view(key, kk), .salt = view(salt, sn), .personalization = view(personal, pn)};
        H h(o);
        for (size_t i = 0; i < n;) {
            size_t t = std::min(pz.next(), n - i);
            h.update(view(p + i, t));
            i += t;
        }
        unsigned char expected[64];
        if (kk != 0) {
            ossl_mac(mac_name, H::digest_size, key, kk, salt, sn, personal, pn, p, n, expected);
        } else if (md_name && sn == 0 && pn == 0) {
            unsigned int len = 0;
            EVP_Digest(p, n, expected, &len, EVP_get_digestbyname(md_name), nullptr);
        } else {
            return;   // OpenSSL has no unkeyed salt or personalization, nor other unkeyed lengths
        }
        auto v = h.value();
        check(std::memcmp(v.data(), expected, H::digest_size) == 0, mac_name);
        check(h.verify(view(expected, H::digest_size)), "verify");
        auto once = H::of(view(p, n), o);
        check(std::memcmp(once.data(), expected, H::digest_size) == 0, "of");
        H copy = h;
        copy.reset();
        copy.update(view(p, n));
        check(copy.value() == v, "reset");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) {
        return 0;
    }
    pieces pz;
    for (int i = 0; i < 4; ++i) {
        pz.size[i] = 1 + data[i] % 160;
    }
    const uint8_t* head = data + 4;
    const uint8_t* p = data + 8;
    size_t n = size - 8;
    one<crypto::blake2b_512>("BLAKE2BMAC", "BLAKE2B512", head, p, n, pz);
    one<crypto::blake2b_384>("BLAKE2BMAC", nullptr, head, p, n, pz);
    one<crypto::blake2b_256>("BLAKE2BMAC", nullptr, head, p, n, pz);
    one<crypto::blake2s_256>("BLAKE2SMAC", "BLAKE2S256", head, p, n, pz);
    one<crypto::blake2s_128>("BLAKE2SMAC", nullptr, head, p, n, pz);
    return 0;
}
