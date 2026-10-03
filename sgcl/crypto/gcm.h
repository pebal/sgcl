//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "aes.h"
#include "constant_time.h"
#include "detail/gcm_core.h"
#include "detail/keys.h"
#include "error.h"
#include "mixin/aead.h"

#include <cstddef>
#include <cstdint>

// AES-GCM (SP 800-38D), the authenticated encryption of TLS and of most of
// what is encrypted today: AES in counter mode for secrecy and GHASH, a
// polynomial MAC over GF(2^128), for integrity, with a key of 128, 192 or
// 256 bits, a nonce of 96 bits and a tag of 128. Go's cipher.NewGCM over
// aes.NewCipher.
//
// The nonce must never repeat under one key: two messages sealed with the
// same key and nonce give away the XOR of their plaintexts and, worse, the
// hash key H, after which anyone can forge. Twelve random bytes are not
// enough for many messages (the birthday bound makes a collision likely
// long before 2^48 messages; NIST allows 2^32 random nonces per key). A
// counter is the safe choice: crypto::nonce_counter. Where a counter
// cannot be kept, xchacha20_poly1305 takes 24 random bytes safely.
//
// On arm64 with the crypto extension AES runs on AESE/AESMC, eight blocks at
// a time, and GHASH on PMULL, eight blocks per reduction; elsewhere (and
// under SGCL_CRYPTO_PORTABLE) both are constant-time C++: bitsliced AES,
// GHASH on integer products with holes. See detail/aes_core.h and
// detail/ghash.h.
namespace sgcl::crypto {
    class aes_gcm
    : public mixin::aead<aes_gcm> {
        friend class mixin::aead<aes_gcm>;

    public:
        static constexpr size_t nonce_size = 12;
        static constexpr size_t tag_size = 16;
        static constexpr size_t overhead = 16;

        // SP 800-38D §5.2.1.1: at most 2^39 - 256 bits of plaintext under
        // one nonce, 2^32 - 2 blocks of the counter
        static constexpr uint64_t max_plaintext_size = (uint64_t(1) << 36) - 32;

        // The key, 16, 24 or 32 bytes; any other length is a broken
        // contract, std::invalid_argument. A key read from data goes
        // through from_key instead.
        explicit aes_gcm(const slice<const byte>& key) {
            if (!detail::is_aes_key_size(key.size())) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::aes_gcm", key.size()));
            }
            detail::gcm_setup(_key, detail::bytes(key.data()), key.size());
            _key_size = key.size();
        }

        // The key from data: a wrong length is errc::invalid_key
        static expected<aes_gcm, error> from_key(const slice<const byte>& key) noexcept {
            if (!detail::is_aes_key_size(key.size())) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("aes_gcm", key.size()))));
            }
            return aes_gcm(key);
        }

        // Move-only, clone() for a copy; the object moved from is zeroed
        // and holds no key (a call on it throws std::logic_error)
        aes_gcm(const aes_gcm&) = delete;
        aes_gcm& operator=(const aes_gcm&) = delete;

        aes_gcm(aes_gcm&& other) noexcept
        : _key(other._key), _key_size(other._key_size) {
            other._wipe();
        }

        aes_gcm& operator=(aes_gcm&& other) noexcept {
            if (this != &other) {
                _key = other._key;
                _key_size = other._key_size;
                other._wipe();
            }
            return *this;
        }

        ~aes_gcm() {
            _wipe();
        }

        aes_gcm clone() const {
            _check();
            return aes_gcm(*this, 0);
        }

        size_t key_size() const noexcept {
            return _key_size;
        }

        // seal, open, seal_to, open_to: mixin/aead.h

    private:
        static constexpr const char* _name = "sgcl::crypto::aes_gcm";

        detail::GcmKey _key;
        size_t _key_size = 0;

        aes_gcm(const aes_gcm& other, int) noexcept
        : _key(other._key), _key_size(other._key_size) {
        }

        void _check() const {
            if (_key_size == 0) {
                detail::moved_from(_name);
            }
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_key);
            _key_size = 0;
        }

        void _seal(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            detail::gcm_seal(_key, nonce, in, n, aad, aad_size, out);
        }

        bool _open(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* tag, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            return detail::gcm_open(_key, nonce, in, n, tag, aad, aad_size, out);
        }
    };
}
