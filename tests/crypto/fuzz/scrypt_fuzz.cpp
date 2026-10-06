//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// scrypt on any bytes, against OpenSSL's EVP_PBE_scrypt: the input's first
// bytes choose N (2 to 1024), r (1 to 8), p (1 to 4) and the output's
// length (0 to 300, across PBKDF2's blocks of 32); the password and the
// salt are cut from the rest. A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/scrypt_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 5) {
        return 0;
    }
    uint32_t n = 2u << (data[0] % 10);
    uint32_t r = 1 + data[1] % 8;
    uint32_t p = 1 + data[2] % 4;
    size_t len = (size_t(data[3]) + (data[4] & 1) * 256) % 301;
    const uint8_t* rest = data + 5;
    size_t left = size - 5;
    size_t pw = std::min<size_t>(left, data[4] >> 1);
    static const uint8_t none = 0;
    std::vector<uint8_t> expected(len + 1);
    if (EVP_PBE_scrypt(reinterpret_cast<const char*>(pw ? rest : &none), pw, left - pw ? rest + pw : &none, left - pw, n, r, p,
                       uint64_t(1) << 32, expected.data(), len) != 1) {
        if (len == 0) {
            return 0;   // OpenSSL derives nothing of nothing
        }
        std::fprintf(stderr, "scrypt_fuzz: OpenSSL refused\n");
        std::abort();
    }
    auto got = crypto::scrypt::derive(view(rest, pw), view(rest + pw, left - pw), len,
                                      {.cost = n, .block_size = r, .parallelism = p});
    if (got.size() != len || (len && std::memcmp(got.as_slice().data(), expected.data(), len) != 0)) {
        std::fprintf(stderr, "scrypt_fuzz: differs from OpenSSL\n");
        std::abort();
    }
    return 0;
}
