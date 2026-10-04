//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "constant_time.h"
#include "detail/aes_core.h"
#include "detail/keys.h"
#include "error.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

// AES in counter mode (SP 800-38A §6.5), the stream cipher alone: the
// encryptions of a counter block, incremented as one 128-bit big-endian
// number and wrapping at 2^128 (as Go's cipher.NewCTR and OpenSSL's
// EVP_aes_*_ctr count), XORed into the data; the same call encrypts and
// decrypts.
//
// Unauthenticated: whoever can change the ciphertext changes the plaintext
// bit for bit, undetected. A program encrypting data takes aes_gcm; this
// type is for a protocol that authenticates by other means (a MAC over the
// ciphertext, encrypt-then-MAC) and for random access into a large
// encrypted file. The key and initial counter pair must never be used for
// two messages.
namespace sgcl::crypto {
    class aes_ctr {
    public:
        static constexpr size_t block_size = 16;
        static constexpr size_t iv_size = 16;

        // The key, 16, 24 or 32 bytes, and the first counter block, 16
        // bytes; another length of either is std::invalid_argument
        aes_ctr(const slice<const byte>& key, const slice<const byte>& iv) {
            if (!detail::is_aes_key_size(key.size())) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::aes_ctr", key.size()));
            }
            if (iv.size() != iv_size) {
                throw invalid_argument("sgcl::crypto::aes_ctr: an initial counter of " + std::to_string(iv.size()) + " bytes, not 16");
            }
            detail::aes_setup(_key, detail::bytes(key.data()), key.size());
            _iv = {detail::load_be64(detail::bytes(iv.data())), detail::load_be64(detail::bytes(iv.data()) + 8)};
            _counter = _iv;
            _key_size = key.size();
        }

        // The key from data: a wrong length is errc::invalid_key (the
        // initial counter is the program's, and a wrong one still throws)
        SGCL_INLINE_HOT static expected<aes_ctr, error> from_key(const slice<const byte>& key, const slice<const byte>& iv) {
            if (!detail::is_aes_key_size(key.size())) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("aes_ctr", key.size()))));
            }
            return aes_ctr(key, iv);
        }

        aes_ctr(const aes_ctr&) = delete;
        aes_ctr& operator=(const aes_ctr&) = delete;

        SGCL_INLINE_HOT aes_ctr(aes_ctr&& other) noexcept {
            _copy(other);
            other._wipe();
        }

        SGCL_INLINE_HOT aes_ctr& operator=(aes_ctr&& other) noexcept {
            if (this != &other) {
                _copy(other);
                other._wipe();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~aes_ctr() {
            _wipe();
        }

        SGCL_INLINE_HOT aes_ctr clone() const {
            _check();
            aes_ctr c;
            c._copy(*this);
            return c;
        }

        SGCL_INLINE_HOT size_t key_size() const noexcept {
            return _key_size;
        }

        // in XORed with the keystream into out, which holds at least
        // in.size() bytes and may be in itself (any other overlap is
        // std::invalid_argument). Calls continue one another: two calls of
        // 10 and 20 bytes are one of 30.
        void xor_key_stream(const slice<byte>& out, const slice<const byte>& in) {
            _check();
            const size_t n = in.size();
            if (out.size() < n) {
                throw length_error("sgcl::crypto::aes_ctr::xor_key_stream: the output is shorter than the input");
            }
            if (detail::inexact_overlap(out.data(), n, in.data(), n)) {
                throw invalid_argument("sgcl::crypto::aes_ctr::xor_key_stream: the output overlaps the input other than exactly");
            }
            const unsigned char* src = detail::bytes(in.data());
            unsigned char* dst = detail::bytes(out.data());
            size_t left = n;
            while (left > 0 && _used < 16) {
                *dst++ = (unsigned char)(*src++ ^ _stream[_used++]);
                --left;
            }
            const size_t blocks = left / 16;
            detail::aes_ctr_blocks<false>(_key, _counter, src, dst, blocks);
            src += blocks * 16;
            dst += blocks * 16;
            left -= blocks * 16;
            if (left > 0) {
                std::memset(_stream, 0, sizeof _stream);
                detail::aes_ctr_blocks<false>(_key, _counter, _stream, _stream, 1);
                _used = 0;
                while (left > 0) {
                    *dst++ = (unsigned char)(*src++ ^ _stream[_used++]);
                    --left;
                }
            }
        }

        // The keystream from the start of block `block` on: the initial
        // counter plus block, as a 128-bit number
        SGCL_INLINE_HOT void seek(uint64_t block) {
            _check();
            _counter = detail::counter_add<false>(_iv, block);
            _used = 16;
            detail::secure_zero(_stream, sizeof _stream);
        }

    private:
        detail::AesEncryptKey _key;
        detail::Counter _iv = {0, 0};
        detail::Counter _counter = {0, 0};  // the next block to make
        unsigned char _stream[16] = {};     // the keystream of the last block made
        size_t _used = 16;                  // its bytes used
        size_t _key_size = 0;

        aes_ctr() = default;

        void _copy(const aes_ctr& o) noexcept {
            _key = o._key;
            _iv = o._iv;
            _counter = o._counter;
            std::memcpy(_stream, o._stream, sizeof _stream);
            _used = o._used;
            _key_size = o._key_size;
        }

        SGCL_INLINE_HOT void _check() const {
            if (_key_size == 0) {
                detail::moved_from("sgcl::crypto::aes_ctr");
            }
        }

        SGCL_INLINE_HOT void _wipe() noexcept {
            detail::secure_zero_object(_key);
            detail::secure_zero(_stream, sizeof _stream);
            _counter = _iv = {0, 0};
            _used = 16;
            _key_size = 0;
        }
    };
}
