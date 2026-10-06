//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// SLH-DSA on any bytes. The first byte picks the parameter set (of the
// twelve) and what the rest is:
//   bit 7 clear: a public key, a signature and a message, cut from the rest
//     at the set's sizes (what comes from the wire): verified under the key
//     read and under a fixed key; a signature no key made does not verify,
//     nothing crashes or reads out of bounds (the digest's indices, the
//     hypertree's layers);
//   bit 7 set (SHA2-128f and SHAKE-128f alone: a signature under ASan takes
//     tens of milliseconds, a small set's seconds): three seeds and a message: the key of the seeds signs the
//     message, hedged and deterministic, both verify, the deterministic one
//     twice the same, a signature with a byte changed does not, and the
//     private key's bytes read back to the same key.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/slhdsa_fuzz.cpp 300
#include "sgcl/crypto/slhdsa.h"

#include <cstdio>
#include <cstdlib>

namespace {
    using namespace sgcl;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "slhdsa_fuzz: %s\n", what);
            std::abort();
        }
    }

    template<class Private>
    void one(bool sign, const uint8_t* data, size_t size) {
        using P = typename Private::params;
        using Public = decltype(std::declval<const Private&>().public_key());
        const auto bytes = [&](size_t at, size_t n) {
            return slice<const byte>(reinterpret_cast<const byte*>(data + at), n);
        };
        if (!sign) {
            static const uint8_t fixed_seeds[96] = {1, 2, 3};
            static const Public fixed = crypto::detail::slhdsa::Access::from_seeds<Private>(fixed_seeds).public_key();
            size_t at = 0;
            const size_t pk_n = size >= P::public_key ? P::public_key : 0;
            at += pk_n;
            const size_t sig_n = size - at >= P::signature ? P::signature : size - at;
            const auto sig = bytes(at, sig_n);
            at += sig_n;
            const auto msg = bytes(at, size - at);
            if (pk_n) {
                auto pub = Public::from_bytes(bytes(0, pk_n));
                check(bool(pub), "a public key of the right length not read");
                (void)pub->verify(msg, sig);
            }
            check(!fixed.verify(msg, sig), "a signature no key made verified");
            return;
        }
        if (size < 3 * P::n) {
            return;
        }
        auto key = crypto::detail::slhdsa::Access::from_seeds<Private>(data);
        const auto msg = bytes(3 * P::n, size - 3 * P::n < 256 ? size - 3 * P::n : 256);
        auto pub = key.public_key();
        auto hedged = key.sign(msg);
        check(pub.verify(msg, hedged), "a hedged signature does not verify");
        auto d1 = key.sign(msg, {.deterministic = true});
        auto d2 = key.sign(msg, {.deterministic = true});
        check(d1 == d2, "two deterministic signatures differ");
        check(pub.verify(msg, d1), "a deterministic signature does not verify");
        auto bad = d1;
        const size_t i = size * 131 % bad.size();
        bad[i] = byte(uint8_t(bad[i]) ^ 0x01);
        check(!pub.verify(msg, bad), "a changed signature verifies");
        auto sk = key.bytes();
        auto again = Private::from_bytes(sk);
        check(again && *again == key, "a private key's bytes do not read back to it");
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const uint8_t pick = data[0];
    const bool sign = (pick & 0x80) != 0;
    ++data;
    --size;
    namespace c = sgcl::crypto;
    if (sign) {   // the two fastest sets alone: a signature under ASan takes tens of milliseconds
        if (pick % 2 == 0) {
            one<c::slhdsa_sha2_128f::private_key>(true, data, size);
        } else {
            one<c::slhdsa_shake_128f::private_key>(true, data, size);
        }
        return 0;
    }
    switch (pick % 12) {
        case 0: one<c::slhdsa_sha2_128s::private_key>(false, data, size); break;
        case 1: one<c::slhdsa_sha2_128f::private_key>(false, data, size); break;
        case 2: one<c::slhdsa_sha2_192s::private_key>(false, data, size); break;
        case 3: one<c::slhdsa_sha2_192f::private_key>(false, data, size); break;
        case 4: one<c::slhdsa_sha2_256s::private_key>(false, data, size); break;
        case 5: one<c::slhdsa_sha2_256f::private_key>(false, data, size); break;
        case 6: one<c::slhdsa_shake_128s::private_key>(false, data, size); break;
        case 7: one<c::slhdsa_shake_128f::private_key>(false, data, size); break;
        case 8: one<c::slhdsa_shake_192s::private_key>(false, data, size); break;
        case 9: one<c::slhdsa_shake_192f::private_key>(false, data, size); break;
        case 10: one<c::slhdsa_shake_256s::private_key>(false, data, size); break;
        default: one<c::slhdsa_shake_256f::private_key>(false, data, size); break;
    }
    return 0;
}
