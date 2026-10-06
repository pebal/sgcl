//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "constant_time.h"
#include "detail/aes_cbc.h"
#include "detail/aes_core.h"
#include "detail/keys.h"
#include "error.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

// AES in cipher block chaining mode (SP 800-38A §6.2), Go's
// cipher.NewCBCEncrypter and NewCBCDecrypter: each block encrypted after
// the ciphertext before it is XORed in, the first after the IV. With
// PKCS #7's padding (RFC 5652 §6.3: 1 to 16 bytes, each the count) for a
// whole message, or over whole blocks for a format that pads itself (7z,
// PDF, TLS 1.0 and 1.1's records).
//
// Unauthenticated: whoever can change the ciphertext changes the next
// block's plaintext bit for bit, undetected, and an answer that tells a
// bad padding from a good one decrypts the whole message (the padding
// oracle). A program encrypting data takes aes_gcm; this type is for the
// formats that name CBC, with a MAC over the ciphertext checked first
// (encrypt-then-MAC). The IV must be unpredictable for each message
// (random::bytes(16)).
namespace sgcl::crypto {
    class aes_cbc {
    public:
        static constexpr size_t block_size = 16;
        static constexpr size_t iv_size = 16;

        // The key, 16, 24 or 32 bytes, and the IV, 16 bytes; another length
        // of either is std::invalid_argument
        aes_cbc(const slice<const byte>& key, const slice<const byte>& iv) {
            if (!detail::is_aes_key_size(key.size())) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::aes_cbc", key.size()));
            }
            _check_iv(iv);
            detail::aes_setup(_key, detail::bytes(key.data()), key.size());
            detail::aes_setup_decrypt(_dec, _key);
            std::memcpy(_chain, iv.data(), 16);
            _key_size = key.size();
        }

