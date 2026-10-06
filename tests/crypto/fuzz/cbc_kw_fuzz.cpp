//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// AES-CBC and AES key wrap on any bytes, against OpenSSL. The input's first
// byte chooses the key's length and the operation, the next 32 the key and
// the IV; the rest is the data. CBC: the data encrypted with PKCS #7 padding
// (in one call, and as whole blocks in pieces the input chooses) must be
// OpenSSL's, decrypt must give it back, and the data decrypted as a
// ciphertext must be refused or accepted as OpenSSL refuses or accepts it,
// with the same plaintext. Key wrap: the data wrapped (padded, and plain when
// its length allows) must be OpenSSL's and unwrap back; the data unwrapped as
// a wrapped key must be refused or accepted as OpenSSL's, with the same key.
// A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/openssl@3/include /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib" \
//       sh tests/fuzz/run.sh tests/crypto/fuzz/cbc_kw_fuzz.cpp 300
#include "sgcl/crypto/crypto.h"

#include <openssl/evp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
    using namespace sgcl;
    using bytes = std::vector<uint8_t>;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "cbc_kw_fuzz: %s\n", what);
            std::abort();
        }
    }

    // OpenSSL's answer, or nothing when it refuses
    bool ossl(const EVP_CIPHER* c, const uint8_t* key, const uint8_t* iv, const uint8_t* in, size_t n, bool encrypt, bool pad, bytes& out) {
        static const uint8_t none = 0;
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
        out.assign(n + 32, 0);
        int a = 0, b = 0;
        bool ok = EVP_CipherInit_ex(ctx, c, nullptr, key, iv, encrypt ? 1 : 0) == 1 && EVP_CIPHER_CTX_set_padding(ctx, pad ? 1 : 0) == 1
               && EVP_CipherUpdate(ctx, out.data(), &a, n ? in : &none, int(n)) == 1 && EVP_CipherFinal_ex(ctx, out.data() + a, &b) == 1;
        EVP_CIPHER_CTX_free(ctx);
        out.resize(ok ? size_t(a + b) : 0);
        return ok;
    }

    bool same(const vector<byte>& v, const bytes& b) {
        return v.size() == b.size() && (b.empty() || std::memcmp(v.data(), b.data(), b.size()) == 0);
    }

    bool same(const crypto::secret_bytes& v, const bytes& b) {
        return v.size() == b.size() && (b.empty() || std::memcmp(v.as_slice().data(), b.data(), b.size()) == 0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 33) {
        return 0;
    }
    const size_t ks = 16 + 8 * (data[0] % 3);
    const bool wrap = (data[0] >> 2) & 1;
    const uint8_t* key = data + 1;
    const uint8_t* iv = data + 17;
    const uint8_t* p = data + 33;
    const size_t n = size - 33;
    if (!wrap) {
        const EVP_CIPHER* c = ks == 16 ? EVP_aes_128_cbc() : ks == 24 ? EVP_aes_192_cbc() : EVP_aes_256_cbc();
        bytes expected;
        check(ossl(c, key, iv, p, n, true, true, expected), "OpenSSL refused to encrypt");
        crypto::aes_cbc enc(view(key, ks), view(iv, 16));
        vector<byte> ct = enc.encrypt(view(p, n));
        check(same(ct, expected), "the padded encryption");
        crypto::aes_cbc dec(view(key, ks), view(iv, 16));
        auto back = dec.decrypt(slice<const byte>(ct.data(), ct.size()));
        check(back && back->size() == n && (n == 0 || std::memcmp(back->data(), p, n) == 0), "the round trip");
        // whole blocks in pieces
        size_t whole = n / 16 * 16;
        bytes raw;
        check(ossl(c, key, iv, p, whole, true, false, raw), "OpenSSL refused blocks");
        bytes mine(p, p + whole);
        crypto::aes_cbc pieces(view(key, ks), view(iv, 16));
        for (size_t at = 0, k = 0; at < whole; ++k) {
            size_t take = std::min(whole - at, size_t(16) * (1 + data[(k % 16) + 1] % 9));
            pieces.encrypt_blocks(slice<byte>(reinterpret_cast<byte*>(mine.data() + at), take), view(mine.data() + at, take));
            at += take;
        }
        check(mine == raw, "the blocks in pieces");
        // the data as a ciphertext
        bytes plain;
        bool ok = n != 0 && n % 16 == 0 && ossl(c, key, iv, p, n, false, true, plain);
        crypto::aes_cbc any(view(key, ks), view(iv, 16));
        auto got = any.decrypt(view(p, n));
        check(bool(got) == ok, "a ciphertext accepted or refused otherwise");
        if (ok) {
            check(same(*got, plain), "a ciphertext decrypted otherwise");
        }
        return 0;
    }
    crypto::aes_kw kek(view(key, ks));
    const EVP_CIPHER* w = ks == 16 ? EVP_aes_128_wrap() : ks == 24 ? EVP_aes_192_wrap() : EVP_aes_256_wrap();
    const EVP_CIPHER* wp = ks == 16 ? EVP_aes_128_wrap_pad() : ks == 24 ? EVP_aes_192_wrap_pad() : EVP_aes_256_wrap_pad();
    bytes expected;
    if (n >= 1) {
        check(ossl(wp, key, nullptr, p, n, true, true, expected), "OpenSSL refused to wrap with padding");
        vector<byte> mine = kek.wrap_padded(view(p, n));
        check(same(mine, expected), "wrap_padded");
        auto back = kek.unwrap_padded(slice<const byte>(mine.data(), mine.size()));
        check(back && back->size() == n && std::memcmp(back->as_slice().data(), p, n) == 0, "the padded round trip");
    }
    if (n >= 16 && n % 8 == 0) {
        check(ossl(w, key, nullptr, p, n, true, true, expected), "OpenSSL refused to wrap");
        vector<byte> mine = kek.wrap(view(p, n));
        check(same(mine, expected), "wrap");
    }
    // the data as a wrapped key, both ways
    bytes unwrapped;
    bool ok = n >= 24 && n % 8 == 0 && ossl(w, key, nullptr, p, n, false, true, unwrapped);
    auto got = kek.unwrap(view(p, n));
    check(bool(got) == ok, "a wrapped key accepted or refused otherwise");
    if (ok) {
        check(same(*got, unwrapped), "a wrapped key unwrapped otherwise");
    }
    ok = n >= 16 && n % 8 == 0 && ossl(wp, key, nullptr, p, n, false, true, unwrapped);
    auto gotp = kek.unwrap_padded(view(p, n));
    check(bool(gotp) == ok, "a padded wrapped key accepted or refused otherwise");
    if (ok) {
        check(same(*gotp, unwrapped), "a padded wrapped key unwrapped otherwise");
    }
    return 0;
}
