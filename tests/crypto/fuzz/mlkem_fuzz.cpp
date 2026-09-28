//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-KEM (FIPS 203) on any bytes, against OpenSSL. The input's first byte
// picks the parameter set (512, 768, 1024); then a seed d‖z (64 bytes), a
// message m (32), and the rest, taken as an encapsulation key and as a
// ciphertext (short input padded with zeros):
//
//   - the rest's first bytes as an encapsulation key: sgcl reads it
//     (§7.2) exactly when OpenSSL imports it; when both do, it writes the
//     same bytes back, and the encapsulation of m to it is OpenSSL's (its
//     "ikme" parameter), ciphertext and key;
//   - the seed's key is OpenSSL's (its "seed" parameter), and the rest as
//     a ciphertext decapsulates to OpenSSL's key: the genuine one, or the
//     implicit rejection's for bytes that are none;
//   - the rest at its own length, when that is not the ciphertext's, is
//     errc::malformed.
//
// A difference aborts.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/mlkem_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o mlkem_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./mlkem_fuzz <seconds> <seed files...>
//
// or with libFuzzer (brew's LLVM): -fsanitize=fuzzer,address,undefined in
// place of the driver.
#include "sgcl/crypto/mlkem.h"

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using bytes_t = std::vector<unsigned char>;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "mlkem_fuzz: %s\n", what);
            std::abort();
        }
    }

    sgcl::slice<const sgcl::byte> view(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    template<class T>
    bytes_t of(const T& r) {
        auto p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    bytes_t decapsulate(EVP_PKEY* key, const bytes_t& c) {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, key, nullptr);
        bytes_t out(32);
        size_t len = 32;
        check(ctx && EVP_PKEY_decapsulate_init(ctx, nullptr) > 0 && EVP_PKEY_decapsulate(ctx, out.data(), &len, c.data(), c.size()) > 0, "OpenSSL decapsulate");
        EVP_PKEY_CTX_free(ctx);
        return out;
    }

    template<class Dk, class Ek, class P>
    void run(const char* name, const unsigned char* seed, const unsigned char* m, const unsigned char* rest, size_t n) {
        namespace core = sgcl::crypto::detail::mlkem;
        constexpr size_t ek_size = core::Sizes<P>::ek, ct_size = core::Sizes<P>::ciphertext;
        bytes_t ek(ek_size, 0), c(ct_size, 0);
        std::memcpy(ek.data(), rest, std::min(n, ek_size));
        std::memcpy(c.data(), rest, std::min(n, ct_size));

        // the rest as an encapsulation key
        auto ours = Ek::from_bytes(view(ek.data(), ek.size()));
        EVP_PKEY* theirs = EVP_PKEY_new_raw_public_key_ex(nullptr, name, nullptr, ek.data(), ek.size());
        check(bool(ours) == (theirs != nullptr), "an encapsulation key read by one side only");
        if (ours) {
            check(of(ours->bytes()) == ek, "the encapsulation key written back");
            auto e = core::Access::encapsulate_with(*ours, m);
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, theirs, nullptr);
            OSSL_PARAM ep[] = {OSSL_PARAM_construct_octet_string(OSSL_KEM_PARAM_IKME, const_cast<unsigned char*>(m), 32), OSSL_PARAM_construct_end()};
            bytes_t tc(ct_size), tk(32);
            size_t clen = ct_size, klen = 32;
            check(ctx && EVP_PKEY_encapsulate_init(ctx, ep) > 0 && EVP_PKEY_encapsulate(ctx, tc.data(), &clen, tk.data(), &klen) > 0, "OpenSSL encapsulate");
            EVP_PKEY_CTX_free(ctx);
            check(of(e.ciphertext) == tc, "the ciphertext of m");
            check(of(e.shared_key.bytes()) == tk, "the key of m");
            EVP_PKEY_free(theirs);
        }

        // the seed's key, and the rest as a ciphertext
        auto dk = Dk::from_seed(view(seed, 64));
        check(bool(dk), "a seed of 64 bytes refused");
        EVP_PKEY_CTX* gen = EVP_PKEY_CTX_new_from_name(nullptr, name, nullptr);
        OSSL_PARAM gp[] = {OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_KEM_SEED, const_cast<unsigned char*>(seed), 64), OSSL_PARAM_construct_end()};
        EVP_PKEY* key = nullptr;
        check(gen && EVP_PKEY_keygen_init(gen) > 0 && EVP_PKEY_CTX_set_params(gen, gp) > 0 && EVP_PKEY_generate(gen, &key) > 0, "OpenSSL key from the seed");
        EVP_PKEY_CTX_free(gen);
        bytes_t tek(ek_size);
        size_t len = ek_size;
        check(EVP_PKEY_get_raw_public_key(key, tek.data(), &len) > 0, "OpenSSL public key");
        check(of(dk->encapsulation_key().bytes()) == tek, "the seed's encapsulation key");
        auto k = dk->decapsulate(view(c.data(), c.size()));
        check(bool(k), "a ciphertext of the right length refused");
        check(of(k->bytes()) == decapsulate(key, c), "the key of the ciphertext");
        EVP_PKEY_free(key);

        // the rest at its own length
        if (n != ct_size) {
            auto wrong = dk->decapsulate(view(rest, n));
            check(!wrong && wrong.error().code() == sgcl::crypto::errc::malformed, "a ciphertext of the wrong length");
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 + 64 + 32) {
        return 0;
    }
    const unsigned char* seed = data + 1;
    const unsigned char* m = data + 65;
    const unsigned char* rest = data + 97;
    size_t n = size - 97;
    switch (data[0] % 3) {
        case 0:
            run<sgcl::crypto::mlkem512::decapsulation_key, sgcl::crypto::mlkem512::encapsulation_key, sgcl::crypto::detail::mlkem::Params512>("ML-KEM-512", seed, m, rest, n);
            break;
        case 1:
            run<sgcl::crypto::mlkem768::decapsulation_key, sgcl::crypto::mlkem768::encapsulation_key, sgcl::crypto::detail::mlkem::Params768>("ML-KEM-768", seed, m, rest, n);
            break;
        default:
            run<sgcl::crypto::mlkem1024::decapsulation_key, sgcl::crypto::mlkem1024::encapsulation_key, sgcl::crypto::detail::mlkem::Params1024>("ML-KEM-1024", seed, m, rest, n);
            break;
    }
    return 0;
}
