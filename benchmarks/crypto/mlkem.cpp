//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-KEM (FIPS 203) against OpenSSL: what one operation costs, one case
// and one side a run (benchmarks/go/mlkem has Go's). Prints one line: ns
// per operation and operations per second.
//
//   mlkem <case> <sgcl|openssl>
//
//   keygen512 keygen768 keygen1024     a decapsulation key from a seed of
//                                      64 bytes, with its encapsulation key
//                                      (decapsulation_key::from_seed and
//                                      encapsulation_key() against
//                                      EVP_PKEY_generate with the "seed"
//                                      parameter and get_raw_public_key)
//   import512 import768 import1024     an encapsulation key read from its
//                                      bytes (encapsulation_key::from_bytes,
//                                      its check of §7.2 and what the key
//                                      makes once, against
//                                      EVP_PKEY_new_raw_public_key_ex)
//   encaps512 encaps768 encaps1024     an encapsulation to a key read once
//                                      (encapsulate() against
//                                      EVP_PKEY_encapsulate with a context
//                                      made once), m from the system's
//                                      random bytes on both sides
//   encdet512 encdet768 encdet1024     the same with a message fixed, the
//                                      randomness taken out (sgcl only:
//                                      detail::mlkem::Access::
//                                      encapsulate_with): what encaps costs
//                                      beside its 32 bytes of the system's
//                                      random
//   decaps512 decaps768 decaps1024     the decapsulation of one ciphertext
//                                      (decapsulate() against
//                                      EVP_PKEY_decapsulate, a context made
//                                      once)
//
// bench_mlkem links libcrypto only when benchmarks/CMakeLists.txt finds
// OpenSSL; without it the openssl side is absent. Each run goes for about
// two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/mlkem.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
            for (int i = 0; i < 16; ++i) {
                acc += f();
            }
            calls += 16;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    sgcl::slice<const sgcl::byte> as_slice(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    unsigned char seed[64];

    void fill() {
        for (int i = 0; i < 64; ++i) {
            seed[i] = (unsigned char)(i * 7 + 1);
        }
    }

    template<class F>
    void report(const std::string& what, const std::string& side, F&& f) {
        run_for(f, 0.25);
        auto [calls, wall] = run_for(f, 2.0);
        double ns = wall * 1e9 / double(calls);
        std::printf("mlkem %s %s ns/op=%.0f op/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), ns, 1e9 / ns, wall);
    }

    template<class Dk>
    int ours(const std::string& what, const std::string& op) {
        auto dk = std::move(*Dk::from_seed(as_slice(seed, 64)));
        auto ek = dk.encapsulation_key();
        auto e = ek.encapsulate();
        if (op == "keygen") {
            report(what, "sgcl", [&] {
                auto k = Dk::from_seed(as_slice(seed, 64));
                return uint64_t(std::to_integer<unsigned>(k->encapsulation_key().bytes()[0]));
            });
        } else if (op == "import") {
            auto bytes = ek.bytes();
            using Ek = decltype(ek);
            report(what, "sgcl", [&] { return uint64_t(Ek::from_bytes(bytes.as_slice()).has_value()); });
        } else if (op == "encaps") {
            report(what, "sgcl", [&] { return uint64_t(std::to_integer<unsigned>(ek.encapsulate().ciphertext[0])); });
        } else if (op == "encdet") {
            const uint8_t m[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                   17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
            report(what, "sgcl", [&] { return uint64_t(std::to_integer<unsigned>(sgcl::crypto::detail::mlkem::Access::encapsulate_with(ek, m).ciphertext[0])); });
        } else if (op == "decaps") {
            report(what, "sgcl", [&] { return uint64_t(std::to_integer<unsigned>(dk.decapsulate(e.ciphertext.as_slice())->bytes()[0])); });
        } else {
            return 2;
        }
        return 0;
    }

#if defined(SGCL_BENCH_OPENSSL)
    EVP_PKEY* openssl_key(const char* name) {
        EVP_PKEY_CTX* gen = EVP_PKEY_CTX_new_from_name(nullptr, name, nullptr);
        OSSL_PARAM p[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_KEM_SEED, seed, 64), OSSL_PARAM_construct_end()};
        EVP_PKEY* key = nullptr;
        EVP_PKEY_keygen_init(gen);
        EVP_PKEY_CTX_set_params(gen, p);
        EVP_PKEY_generate(gen, &key);
        EVP_PKEY_CTX_free(gen);
        return key;
    }

    int theirs(const std::string& what, const std::string& op, const char* name) {
        EVP_PKEY* key = openssl_key(name);
        if (!key) {
            std::fprintf(stderr, "OpenSSL has no %s\n", name);
            return 2;
        }
        EVP_PKEY_CTX* enc = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
        EVP_PKEY_encapsulate_init(enc, nullptr);
        size_t clen = 0, klen = 0;
        EVP_PKEY_encapsulate(enc, nullptr, &clen, nullptr, &klen);
        std::vector<unsigned char> c(clen), k(klen);
        EVP_PKEY_encapsulate(enc, c.data(), &clen, k.data(), &klen);
        EVP_PKEY_CTX* dec = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
        EVP_PKEY_decapsulate_init(dec, nullptr);
        if (op == "keygen") {
            report(what, "openssl", [&] {
                EVP_PKEY* made = openssl_key(name);
                unsigned char pub[1568];
                size_t n = sizeof pub;
                EVP_PKEY_get_raw_public_key(made, pub, &n);
                EVP_PKEY_free(made);
                return uint64_t(pub[0]);
            });
        } else if (op == "import") {
            unsigned char pub[1568];
            size_t pn = sizeof pub;
            EVP_PKEY_get_raw_public_key(key, pub, &pn);
            report(what, "openssl", [&] {
                EVP_PKEY* k = EVP_PKEY_new_raw_public_key_ex(nullptr, name, nullptr, pub, pn);
                uint64_t ok = k != nullptr;
                EVP_PKEY_free(k);
                return ok;
            });
        } else if (op == "encaps") {
            report(what, "openssl", [&] {
                size_t cl = clen, kl = klen;
                EVP_PKEY_encapsulate(enc, c.data(), &cl, k.data(), &kl);
                return uint64_t(c[0]);
            });
        } else if (op == "decaps") {
            report(what, "openssl", [&] {
                unsigned char out[32];
                size_t n = 32;
                EVP_PKEY_decapsulate(dec, out, &n, c.data(), c.size());
                return uint64_t(out[0]);
            });
        } else {
            return 2;
        }
        EVP_PKEY_CTX_free(enc);
        EVP_PKEY_CTX_free(dec);
        EVP_PKEY_free(key);
        return 0;
    }
#endif
}

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    std::string what = argc > 1 ? argv[1] : "";
    std::string op = what.substr(0, 6), set = what.size() > 6 ? what.substr(6) : "";
    if (argc < 3 || (side != "sgcl" && side != "openssl") || (op != "keygen" && op != "import" && op != "encaps" && op != "decaps" && op != "encdet")
        || (set != "512" && set != "768" && set != "1024")) {
        std::fprintf(stderr, "usage: mlkem <keygen|import|encaps|encdet|decaps><512|768|1024> <sgcl|openssl>\n");
        return 2;
    }
    fill();
    if (side == "sgcl") {
        if (set == "512") {
            return ours<sgcl::crypto::mlkem512::decapsulation_key>(what, op);
        }
        if (set == "768") {
            return ours<sgcl::crypto::mlkem768::decapsulation_key>(what, op);
        }
        return ours<sgcl::crypto::mlkem1024::decapsulation_key>(what, op);
    }
#if defined(SGCL_BENCH_OPENSSL)
    return theirs(what, op, set == "512" ? "ML-KEM-512" : set == "768" ? "ML-KEM-768" : "ML-KEM-1024");
#else
    std::fprintf(stderr, "built without OpenSSL\n");
    return 2;
#endif
}
