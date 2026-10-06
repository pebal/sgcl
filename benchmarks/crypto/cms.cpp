//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::cms (RFC 5652): one case a run over 1 KB of content, the key and
// its self-signed certificate made before the clock starts. Prints one
// line with ns/op. Go's standard library has no CMS; the reference is
// OpenSSL 3.6's CMS_sign, CMS_verify, CMS_encrypt and CMS_decrypt with the
// same algorithms (a program of its own outside the tree).
//
//   cms <case> sgcl
//
//   sign_p256, sign_rsa
//       a SignedData with signed attributes and the certificate (ECDSA
//       P-256 with SHA-256; RSA 2048 PKCS #1 v1.5 with SHA-256)
//   verify_p256, verify_rsa
//       its verification: the digest, the signature, the chain (the
//       certificate its own root)
//   encrypt_p256, encrypt_rsa
//       an AuthEnvelopedData of AES-256-GCM to the certificate (ECDH with
//       the X9.63 KDF and AES key wrap; RSAES-OAEP of SHA-256)
//   decrypt_p256, decrypt_rsa
//       its decryption
//
// About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/cms.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;
    namespace cms = sgcl::crypto::cms;

    volatile uint64_t sink;

    template<class F>
    void measure(const char* what, F&& f) {
        auto run_for = [&](double seconds) {
            uint64_t calls = 0, acc = 0;
            auto t0 = bench::Clock::now();
            double wall = 0;
            do {
                acc += f();
                ++calls;
                wall = bench::seconds_since(t0);
            } while (wall < seconds);
            sink = acc;
            return std::pair<uint64_t, double>(calls, wall);
        };
        run_for(0.25);
        auto [calls, wall] = run_for(2.0);
        std::printf("cms %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }

    template<class Key>
    int run(const std::string& what, const Key& key) {
        crypto::x509::certificate_template t;
        t.common_name = "bench";
        t.ext_key_usages = {crypto::x509::ext_key_usage::email_protection};
        auto cert = crypto::x509::create_certificate(t, key);
        crypto::x509::certificate_pool roots;
        roots.add(cert);
        cms::verify_options vo;
        vo.chain.roots = roots;
        crypto::x509::chain to;
        to.push_back(cert);
        const std::string data(1024, 'd');
        const slice<const byte> content(reinterpret_cast<const byte*>(data.data()), data.size());
        if (what.rfind("sign_", 0) == 0) {
            measure(what.c_str(), [&] { return cms::sign(content, cert, key).size(); });
        } else if (what.rfind("verify_", 0) == 0) {
            auto sd = cms::sign(content, cert, key);
            measure(what.c_str(), [&] { return cms::verify(sd, vo)->content.size(); });
        } else if (what.rfind("encrypt_", 0) == 0) {
            measure(what.c_str(), [&] { return cms::encrypt(content, to).size(); });
        } else {
            auto env = cms::encrypt(content, to);
            measure(what.c_str(), [&] { return cms::decrypt(env, cert, key)->size(); });
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: cms <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what.find("_p256") != std::string::npos) {
        return run(what, crypto::p256::private_key::generate());
    }
    if (what.find("_rsa") != std::string::npos) {
        return run(what, crypto::rsa::private_key::generate(2048));
    }
    std::fprintf(stderr, "cms: unknown case %s\n", what.c_str());
    return 2;
}