        // The key from data: a wrong length is errc::invalid_key (the IV is
        // the program's, and a wrong one still throws)
        SGCL_INLINE_HOT static expected<aes_cbc, error> from_key(const slice<const byte>& key, const slice<const byte>& iv) {
            if (!detail::is_aes_key_size(key.size())) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("aes_cbc", key.size()))));
            }
            return aes_cbc(key, iv);
        }

        aes_cbc(const aes_cbc&) = delete;
        aes_cbc& operator=(const aes_cbc&) = delete;

        SGCL_INLINE_HOT aes_cbc(aes_cbc&& other) noexcept {
            _copy(other);
            other._wipe();
        }

        SGCL_INLINE_HOT aes_cbc& operator=(aes_cbc&& other) noexcept {
            if (this != &other) {
                _copy(other);
                other._wipe();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~aes_cbc() {
            _wipe();
        }

        // A second cipher under the same key, at the same point of its chain
        SGCL_INLINE_HOT aes_cbc clone() const {
            _check();
            aes_cbc c;
            c._copy(*this);
            return c;
        }

        SGCL_INLINE_HOT size_t key_size() const noexcept {
            return _key_size;
        }

        // A new message under the same key: the chain back to iv, 16 bytes
        SGCL_INLINE_HOT void reset(const slice<const byte>& iv) {
            _check();
            _check_iv(iv);
            std::memcpy(_chain, iv.data(), 16);
        }

        // The whole of plaintext, padded by PKCS #7 to the next whole block
        // (16 bytes of padding when it is one already), encrypted after
        // what the cipher encrypted before
        vector<byte> encrypt(const slice<const byte>& plaintext) {
            _check();
            const size_t n = plaintext.size();
            const size_t whole = n / 16 * 16;
            vector<byte> out(whole + 16);
            unsigned char* dst = detail::bytes(out.data());
            detail::aes_cbc_encrypt(_key, _chain, detail::bytes(plaintext.data()), dst, whole / 16);
            unsigned char last[16];
            const size_t rest = n - whole;
            if (rest != 0) {
                sgcl::detail::copy_bytes(last, plaintext.data() + whole, rest);
            }
            std::memset(last + rest, int(16 - rest), 16 - rest);
            detail::aes_cbc_encrypt(_key, _chain, last, dst + whole, 1);
            detail::secure_zero(last, sizeof last);
            return out;
        }

        // ciphertext decrypted and its PKCS #7 padding taken off: the
        // plaintext, or errc::malformed for a length that is not whole
        // blocks, errc::authentication for a padding that is not PKCS #7's
        // (the last block checked whole, in a time that does not depend on
        // where it is wrong). A ciphertext of a block or more; what was
        // decrypted is zeroed when the padding is refused
        [[nodiscard]] expected<vector<byte>, error> decrypt(const slice<const byte>& ciphertext) {
            _check();
            const size_t n = ciphertext.size();
            if (n == 0 || n % 16 != 0) {
                return unexpected(error(errc::malformed, "sgcl::crypto::aes_cbc: a ciphertext of no whole blocks"));
            }
            vector<byte> out(n);
            unsigned char* dst = detail::bytes(out.data());
            detail::aes_cbc_decrypt(_key, _dec, _chain, detail::bytes(ciphertext.data()), dst, n / 16);
            // the padding: 1 to 16, and the last `pad` bytes all equal to it
            const unsigned char* last = dst + n - 16;
            const uint32_t pad = last[15];
            uint32_t bad = constant_time_is_zero(pad) | (uint32_t(16 - pad) >> 31);
            for (uint32_t i = 0; i < 16; ++i) {
                // byte 15 - i belongs to the padding when i < pad
                uint32_t in_pad = uint32_t(i - pad) >> 31;
                bad |= in_pad & (constant_time_is_zero(uint32_t(last[15 - i] ^ pad)) ^ 1u);
            }
            if (bad != 0) {
                detail::secure_zero(dst, n);
                return unexpected(error(errc::authentication, "sgcl::crypto::aes_cbc: the padding is not PKCS #7's"));
            }
            out.resize(n - pad);
            return out;
        }

        // in.size() bytes, whole blocks, encrypted into out, which holds at
        // least as many and may be in itself (any other overlap is
        // std::invalid_argument), the chain carried on from call to call:
        // Go's CryptBlocks. A length that is not whole blocks is
        // std::invalid_argument; a shorter out std::length_error
        void encrypt_blocks(const slice<byte>& out, const slice<const byte>& in) {
            _check_blocks(out, in, "encrypt_blocks");
            detail::aes_cbc_encrypt(_key, _chain, detail::bytes(in.data()), detail::bytes(out.data()), in.size() / 16);
        }

        // The decryption of encrypt_blocks: eight blocks at a time on the
        // processor's AES instructions
        void decrypt_blocks(const slice<byte>& out, const slice<const byte>& in) {
            _check_blocks(out, in, "decrypt_blocks");
            detail::aes_cbc_decrypt(_key, _dec, _chain, detail::bytes(in.data()), detail::bytes(out.data()), in.size() / 16);
        }

    private:
        detail::AesEncryptKey _key;
        detail::AesDecryptKey _dec;
        unsigned char _chain[16] = {};
        size_t _key_size = 0;

        aes_cbc() = default;

        // 1 when x is zero, 0 otherwise, with no branch
        SGCL_INLINE_HOT static uint32_t constant_time_is_zero(uint32_t x) noexcept {
            return ((x | (0u - x)) >> 31) ^ 1u;
        }

        static void _check_iv(const slice<const byte>& iv) {
            if (iv.size() != iv_size) {
                throw invalid_argument("sgcl::crypto::aes_cbc: an IV of " + std::to_string(iv.size()) + " bytes, not 16");
            }
        }

        void _check_blocks(const slice<byte>& out, const slice<const byte>& in, const char* what) const {
            _check();
            if (in.size() % 16 != 0) {
                throw invalid_argument(std::string("sgcl::crypto::aes_cbc::") + what + ": an input of no whole blocks");
            }
            if (out.size() < in.size()) {
                throw length_error(std::string("sgcl::crypto::aes_cbc::") + what + ": the output is shorter than the input");
            }
            if (detail::inexact_overlap(out.data(), in.size(), in.data(), in.size())) {
                throw invalid_argument(std::string("sgcl::crypto::aes_cbc::") + what + ": the output overlaps the input other than exactly");
            }
        }

        void _copy(const aes_cbc& o) noexcept {
            _key = o._key;
            _dec = o._dec;
            std::memcpy(_chain, o._chain, sizeof _chain);
            _key_size = o._key_size;
        }

        SGCL_INLINE_HOT void _check() const {
            if (_key_size == 0) {
                detail::moved_from("sgcl::crypto::aes_cbc");
            }
        }

        SGCL_INLINE_HOT void _wipe() noexcept {
            detail::secure_zero_object(_key);
            detail::secure_zero_object(_dec);
            detail::secure_zero(_chain, sizeof _chain);
            _key_size = 0;
        }
    };
}
