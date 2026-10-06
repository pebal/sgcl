//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PKCS #12 on any bytes. The input is read as a file under the password
// "pw" (the seeds are files under it, OpenSSL's of every kind of key and the
// module's), the derivations capped at 4096 iterations so that a mutated
// count does not stall the run: nothing crashes or reads out of bounds, a
// file read has a key that signs for its leaf or no key, and a file without
// a valid MAC under "pw" is refused. Then the input as a password: a file of
// a fixed key and certificate written under it reads back under it alone.
//
//   tests/fuzz/run.sh tests/crypto/fuzz/pkcs12_fuzz.cpp 300
#include "sgcl/core/rooted.h"
#include "sgcl/crypto/pkcs12.h"

#include <cstdio>
#include <cstdlib>

namespace {
    using namespace sgcl;
    namespace x509 = sgcl::crypto::x509;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "pkcs12_fuzz: %s\n", what);
            std::abort();
        }
    }

    struct Fixed {
        crypto::ed25519::private_key key = crypto::ed25519::private_key::from_seed(slice<const byte>("a fixed seed of 32 bytes .......")).value();
        x509::chain chain;

        Fixed() {
            x509::certificate_template t;
            t.common_name = "fuzz";
            chain.push_back(x509::create_certificate(t, key));
        }
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const rooted<Fixed> fixed(std::in_place);
    const slice<const byte> input(reinterpret_cast<const byte*>(data), size);
    auto p = crypto::pkcs12::parse(input, "pw", {.max_iterations = 4096});
    if (p) {
        (void)p->friendly_name();
        if (p->key_kind() != x509::key_kind::none) {
            auto der = p->signing_key().public_key_der();
            check(!der.empty(), "a key without a public half");
            check(p->key_pkcs8().size() > 0, "a key without its PKCS #8");
        }
    }
    if (size <= 64) {
        auto file = crypto::pkcs12::encode(fixed->key, fixed->chain, input, {.iterations = 1});
        auto back = crypto::pkcs12::parse(file, input);
        check(bool(back), "a file written under a password does not read under it");
        check(back->key_kind() == x509::key_kind::ed25519 && back->certificates().size() == 1, "a file reads to other contents");
        const char other[] = "another password";
        if (size != sizeof other - 1 || std::memcmp(data, other, size) != 0) {
            check(!crypto::pkcs12::parse(file, "another password"), "a file reads under another password");
        }
    }
    return 0;
}
