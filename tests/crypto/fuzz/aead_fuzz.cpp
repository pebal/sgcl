//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The AEADs' open on any bytes, and seal and open on any plaintext. The first
// byte picks the cipher (AES-128/192/256-GCM, ChaCha20-Poly1305,
// XChaCha20-Poly1305) and the length of the additional data; the key and the
// nonce come from the next bytes, the rest is the data. What must hold:
//
// - the data as sealed bytes does not open (a forgery would need the tag of
//   a key the input does not know; a chance of 2^-128), and the bytes open_to
//   would have written are zero after it, through a buffer and in place;
// - the data as plaintext seals and opens back to itself, in place too;
// - the sealed data with one bit changed (the bit chosen by the input) does
//   not open;
// - open_random of xchacha20_poly1305 on the data fails.
//
// Built with the driver of tests/fuzz/driver.cpp under ASan and UBSan
// (its header has the command), or with -fsanitize=fuzzer where libFuzzer is.
#include "sgcl/crypto/crypto.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;
    using bytes = std::vector<std::byte>;

    void check(bool ok) {
        if (!ok) {
            std::abort();
        }
    }

    bool all_zero(const std::byte* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != std::byte(0)) {
                return false;
            }
        }
        return true;
    }

    template<class Aead>
    void run(const bytes& key, const bytes& nonce, const bytes& aad, const bytes& data, size_t flip) {
        Aead a(key);
        // the data as a sealed message
        auto opened = a.open(nonce, data, aad);
        check(!opened.has_value());
        if (data.size() >= Aead::tag_size) {
            const size_t n = data.size() - Aead::tag_size;
            bytes out(n + 3, std::byte(0x5a));
            auto r = a.open_to(out, nonce, data, aad);
            check(!r.has_value() && r.error().code() == crypto::errc::authentication);
            check(all_zero(out.data(), n));
            check(out[n] == std::byte(0x5a));
            bytes in_place = data;
            check(!a.open_to(in_place, nonce, in_place, aad).has_value());
            check(all_zero(in_place.data(), n));
        }
        // the data as a plaintext
        auto sealed = a.seal(nonce, data, aad);
        check(sealed.size() == data.size() + Aead::tag_size);
        auto back = a.open(nonce, sealed, aad);
        check(back.has_value() && back->size() == data.size() && (data.empty() || std::memcmp(back->data(), data.data(), data.size()) == 0));
        bytes buffer = data;
        buffer.resize(data.size() + Aead::tag_size);
        check(a.seal_to(buffer, nonce, slice<const std::byte>(buffer.data(), data.size()), aad) == buffer.size());
        check(std::memcmp(buffer.data(), sealed.data(), sealed.size()) == 0);
        auto m = a.open_to(buffer, nonce, buffer, aad);
        check(m.has_value() && *m == data.size() && (data.empty() || std::memcmp(buffer.data(), data.data(), data.size()) == 0));
        // one bit changed
        bytes damaged(sealed.begin(), sealed.end());
        const size_t bit = flip % (damaged.size() * 8);
        damaged[bit / 8] ^= std::byte(1u << (bit % 8));
        check(!a.open(nonce, damaged, aad).has_value());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* p, size_t size) {
    if (size < 1 + 32 + 24 + 2) {
        return 0;
    }
    const unsigned which = p[0] % 5;
    const size_t aad_size = p[0] / 5 % 40;
    const size_t flip = size_t(p[1]) << 8 | p[2];
    p += 3;
    size_t left = size - 3;
    bytes key(reinterpret_cast<const std::byte*>(p), reinterpret_cast<const std::byte*>(p) + 32);
    p += 32;
    left -= 32;
    bytes nonce(reinterpret_cast<const std::byte*>(p), reinterpret_cast<const std::byte*>(p) + 24);
    p += 24;
    left -= 24;
    const size_t a = aad_size < left ? aad_size : left;
    bytes aad(reinterpret_cast<const std::byte*>(p), reinterpret_cast<const std::byte*>(p) + a);
    p += a;
    left -= a;
    bytes data(reinterpret_cast<const std::byte*>(p), reinterpret_cast<const std::byte*>(p) + left);
    switch (which) {
        case 0: key.resize(16); nonce.resize(12); run<crypto::aes_gcm>(key, nonce, aad, data, flip); break;
        case 1: key.resize(24); nonce.resize(12); run<crypto::aes_gcm>(key, nonce, aad, data, flip); break;
        case 2: nonce.resize(12); run<crypto::aes_gcm>(key, nonce, aad, data, flip); break;
        case 3: nonce.resize(12); run<crypto::chacha20_poly1305>(key, nonce, aad, data, flip); break;
        default: {
            run<crypto::xchacha20_poly1305>(key, nonce, aad, data, flip);
            crypto::xchacha20_poly1305 x(key);
            check(!x.open_random(data, aad).has_value());
            break;
        }
    }
    return 0;
}
