//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// RSA against OpenSSL: one operation, one side a run. Prints one line:
// microseconds per operation and operations per second.
//
//   rsa <bits>-<case> <sgcl|openssl>      bits: 2048, 3072 or 4096
//
//   sign      a PKCS #1 v1.5 signature of a SHA-256 digest: sign_digest
//             against EVP_PKEY_sign (the context made once)
//   pss       a PSS signature, salt of the digest's length
//   verify    the v1.5 signature's verification: verify_digest against
//             EVP_PKEY_verify
//   encrypt   OAEP with SHA-256 of 32 bytes: encrypt_oaep against
//             EVP_PKEY_encrypt
//   decrypt   its decryption: decrypt_oaep against EVP_PKEY_decrypt
//
// Both sides use the same key, made by OpenSSL when it is linked (by
// private_key::generate otherwise). bench_rsa links libcrypto only when
// benchmarks/CMakeLists.txt finds OpenSSL. About two seconds a run after a
// quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#endif

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
    volatile uint64_t sink;

    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        auto t0 = bench::Clock::now();
        double wall = 0;
        do {
            for (int i = 0; i < 4; ++i) {
                acc += f();
            }
            calls += 4;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    sgcl::slice<const sgcl::byte> as_slice(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    const unsigned char digest[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                      17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    using sgcl::crypto::hash_id;

    int sgcl_case(sgcl::crypto::rsa::private_key& key, const std::string& op, auto measure) {
        auto pub = key.public_key();
        auto d = as_slice(digest, 32);
        if (op == "sign") {
            measure([&] { return uint64_t(key.sign_digest(hash_id::sha256, d)[0]); });
        } else if (op == "pss") {
            measure([&] { return uint64_t(key.sign_digest_pss(hash_id::sha256, d)[0]); });
        } else if (op == "verify") {
            auto sig = key.sign_digest(hash_id::sha256, d);
            measure([&] { return uint64_t(pub.verify_digest(hash_id::sha256, d, sig.as_slice())); });
        } else if (op == "encrypt") {
            measure([&] { return uint64_t(pub.encrypt_oaep(hash_id::sha256, d)[0]); });
        } else if (op == "decrypt") {
            auto ct = pub.encrypt_oaep(hash_id::sha256, d);
            measure([&] { return uint64_t(key.decrypt_oaep(hash_id::sha256, ct.as_slice())->size()); });
        } else {
            return 2;
        }
        return 0;
    }

#if defined(SGCL_BENCH_OPENSSL)
    int openssl_case(EVP_PKEY* key, const std::string& op, auto measure) {
        auto sign_ctx = [&](bool pss) {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_sign_init(ctx);
            EVP_PKEY_CTX_set_rsa_padding(ctx, pss ? RSA_PKCS1_PSS_PADDING : RSA_PKCS1_PADDING);
            EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256());
            if (pss) {
                EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, RSA_PSS_SALTLEN_DIGEST);
            }
            return ctx;
        };
        auto oaep = [&](EVP_PKEY_CTX* ctx) {
            EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);
            EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256());
            EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());
        };
        std::vector<unsigned char> buf(1024);
        if (op == "sign" || op == "pss") {
            EVP_PKEY_CTX* ctx = sign_ctx(op == "pss");
            measure([&] {
                size_t n = buf.size();
                EVP_PKEY_sign(ctx, buf.data(), &n, digest, 32);
                return uint64_t(buf[0]);
            });
            EVP_PKEY_CTX_free(ctx);
        } else if (op == "verify") {
            EVP_PKEY_CTX* ctx = sign_ctx(false);
            size_t n = buf.size();
            EVP_PKEY_sign(ctx, buf.data(), &n, digest, 32);
            EVP_PKEY_CTX_free(ctx);
            ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_verify_init(ctx);
            EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING);
            EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256());
            measure([&] { return uint64_t(EVP_PKEY_verify(ctx, buf.data(), n, digest, 32)); });
            EVP_PKEY_CTX_free(ctx);
        } else if (op == "encrypt" || op == "decrypt") {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_encrypt_init(ctx);
            oaep(ctx);
            if (op == "encrypt") {
                measure([&] {
                    size_t n = buf.size();
                    EVP_PKEY_encrypt(ctx, buf.data(), &n, digest, 32);
                    return uint64_t(buf[0]);
                });
            } else {
                size_t n = buf.size();
                EVP_PKEY_encrypt(ctx, buf.data(), &n, digest, 32);
                EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new(key, nullptr);
                EVP_PKEY_decrypt_init(dctx);
                oaep(dctx);
                std::vector<unsigned char> out(1024);
                measure([&] {
                    size_t m = out.size();
                    EVP_PKEY_decrypt(dctx, out.data(), &m, buf.data(), n);
                    return uint64_t(m);
                });
                EVP_PKEY_CTX_free(dctx);
            }
            EVP_PKEY_CTX_free(ctx);
        } else {
            return 2;
        }
        return 0;
    }
#endif
}

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    std::string what = argc > 1 ? argv[1] : "";
    size_t dash = what.find('-');
    if (argc < 3 || (side != "sgcl" && side != "openssl") || dash == std::string::npos) {
        std::fprintf(stderr, "usage: rsa <2048|3072|4096>-<sign|pss|verify|encrypt|decrypt> <sgcl|openssl>\n");
        return 2;
    }
    unsigned bits = unsigned(std::stoul(what.substr(0, dash)));
    std::string op = what.substr(dash + 1);
    auto measure = [&](auto f) {
        run_for(f, 0.25);   // thrown away
        auto [calls, wall] = run_for(f, 2.0);
        double us = wall * 1e6 / double(calls);
        std::printf("rsa %s %s us/op=%.2f op/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), us, double(calls) / wall, wall);
    };
    int rc = 2;
#if defined(SGCL_BENCH_OPENSSL)
    EVP_PKEY* ossl = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(bits));
    unsigned char* der = nullptr;
    PKCS8_PRIV_KEY_INFO* info = EVP_PKEY2PKCS8(ossl);
    int len = i2d_PKCS8_PRIV_KEY_INFO(info, &der);
    auto parsed = sgcl::crypto::rsa::private_key::from_pkcs8_der(as_slice(der, size_t(len)));
    OPENSSL_free(der);
    PKCS8_PRIV_KEY_INFO_free(info);
    if (!parsed) {
        std::fprintf(stderr, "the key does not read\n");
        return 1;
    }
    auto key = std::move(*parsed);
    rc = side == "sgcl" ? sgcl_case(key, op, measure) : openssl_case(ossl, op, measure);
    EVP_PKEY_free(ossl);
#else
    auto key = sgcl::crypto::rsa::private_key::generate(bits);
    if (side == "sgcl") {
        rc = sgcl_case(key, op, measure);
    } else {
        std::fprintf(stderr, "built without OpenSSL\n");
    }
#endif
    if (rc != 0) {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
    }
    return rc;
}
