//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/array.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "constant_time.h"
#include "detail/aes_core.h"
#include "detail/keys.h"
#include "error.h"

#include <cstddef>
#include <stdexcept>
#include <string>

// AES (FIPS 197), the block cipher alone: one block of 16 bytes encrypted
// or decrypted under a key of 128, 192 or 256 bits. This is the primitive a
// mode is built from, not a way to encrypt data: a block encrypted on its
// own is ECB, which shows every repeated block of the message, and nothing
// here authenticates anything. A program encrypting data takes aes_gcm (or
// chacha20_poly1305); this type is for a mode the module does not have and
// for tests.
namespace sgcl::crypto {
    class aes {
    public:
        static constexpr size_t block_size = 16;

        // The key, 16, 24 or 32 bytes (AES-128, -192, -256); any other
        // length is a broken contract, std::invalid_argument. A key read
        // from data goes through from_key instead.
        explicit aes(const slice<const byte>& key) {
            if (!detail::is_aes_key_size(key.size())) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::aes", key.size()));
            }
            _setup(key);
        }

        // The key from data: a wrong length is errc::invalid_key
        static expected<aes, error> from_key(const slice<const byte>& key) {
            if (!detail::is_aes_key_size(key.size())) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("aes", key.size()))));
            }
            return aes(key);
        }

        // Move-only: the key schedule is a secret, and a copy of a secret is
        // made on purpose, with clone(). The object moved from is zeroed
        // and holds no key.
        aes(const aes&) = delete;
        aes& operator=(const aes&) = delete;

        aes(aes&& other) noexcept
        : _enc(other._enc), _dec(other._dec), _key_size(other._key_size) {
            other._wipe();
        }

        aes& operator=(aes&& other) noexcept {
            if (this != &other) {
                _enc = other._enc;
                _dec = other._dec;
                _key_size = other._key_size;
                other._wipe();
            }
            return *this;
        }

        ~aes() {
            _wipe();
        }

        aes clone() const {
            _check();
            return aes(*this, 0);
        }

        // 16, 24 or 32
        size_t key_size() const noexcept {
            return _key_size;
        }

        array<byte, 16> encrypt_block(const array<byte, 16>& in) const {
            _check();
            array<byte, 16> out;
            detail::aes_encrypt_block(_enc, detail::bytes(in.data()), detail::bytes(out.data()));
            return out;
        }

        array<byte, 16> decrypt_block(const array<byte, 16>& in) const {
            _check();
            array<byte, 16> out;
            detail::aes_decrypt_block(_enc, _dec, detail::bytes(in.data()), detail::bytes(out.data()));
            return out;
        }

    private:
        detail::AesEncryptKey _enc;
        detail::AesDecryptKey _dec;
        size_t _key_size = 0;

        aes(const aes& other, int)
        : _enc(other._enc), _dec(other._dec), _key_size(other._key_size) {
        }

        void _setup(const slice<const byte>& key) noexcept {
            detail::aes_setup(_enc, detail::bytes(key.data()), key.size());
            detail::aes_setup_decrypt(_dec, _enc);
            _key_size = key.size();
        }

        void _check() const {
            if (_key_size == 0) {
                detail::moved_from("sgcl::crypto::aes");
            }
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_enc);
            detail::secure_zero_object(_dec);
            _key_size = 0;
        }
    };
}
