//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HPKE on any bytes. The first byte picks a suite (the KEM, the KDF, the
// AEAD, the mode); the rest is read as what comes from the wire: a public
// key, an enc, a sealed message and the info, to keys derived once from
// fixed seeds. Nothing that does not come from the keys opens; nothing
// crashes or reads out of bounds. Then the input as a message: sealed in
// that suite and mode and opened again, it comes back byte for byte, and
// both sides export the same secret.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/hpke_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <cstdio>
#include <cstdlib>

namespace {
    using namespace sgcl;
    namespace hpke = sgcl::crypto::hpke;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "hpke_fuzz: %s\n", what);
            std::abort();
        }
    }

    const hpke::kem kems[4] = {hpke::kem::dhkem_x25519, hpke::kem::dhkem_p256, hpke::kem::dhkem_p384, hpke::kem::dhkem_p521};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const uint8_t pick = data[0];
    const hpke::kem k = kems[pick % 4];
    const hpke::suite s{hpke::kdf(1 + (pick / 4) % 3), (pick / 12) % 4 == 3 ? hpke::aead::export_only : hpke::aead(1 + (pick / 12) % 4)};
    const int mode = (pick / 48) % 4;
    const slice<const byte> rest(reinterpret_cast<const byte*>(data + 1), size - 1);
    auto recipient_key = hpke::private_key::derive(k, "the recipient's seed of 48 bytes, fixed .........");
    auto sender_key = hpke::private_key::derive(k, "the sender's seed of 48 bytes, fixed ............");
    auto sender_public = sender_key.public_key();
    static const char psk[] = "a pre-shared key of 32 bytes ...";
    hpke::options so, ro;
    if (mode & 1) {
        so.psk = ro.psk = slice<const byte>(psk);
        so.psk_id = ro.psk_id = slice<const byte>("id");
    }
    const bool auth = mode & 2;
    // what comes from the wire
    (void)hpke::public_key::from_bytes(k, rest);
    (void)hpke::private_key::from_bytes(k, rest);
    const size_t n = k == hpke::kem::dhkem_x25519 ? 32 : k == hpke::kem::dhkem_p256 ? 65 : k == hpke::kem::dhkem_p384 ? 97 : 133;
    if (rest.size() >= n) {
        const auto info = rest.subslice(n, rest.size() - n < 8 ? rest.size() - n : 8);
        auto r = auth ? hpke::recipient::setup(rest.subslice(0, n), recipient_key, s, info, ro, sender_public)
                      : hpke::recipient::setup(rest.subslice(0, n), recipient_key, s, info, ro);
        if (r && s.aead != hpke::aead::export_only) {
            auto o = r->open(rest.subslice(n, rest.size() - n));
            check(!o, "a message opened that no key sealed");
        }
    }
    // the input as a message
    if (rest.size() <= 4096) {
        const auto info = rest.subslice(0, rest.size() < 16 ? rest.size() : 16);
        auto snd = auth ? hpke::sender::setup(recipient_key.public_key(), s, info, so, sender_key) : hpke::sender::setup(recipient_key.public_key(), s, info, so);
        check(bool(snd), "a sender of a valid key not made");
        auto rcv = auth ? hpke::recipient::setup(snd->enc(), recipient_key, s, info, ro, sender_public) : hpke::recipient::setup(snd->enc(), recipient_key, s, info, ro);
        check(bool(rcv), "a recipient of the sender's enc not made");
        if (s.aead != hpke::aead::export_only) {
            for (int i = 0; i < 2; ++i) {
                auto c = snd->seal(rest, "aad");
                auto p = rcv->open(c, "aad");
                check(p && p->size() == rest.size(), "a message sealed does not open to itself");
                for (size_t k = 0; k < rest.size(); ++k) {
                    check((*p)[k] == rest[k], "a message sealed does not open to itself");
                }
            }
        }
        check(snd->export_secret(rest.subslice(0, rest.size() < 32 ? rest.size() : 32), 40) == rcv->export_secret(rest.subslice(0, rest.size() < 32 ? rest.size() : 32), 40),
              "the two sides export different secrets");
    }
    return 0;
}
