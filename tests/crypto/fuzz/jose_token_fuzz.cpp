//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// JWS, JWE and JWT on any bytes: read (compact and JSON), verified and
// decrypted under keys made once (HS256, ES256, ES512, EdDSA; dir,
// A128GCMKW, A128KW, ECDH-ES on P-256 and X25519, ECDH-ES+A256KW on P-521),
// the claims of a JWT checked. No input can
// verify or decrypt but one made with those keys (the seeds hold some, and
// the fuzzer mutates them), and nothing may crash or read out of bounds.
// Then the input as a payload and a plaintext: signed and verified, sealed
// and opened under each key, it comes back byte for byte.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/jose_token_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"
#include "sgcl/time.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
    using namespace sgcl;
    namespace jose = sgcl::crypto::jose;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "jose_token_fuzz: %s\n", what);
            std::abort();
        }
    }

    // the keys, made once from fixed bytes so that seeds made with them verify
    struct Keys {
        jose::jwk hs = *jose::jwk::parse(string(R"({"kty":"oct","k":"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"})"));
        jose::jwk es = *jose::jwk::parse(string(
            R"({"kty":"EC","crv":"P-256","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","d":"870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE"})"));
        jose::jwk ed = *jose::jwk::parse(string(
            R"({"kty":"OKP","crv":"Ed25519","d":"nWGxne_9WmC6hEr0kuwsxERJxWl7MmkZcDusAxyuf2A","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})"));
        jose::jwk dir = *jose::jwk::parse(string(R"({"kty":"oct","alg":"dir","k":"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"})"));
        jose::jwk kw = *jose::jwk::parse(string(R"({"kty":"oct","alg":"A128GCMKW","k":"AAECAwQFBgcICQoLDA0ODw"})"));
        jose::jwk ecdh = *jose::jwk::parse(string(
            R"({"kty":"EC","crv":"P-256","alg":"ECDH-ES","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","d":"870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE"})"));
        jose::jwk x = *jose::jwk::parse(string(
            R"({"kty":"OKP","crv":"X25519","alg":"ECDH-ES","d":"dwdtCnMYpX08FsFyUbJmRd9ML4frwJkqsXf7pR25LCo","x":"hSDwCYkwp1R0i33ctD73Wg2_Og0mOBr066SpjqqbTmo"})"));
        jose::jwk aeskw = *jose::jwk::parse(string(R"({"kty":"oct","alg":"A128KW","k":"AAECAwQFBgcICQoLDA0ODw"})"));
        // P-521 of a fixed scalar, ES512 and ECDH-ES+A256KW
        jose::jwk es512 = jose::jwk(crypto::p521::private_key::from_bytes(fixed521()).value(), {.alg = jose::algorithm::es512});
        jose::jwk ecdh521 = jose::jwk(crypto::p521::private_key::from_bytes(fixed521()).value(), {.alg = jose::algorithm::ecdh_es_a256kw});

        static slice<const byte> fixed521() {
            static const uint8_t d[66] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                                          33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65};
            return slice<const byte>(reinterpret_cast<const byte*>(d), sizeof d);
        }
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const rooted<Keys> keys(std::in_place);   // made once, kept by a root
    // parsed from libFuzzer's buffer itself (ASan sees no read past a managed copy)
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    using Text = crypto::detail::JoseTextAccess;
    const slice<const byte> raw(reinterpret_cast<const byte*>(data), size);
    // reading and verifying
    if (auto j = Text::jws_of(input)) {
        for (size_t i = 0; i < j->signature_count(); ++i) {
            (void)j->header(i);
            (void)j->alg(i);
            (void)j->kid(i);
        }
        (void)j->unverified_payload();
        for (const auto* k : {&keys->hs, &keys->es, &keys->es512, &keys->ed}) {
            auto v = j->verify(*k);
            if (v) {
                check(*v == j->unverified_payload(), "a verified payload that is not the one read");
            }
        }
        (void)j->verify(jose::jwk_set{keys->es.public_key(), keys->ed.public_key()});
    }
    if (auto t = Text::jwt_unverified(input)) {
        (void)t->audience();
        (void)t->expires_at();
        (void)t->issued_at();
        (void)t->not_before();
        (void)Text::jwt_verify(input, keys->hs, jose::jwt::verify_options{.audience = "a", .require_expiration = false});
    }
    if (auto e = Text::jwe_of(input)) {
        (void)e->alg();
        (void)e->enc();
        for (const auto* k : {&keys->dir, &keys->kw, &keys->ecdh, &keys->x, &keys->aeskw, &keys->ecdh521}) {
            (void)e->decrypt(*k);
        }
    }
    // the input as a payload and a plaintext
    if (size <= 4096) {
        for (const auto* k : {&keys->hs, &keys->es, &keys->es512, &keys->ed}) {
            auto v = jose::jws::verify(jose::jws::sign(raw, *k), *k);
            check(v && *v == vector<byte>(raw.data(), raw.data() + size), "a payload signed does not verify to itself");
        }
        const jose::encryption enc = jose::encryption(1 + (size % 6));
        const size_t cek = enc == jose::encryption::a128gcm ? 16 : enc == jose::encryption::a192gcm ? 24 : enc == jose::encryption::a256gcm || enc == jose::encryption::a128cbc_hs256 ? 32 : 0;
        for (const auto* k : {&keys->kw, &keys->ecdh, &keys->x, &keys->aeskw, &keys->ecdh521}) {
            auto d = jose::jwe::decrypt(jose::jwe::encrypt(raw, *k, {.enc = enc}), *k);
            check(d && *d == vector<byte>(raw.data(), raw.data() + size), "a plaintext sealed does not open to itself");
        }
        if (cek == 32) {
            auto d = jose::jwe::decrypt(jose::jwe::encrypt(raw, keys->dir, {.enc = enc}), keys->dir);
            check(d && *d == vector<byte>(raw.data(), raw.data() + size), "a plaintext sealed under dir does not open to itself");
        }
    }
    return 0;
}
