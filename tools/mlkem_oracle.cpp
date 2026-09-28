// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl::crypto's ML-KEM (FIPS 203): OpenSSL (3.5 and
// later; the system's libcrypto, never linked by the library) asked the
// same questions case by case in this one process, every answer compared
// byte for byte, and all of them folded into the hash tools/mlkem_oracle.go
// prints for Go. Only in this tool, never in the library or its tests.
// From the root of the tree:
//
//     clang++ -std=c++20 -O2 -I. -I/opt/homebrew/opt/openssl@3/include tools/mlkem_oracle.cpp \
//         -L/opt/homebrew/opt/openssl@3/lib -lcrypto -o /tmp/mlkem_oracle
//     /tmp/mlkem_oracle 768 100000     # 512, 768 or 1024; then, for 768 and 1024:
//     go run tools/mlkem_oracle.go 768 100000
//
// The cases (the same in the Go tool): SHAKE128 of "sgcl mlkem oracle
// ML-KEM-768" read as a stream, 99 bytes a case — the seed d‖z (64), the
// message m (32), a position (two bytes little-endian, modulo the
// ciphertext's length) and a bit (a byte, bit b mod 8). For each, on both
// sides: the decapsulation key of the seed (OpenSSL: the "seed" parameter
// of its key generation) and its encapsulation key ek; the encapsulation
// of m (OpenSSL: the "ikme" parameter), c and K; the decapsulation of c
// (K again) and of c with the bit flipped (K', the implicit rejection).
// A difference is printed with its case and ends the run. The hash:
// SHAKE128 over ek‖c‖K‖K' of every case in order, 32 bytes.
#include "sgcl/crypto/mlkem.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace mlkem = sgcl::crypto::detail::mlkem;
using bytes = std::vector<unsigned char>;

namespace {
    [[noreturn]] void fail(const char* what) {
        std::fprintf(stderr, "OpenSSL: %s\n", what);
        std::exit(1);
    }

