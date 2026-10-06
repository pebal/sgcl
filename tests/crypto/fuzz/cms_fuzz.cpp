//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// CMS and S/MIME on any bytes. The input is read as a SignedData (verified,
// embedded and detached over itself, under the fixed certificate of
// cms_fixed.h as the root), as an EnvelopedData or AuthEnvelopedData
// (decrypted for the fixed certificate and key) and as an S/MIME entity
// (verified, decrypted): nothing crashes or reads out of bounds, and what
// verifies has the fixed signer. Then the input as content: signed and
// verified, encrypted and decrypted in both ciphers, as CMS and as S/MIME,
// it comes back byte for byte. The seeds (seeds/cms) are made with the
// fixed key and certificate.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/cms_fuzz.cpp 300
#include "sgcl/core/rooted.h"
#include "tests/crypto/fuzz/cms_fixed.h"

#include <cstdio>
#include <cstdlib>

namespace {
    using namespace sgcl;
    namespace cms = sgcl::crypto::cms;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "cms_fuzz: %s\n", what);
            std::abort();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const rooted<cms_fuzz::Fixed> fixed(std::in_place);
    const slice<const byte> input(reinterpret_cast<const byte*>(data), size);
    const auto vo = fixed->options();
    if (auto v = cms::verify(input, vo)) {
        check(!v->signer.empty() && v->signer[0] == fixed->cert, "a SignedData verified by another signer than the root");
    }
    (void)cms::verify_detached(input, input, vo);
    (void)cms::decrypt(input, fixed->cert, fixed->key);
    (void)crypto::smime::verify(input, vo);
    (void)crypto::smime::decrypt(input, fixed->cert, fixed->key);
    if (size <= 2048) {
        auto sd = cms::sign(input, fixed->cert, fixed->key);
        auto v = cms::verify(sd, vo);
        check(v && v->content == vector<byte>(input.data(), input.data() + size), "a content signed does not verify to itself");
        sgcl::crypto::x509::chain to;
        to.push_back(fixed->cert);
        for (auto c : {cms::content_cipher::aes256_gcm, cms::content_cipher::aes256_cbc}) {
            auto env = cms::encrypt(input, to, {.cipher = c});
            auto back = cms::decrypt(env, fixed->cert, fixed->key);
            check(back && *back == vector<byte>(input.data(), input.data() + size), "a content encrypted does not decrypt to itself");
        }
        auto sealed = crypto::smime::encrypt(input, to);
        check(bool(crypto::smime::decrypt(sealed, fixed->cert, fixed->key)), "an S/MIME entity sealed does not open");
    }
    return 0;
}
