//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X.509 against OpenSSL: one operation, one side a run. Prints one line:
// microseconds per operation and operations per second.
//
//   x509 <case>-<chain> <sgcl|openssl>
//
//   parse     the leaf's DER read: certificate::parse against d2i_X509
//             (and X509_free)
//   verify    the chain root -> intermediate -> leaf verified with a host
//             name, the pools made once: certificate::verify against
//             X509_verify_cert (a store made once, a context each time)
//
//   chain     ecdsa: P-256 keys, ECDSA with SHA-256; rsa: RSA-2048 keys,
//             PKCS #1 v1.5 with SHA-256
//
// The chains are made by OpenSSL at the start; bench_x509 links libcrypto
// only when benchmarks/CMakeLists.txt finds OpenSSL, and without it has no
// case. About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
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

#if defined(SGCL_BENCH_OPENSSL)
    namespace x509 = sgcl::crypto::x509;
    constexpr long now = 1800000000;

    struct made {
        X509* cert;
        EVP_PKEY* key;
        std::vector<unsigned char> der;
    };

    void ext(X509* x, X509* issuer, int nid, const char* value) {
        X509V3_CTX ctx;
        X509V3_set_ctx_nodb(&ctx);
        X509V3_set_ctx(&ctx, issuer, x, nullptr, nullptr, 0);
        X509_EXTENSION* e = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
        X509_add_ext(x, e, -1);
        X509_EXTENSION_free(e);
    }

    made make(bool rsa, const char* cn, bool ca, const made* issuer) {
        made m;
        m.key = rsa ? EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(2048)) : EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
        X509* x = m.cert = X509_new();
        X509_set_version(x, 2);
        ASN1_INTEGER_set(X509_get_serialNumber(x), 7);
        X509_NAME_add_entry_by_txt(X509_get_subject_name(x), "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(cn), -1, -1, 0);
        X509_set_issuer_name(x, issuer ? X509_get_subject_name(issuer->cert) : X509_get_subject_name(x));
        ASN1_TIME_set(X509_getm_notBefore(x), now - 86400);
        ASN1_TIME_set(X509_getm_notAfter(x), now + 86400 * 365);
        X509_set_pubkey(x, m.key);
        X509* iss = issuer ? issuer->cert : x;
        if (ca) {
            ext(x, iss, NID_basic_constraints, "critical,CA:TRUE");
            ext(x, iss, NID_key_usage, "critical,keyCertSign,cRLSign");
        } else {
            ext(x, iss, NID_basic_constraints, "critical,CA:FALSE");
            ext(x, iss, NID_key_usage, "critical,digitalSignature");
            ext(x, iss, NID_ext_key_usage, "serverAuth");
            ext(x, iss, NID_subject_alt_name, "DNS:www.example.com,DNS:*.example.org");
        }
        ext(x, iss, NID_subject_key_identifier, "hash");
        ext(x, iss, NID_authority_key_identifier, "keyid");
        X509_sign(x, issuer ? issuer->key : m.key, EVP_sha256());
        unsigned char* p = nullptr;
        int n = i2d_X509(x, &p);
        m.der.assign(p, p + n);
        OPENSSL_free(p);
        return m;
    }

    sgcl::slice<const sgcl::byte> as_slice(const std::vector<unsigned char>& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }
#endif
}

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    std::string what = argc > 1 ? argv[1] : "";
    size_t dash = what.find('-');
    if (argc < 3 || (side != "sgcl" && side != "openssl") || dash == std::string::npos) {
        std::fprintf(stderr, "usage: x509 <parse|verify>-<ecdsa|rsa> <sgcl|openssl>\n");
        return 2;
    }
    std::string op = what.substr(0, dash);
    std::string chain = what.substr(dash + 1);
    auto measure = [&](auto f) {
        run_for(f, 0.25);   // thrown away
        auto [calls, wall] = run_for(f, 2.0);
        double us = wall * 1e6 / double(calls);
        std::printf("x509 %s %s us/op=%.2f op/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), us, double(calls) / wall, wall);
    };
#if defined(SGCL_BENCH_OPENSSL)
    if ((chain != "ecdsa" && chain != "rsa") || (op != "parse" && op != "verify")) {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    bool rsa = chain == "rsa";
    made root = make(rsa, "Bench Root", true, nullptr);
    made inter = make(rsa, "Bench Intermediate", true, &root);
    made leaf = make(rsa, "www.example.com", false, &inter);
    if (side == "sgcl") {
        if (op == "parse") {
            measure([&] { return uint64_t(x509::certificate::parse(as_slice(leaf.der))->version()); });
        } else {
            x509::verify_options o;
            x509::certificate_pool roots;
            roots.add(*x509::certificate::parse(as_slice(root.der)));
            o.roots = roots;
            o.intermediates.add(*x509::certificate::parse(as_slice(inter.der)));
            o.dns_name = "www.example.com";
            o.time = sgcl::time::datetime::from_unix(now, sgcl::time::zone::utc());
            auto c = *x509::certificate::parse(as_slice(leaf.der));
            if (!c.verify(o)) {
                std::fprintf(stderr, "the chain does not verify\n");
                return 1;
            }
            measure([&] { return uint64_t(c.verify(o)->size()); });
        }
    } else {
        if (op == "parse") {
            measure([&] {
                const unsigned char* p = leaf.der.data();
                X509* x = d2i_X509(nullptr, &p, long(leaf.der.size()));
                uint64_t v = uint64_t(X509_get_version(x));
                X509_free(x);
                return v;
            });
        } else {
            X509_STORE* store = X509_STORE_new();
            X509_STORE_add_cert(store, root.cert);
            STACK_OF(X509)* untrusted = sk_X509_new_null();
            sk_X509_push(untrusted, inter.cert);
            measure([&] {
                X509_STORE_CTX* ctx = X509_STORE_CTX_new();
                X509_STORE_CTX_init(ctx, store, leaf.cert, untrusted);
                X509_VERIFY_PARAM* p = X509_STORE_CTX_get0_param(ctx);
                X509_VERIFY_PARAM_set_time(p, now);
                X509_VERIFY_PARAM_set_purpose(p, X509_PURPOSE_SSL_SERVER);
                X509_VERIFY_PARAM_set1_host(p, "www.example.com", 0);
                int ok = X509_verify_cert(ctx);
                X509_STORE_CTX_free(ctx);
                return uint64_t(ok);
            });
            sk_X509_free(untrusted);
            X509_STORE_free(store);
        }
    }
    return 0;
#else
    (void)measure;
    std::fprintf(stderr, "built without OpenSSL: the chains are made by it\n");
    return 1;
#endif
}
