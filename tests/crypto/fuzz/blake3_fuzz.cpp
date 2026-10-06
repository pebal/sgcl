//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// BLAKE3 on any bytes, against its authors' C library: the input's first
// bytes choose the mode (hash, keyed_hash with a key from the input,
// derive_key with a context from it), the piece sizes of the updates (up
// to 64 KiB, so that whole subtrees go through the four-lane path), how
// many times the input is repeated (to reach several subtrees from a short
// input) and where the output is read from and how much of it; the rest is
// the message. The output, read from that position, must be the library's
// finalize_seek; so must a copy's after a reset and the same input again.
// A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/blake3/include /opt/homebrew/opt/blake3/lib/libblake3.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/blake3_fuzz.cpp 300
#include "sgcl/crypto/blake3.h"

#include <blake3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "blake3_fuzz: %s differs from the C library\n", what);
            std::abort();
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 12) {
        return 0;
    }
    int mode = data[0] % 3;
    size_t piece[3] = {1 + size_t(data[1]) * 7, 1 + size_t(data[2]) * 257, 1 + size_t(data[3])};
    size_t repeat = 1 + data[4] % 80;
    uint64_t position = uint64_t(data[5]) << (data[6] % 40) | data[7];
    size_t out_len = size_t(data[8]) << 2 | data[9] >> 6;
    const uint8_t* p = data + 12;
    size_t n = size - 12;
    std::vector<uint8_t> message;
    for (size_t i = 0; i < repeat; ++i) {
        message.insert(message.end(), p, p + n);
    }
    uint8_t key[32] = {};
    std::memcpy(key, p, std::min<size_t>(n, 32));
    std::string context(reinterpret_cast<const char*>(p), std::min<size_t>(n, 20));

    blake3_hasher ref;
    crypto::blake3 h = mode == 0 ? crypto::blake3() : mode == 1 ? crypto::blake3(view(key, 32)) : crypto::blake3::for_derive_key(view(reinterpret_cast<const uint8_t*>(context.data()), context.size()));
    if (mode == 0) {
        blake3_hasher_init(&ref);
    } else if (mode == 1) {
        blake3_hasher_init_keyed(&ref, key);
    } else {
        blake3_hasher_init_derive_key_raw(&ref, context.data(), context.size());
    }
    blake3_hasher_update(&ref, message.data(), message.size());
    size_t at = 0;
    for (int k = 0; at < message.size(); ++k) {
        size_t take = std::min(piece[k % 3], message.size() - at);
        h.update(view(message.data() + at, take));
        at += take;
    }
    std::vector<uint8_t> expected(out_len), got(out_len);
    blake3_hasher_finalize_seek(&ref, position, expected.data(), out_len);
    h.value_to(slice<byte>(reinterpret_cast<byte*>(got.data()), out_len), position);
    check(got == expected, "the output");
    crypto::blake3 copy = h;
    copy.reset();
    copy.update(view(message.data(), message.size()));
    std::fill(got.begin(), got.end(), 0);
    copy.value_to(slice<byte>(reinterpret_cast<byte*>(got.data()), out_len), position);
    check(got == expected, "the output after a reset");
    return 0;
}
