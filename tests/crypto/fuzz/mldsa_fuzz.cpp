//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ML-DSA on any bytes. The first byte picks the parameter set (mod 3) and
// what the rest is:
//   bit 2 clear: a public key, a signature and a message, each cut from the
//     rest at the set's sizes (what comes from the wire): verified, under
//     the key read and under a fixed key, the context the next byte's
//     length; a signature no key made does not verify, nothing crashes or
//     reads out of bounds (sigDecode's hints, the norm check);
//   bit 2 set: a seed and a message: the key of the seed signs the message,
//     hedged and deterministic, both verify, the deterministic one twice
//     the same, and a signature with a byte changed does not.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/mldsa_fuzz.cpp 300
#include "sgcl/core/rooted.h"
#include "sgcl/crypto/mldsa.h"

#include <cstdio>
#include <cstdlib>

namespace {
    using namespace sgcl;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "mldsa_fuzz: %s\n", what);
            std::abort();
        }
    }

    template<class Private>
    struct Fixed {
        using Public = decltype(std::declval<const Private&>().public_key());
        Public pub = Private::from_seed(slice<const byte>("a fixed seed of 32 bytes ......."))->public_key();
    };

    struct Keys {
        Fixed<crypto::mldsa44::private_key> k44;
        Fixed<crypto::mldsa65::private_key> k65;
        Fixed<crypto::mldsa87::private_key> k87;
    };

    template<class Private, class Public>
    void one(const Public& fixed, bool sign, const uint8_t* data, size_t size) {
        using P = typename Private::params;
        using S = crypto::detail::mldsa::Sizes<P>;
        const auto bytes = [&](size_t at, size_t n) {
            return slice<const byte>(reinterpret_cast<const byte*>(data + at), n);
        };
        if (!sign) {
            if (size < 1) {
                return;
            }
            const size_t ctx = data[0] % 4 == 3 ? data[0] : 0;
            ++data;
            --size;
            size_t at = 0;
            const size_t pk_n = size >= S::public_key ? S::public_key : 0;
            at += pk_n;
            const size_t sig_n = size - at >= S::signature ? S::signature : size - at;
            const auto sig = bytes(at, sig_n);
            at += sig_n;
            const auto msg = bytes(at, size - at);
            const auto context = bytes(0, ctx < size ? ctx : size);
            if (pk_n) {
                auto pub = Public::from_bytes(bytes(0, pk_n));
                check(bool(pub), "a public key of the right length not read");
                (void)pub->verify(msg, sig, {.context = context});
                check(pub->bytes().size() == S::public_key, "a public key's bytes of another length");
            }
            check(!fixed.verify(msg, sig, {.context = context}), "a signature no key made verified");
            return;
        }
        if (size < 32) {
            return;
        }
        auto key = Private::from_seed(bytes(0, 32)).value();
        const auto msg = bytes(32, size - 32 < 512 ? size - 32 : 512);
        auto pub = key.public_key();
        auto hedged = key.sign(msg);
        check(pub.verify(msg, hedged), "a hedged signature does not verify");
        auto d1 = key.sign(msg, {.deterministic = true});
        auto d2 = key.sign(msg, {.deterministic = true});
        check(d1 == d2, "two deterministic signatures differ");
        check(pub.verify(msg, d1), "a deterministic signature does not verify");
        auto bad = d1;
        const size_t i = size % bad.size();
        bad[i] = byte(uint8_t(bad[i]) ^ 0x10);
        check(!pub.verify(msg, bad), "a changed signature verifies");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const rooted<Keys> keys(std::in_place);   // made once, kept by a root
    if (size == 0) {
        return 0;
    }
    const uint8_t pick = data[0];
    const bool sign = (pick & 4) != 0;
    switch (pick % 3) {
        case 0: one<crypto::mldsa44::private_key>(keys->k44.pub, sign, data + 1, size - 1); break;
        case 1: one<crypto::mldsa65::private_key>(keys->k65.pub, sign, data + 1, size - 1); break;
        default: one<crypto::mldsa87::private_key>(keys->k87.pub, sign, data + 1, size - 1); break;
    }
    return 0;
}
