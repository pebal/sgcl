//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Argon2 on any bytes. The PHC reader: the input as a string, which must be
// read or refused without a fault, and when read, written back by the
// module's own format to the same string (one form per value: no leading
// zeros, base64 without padding), and verified when its costs are small.
// The function: the variant, the costs (up to 64 KiB, two passes, four
// lanes), the tag's length (through both forms of H') and the password,
// the salt, the secret and the associated data cut from the input, against
// OpenSSL's ARGON2D/I/ID. A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/argon2_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <algorithm>
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
            std::fprintf(stderr, "argon2_fuzz: %s\n", what);
            std::abort();
        }
    }

    void phc(const uint8_t* data, size_t size) {
        slice<const char> text(reinterpret_cast<const char*>(data), size);
        crypto::detail::Argon2Phc parsed;
        auto r = crypto::detail::argon2_phc_parse(text, parsed);
        if (!r) {
            check(r.error().code() == crypto::errc::malformed || r.error().code() == crypto::errc::unsupported, "an error of another code");
            return;
        }
        string again = crypto::detail::argon2_phc_format(parsed.variant, parsed.memory, parsed.iterations, parsed.parallelism,
                                                         parsed.salt, parsed.salt_size, parsed.hash, parsed.hash_size);
        check(again.size() == size && std::memcmp(again.data(), data, size) == 0, "a PHC string read is not written back as it was");
        if (parsed.memory <= 64 && parsed.iterations <= 2) {
            auto v = crypto::argon2::verify("password", string(text.data(), text.size()));
            check(v || v.error().code() == crypto::errc::authentication, "verify of a string read");
        }
    }

    void function(const uint8_t* data, size_t size) {
        if (size < 8) {
            return;
        }
        uint32_t variant = data[0] % 3;
        uint32_t p = 1 + data[1] % 4;
        uint32_t m = 8 * p + data[2] % 40;
        uint32_t t = 1 + data[3] % 2;
        size_t n = 4 + (size_t(data[4]) | size_t(data[5] & 1) << 8);
        size_t cut1 = data[6] % 32, cut2 = data[7] % 16;
        const uint8_t* rest = data + 8;
        size_t left = size - 8;
        size_t pw = std::min(left, cut1);
        size_t salt = std::min(left - pw, size_t(8) + cut2);
        if (salt < 8) {
            return;
        }
        size_t secret = std::min(left - pw - salt, size_t(data[6] >> 5) * 3);
        size_t ad = left - pw - salt - secret;
        const uint8_t* ppw = rest;
        const uint8_t* psalt = rest + pw;
        const uint8_t* psecret = psalt + salt;
        const uint8_t* pad = psecret + secret;
        static const char* names[3] = {"ARGON2D", "ARGON2I", "ARGON2ID"};
        static const uint8_t none = 0;
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, names[variant], nullptr);
        EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
        uint32_t threads = 1, version = 0x13;
        OSSL_PARAM params[10];
        int i = 0;
        params[i++] = OSSL_PARAM_construct_octet_string("pass", const_cast<uint8_t*>(pw ? ppw : &none), pw);
        params[i++] = OSSL_PARAM_construct_octet_string("salt", const_cast<uint8_t*>(psalt), salt);
        if (secret) {
            params[i++] = OSSL_PARAM_construct_octet_string("secret", const_cast<uint8_t*>(psecret), secret);
        }
        if (ad) {
            params[i++] = OSSL_PARAM_construct_octet_string("ad", const_cast<uint8_t*>(pad), ad);
        }
        params[i++] = OSSL_PARAM_construct_uint32("iter", &t);
        params[i++] = OSSL_PARAM_construct_uint32("threads", &threads);
        params[i++] = OSSL_PARAM_construct_uint32("lanes", &p);
        params[i++] = OSSL_PARAM_construct_uint32("memcost", &m);
        params[i++] = OSSL_PARAM_construct_uint32("version", &version);
        params[i] = OSSL_PARAM_construct_end();
        std::vector<uint8_t> expected(n);
        check(EVP_KDF_derive(c, expected.data(), n, params) == 1, "OpenSSL refused");
        EVP_KDF_CTX_free(c);
        EVP_KDF_free(kdf);
        crypto::argon2::options o{.variant = crypto::argon2::variant(variant), .memory = m, .iterations = t, .parallelism = p,
                                 .secret = view(psecret, secret), .associated_data = view(pad, ad)};
        auto got = crypto::argon2::derive(view(ppw, pw), view(psalt, salt), n, o);
        check(got.size() == n && std::memcmp(got.as_slice().data(), expected.data(), n) == 0, "differs from OpenSSL");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    if (data[0] == '$') {
        phc(data, size);
    } else {
        function(data, size);
    }
    return 0;
}