    std::string hex(const unsigned char* p, size_t n) {
        static const char* d = "0123456789abcdef";
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            s += d[p[i] >> 4];
            s += d[p[i] & 15];
        }
        return s;
    }

    sgcl::slice<const sgcl::byte> view(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    template<class T>
    bytes of(const T& r) {
        auto p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes(p, p + r.size());
    }

    // OpenSSL's side of one case
    struct Theirs {
        bytes ek, c, k, k_again, k_rejected;
    };

    Theirs openssl(const char* name, const unsigned char seed[64], const unsigned char m[32], size_t at, unsigned char bit) {
        Theirs t;
        EVP_PKEY_CTX* gen = EVP_PKEY_CTX_new_from_name(nullptr, name, nullptr);
        if (!gen || EVP_PKEY_keygen_init(gen) <= 0) {
            fail("keygen_init");
        }
        OSSL_PARAM gp[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_KEM_SEED, const_cast<unsigned char*>(seed), 64), OSSL_PARAM_construct_end()};
        EVP_PKEY* key = nullptr;
        if (EVP_PKEY_CTX_set_params(gen, gp) <= 0 || EVP_PKEY_generate(gen, &key) <= 0) {
            fail("generate from the seed");
        }
        EVP_PKEY_CTX_free(gen);
        size_t n = 0;
        if (EVP_PKEY_get_raw_public_key(key, nullptr, &n) <= 0) {
            fail("the public key's size");
        }
        t.ek.resize(n);
        if (EVP_PKEY_get_raw_public_key(key, t.ek.data(), &n) <= 0) {
            fail("the public key");
        }
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
        OSSL_PARAM ep[] = {OSSL_PARAM_construct_octet_string(OSSL_KEM_PARAM_IKME, const_cast<unsigned char*>(m), 32), OSSL_PARAM_construct_end()};
        size_t clen = 0, klen = 0;
        if (EVP_PKEY_encapsulate_init(ctx, ep) <= 0 || EVP_PKEY_encapsulate(ctx, nullptr, &clen, nullptr, &klen) <= 0) {
            fail("encapsulate_init");
        }
        t.c.resize(clen);
        t.k.resize(klen);
        if (EVP_PKEY_encapsulate(ctx, t.c.data(), &clen, t.k.data(), &klen) <= 0) {
            fail("encapsulate");
        }
        auto decaps = [&](const bytes& c, bytes& out) {
            size_t len = 32;
            out.resize(len);
            if (EVP_PKEY_decapsulate_init(ctx, nullptr) <= 0 || EVP_PKEY_decapsulate(ctx, out.data(), &len, c.data(), c.size()) <= 0) {
                fail("decapsulate");
            }
            out.resize(len);
        };
        decaps(t.c, t.k_again);
        bytes bad = t.c;
        bad[at % bad.size()] ^= bit;
        decaps(bad, t.k_rejected);
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(key);
        return t;
    }

    template<class Dk, class Ek>
    int run(const char* name, const std::string& set, long cases) {
        using namespace sgcl::crypto;
        detail::KeccakSponge<168> stream, acc;
        stream.init();
        std::string label = "sgcl mlkem oracle ML-KEM-" + set;
        stream.absorb(reinterpret_cast<const unsigned char*>(label.data()), label.size());
        stream.pad(0x1F);
        acc.init();
        auto t0 = std::chrono::steady_clock::now();
        double theirs_seconds = 0;
        for (long i = 0; i < cases; ++i) {
            unsigned char seed[64], m[32], place[3];
            stream.squeeze(seed, 64);
            stream.squeeze(m, 32);
            stream.squeeze(place, 3);
            size_t at = size_t(place[0]) | (size_t(place[1]) << 8);
            unsigned char bit = (unsigned char)(1u << (place[2] % 8));
            // ours
            auto dk = Dk::from_seed(view(seed, 64));
            if (!dk) {
                std::printf("case %ld: our from_seed failed\n", i);
                return 1;
            }
            auto pub = dk->encapsulation_key();
            bytes ek = of(pub.bytes());
            auto e = detail::mlkem::Access::encapsulate_with(pub, m);
            bytes c = of(e.ciphertext), k = of(e.shared_key.bytes());
            bytes k_again = of(dk->decapsulate(e.ciphertext.as_slice()).value().bytes());
            bytes bad = c;
            bad[at % bad.size()] ^= bit;
            bytes k_rejected = of(dk->decapsulate(view(bad.data(), bad.size())).value().bytes());
            // theirs
            auto s0 = std::chrono::steady_clock::now();
            Theirs t = openssl(name, seed, m, at, bit);
            theirs_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - s0).count();
            auto differ = [&](const char* what, const bytes& a, const bytes& b) {
                if (a != b) {
                    std::printf("case %ld: %s differs\n  ours   %s\n  theirs %s\n", i, what, hex(a.data(), a.size()).c_str(), hex(b.data(), b.size()).c_str());
                    std::exit(1);
                }
            };
            differ("ek", ek, t.ek);
            differ("c", c, t.c);
            differ("K", k, t.k);
            differ("K of decapsulation", k_again, t.k_again);
            differ("K of the flipped ciphertext", k_rejected, t.k_rejected);
            if (k_again != k) {
                std::printf("case %ld: our decapsulation is not our encapsulation's key\n", i);
                return 1;
            }
            if (k_rejected == k) {
                std::printf("case %ld: the flipped ciphertext gave the genuine key\n", i);
                return 1;
            }
            acc.absorb(ek.data(), ek.size());
            acc.absorb(c.data(), c.size());
            acc.absorb(k.data(), k.size());
            acc.absorb(k_rejected.data(), k_rejected.size());
        }
        acc.pad(0x1F);
        unsigned char sum[32];
        acc.squeeze(sum, 32);
        double all = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("sgcl = OpenSSL ML-KEM-%s cases %ld accumulated %s (%.2f s, OpenSSL %.2f s of it)\n", set.c_str(), cases, hex(sum, 32).c_str(), all, theirs_seconds);
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: mlkem_oracle <512|768|1024> <cases>\n");
        return 2;
    }
    std::string set = argv[1];
    long cases = std::atol(argv[2]);
    if (set == "512") {
        return run<sgcl::crypto::mlkem512::decapsulation_key, sgcl::crypto::mlkem512::encapsulation_key>("ML-KEM-512", set, cases);
    }
    if (set == "768") {
        return run<sgcl::crypto::mlkem768::decapsulation_key, sgcl::crypto::mlkem768::encapsulation_key>("ML-KEM-768", set, cases);
    }
    if (set == "1024") {
        return run<sgcl::crypto::mlkem1024::decapsulation_key, sgcl::crypto::mlkem1024::encapsulation_key>("ML-KEM-1024", set, cases);
    }
    std::fprintf(stderr, "512, 768 or 1024\n");
    return 2;
}
