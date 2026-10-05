// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the PRF of the client's TLS 1.2 (sgcl/net/tls/detail/
// prf.h): OpenSSL's TLS1-PRF (EVP_KDF) asked on random inputs, the answers
// written out as a C++ header the tests include. Only in this tool, never in
// the library. For P_SHA256 and P_SHA384: secrets of 32 to 64 bytes, the
// labels the client uses ("extended master secret", "key expansion",
// "client finished", "server finished") and one of the tests', seeds of one
// part and of two, outputs of 12 to 136 bytes.
//
// From the root of the tree:
//
//     clang++ -std=c++20 -O1 -I/opt/homebrew/opt/openssl@3/include tools/tls12_prf_oracle.cpp \
//         -L/opt/homebrew/opt/openssl@3/lib -lcrypto -o /tmp/tls12_prf_oracle
//     /tmp/tls12_prf_oracle > tests/net/tls12_prf_vectors.h
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {
    using bytes = std::vector<unsigned char>;

    bytes random(size_t n) {
        bytes b(n);
        RAND_bytes(b.data(), int(n));
        return b;
    }

    std::string hex(const bytes& b) {
        static const char* d = "0123456789abcdef";
        std::string s;
        for (unsigned char c : b) {
            s += d[c >> 4];
            s += d[c & 15];
        }
        return s;
    }

    bytes prf(const char* digest, const bytes& secret, const std::string& label, const bytes& seed1, const bytes& seed2, size_t n) {
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "TLS1-PRF", nullptr);
        EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
        OSSL_PARAM params[6], *p = params;
        *p++ = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>(digest), 0);
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SECRET, const_cast<unsigned char*>(secret.data()), secret.size());
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED, const_cast<char*>(label.data()), label.size());
        *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED, const_cast<unsigned char*>(seed1.data()), seed1.size());
        if (!seed2.empty()) {
            *p++ = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SEED, const_cast<unsigned char*>(seed2.data()), seed2.size());
        }
        *p = OSSL_PARAM_construct_end();
        bytes out(n);
        if (EVP_KDF_derive(ctx, out.data(), n, params) <= 0) {
            std::fprintf(stderr, "TLS1-PRF failed\n");
            std::exit(1);
        }
        EVP_KDF_CTX_free(ctx);
        EVP_KDF_free(kdf);
        return out;
    }
}

int main() {
    std::printf("// Made by tools/tls12_prf_oracle.cpp (%s): do not edit.\n#pragma once\n\n", OPENSSL_VERSION_TEXT);
    std::printf("namespace tls12_vectors {\n");
    std::printf("    // OpenSSL's TLS1-PRF on random inputs: PRF(secret, label, seed1 || seed2)\n");
    std::printf("    struct Prf {\n        int hash;\n        const char *secret, *label, *seed1, *seed2, *out;\n    };\n\n");
    std::printf("    inline const Prf prfs[] = {\n");
    const char* labels[] = {"extended master secret", "key expansion", "client finished", "server finished", "test label"};
    const size_t secrets[] = {48, 32, 48, 48, 64};
    const size_t outs[] = {48, 88, 12, 12, 136};
    for (int h : {256, 384}) {
        for (int i = 0; i < 5; ++i) {
            bytes secret = random(secrets[i]);
            bytes seed1 = random(i == 1 ? 32 : h / 8);
            bytes seed2 = i == 1 || i == 4 ? random(32) : bytes();
            bytes out = prf(h == 256 ? "SHA256" : "SHA384", secret, labels[i], seed1, seed2, outs[i]);
            std::printf("    {%d, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"},\n", h, hex(secret).c_str(), labels[i], hex(seed1).c_str(), hex(seed2).c_str(), hex(out).c_str());
        }
    }
    std::printf("    };\n}\n");
}
