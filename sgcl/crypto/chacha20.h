//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "constant_time.h"
#include "detail/chacha_core.h"
#include "detail/keys.h"
#include "error.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

// ChaCha20 (RFC 8439 §2.4), the stream cipher alone: a keystream XORed into
// the data, the same call encrypting and decrypting. Go's
// chacha20.NewUnauthenticatedCipher. A 12-byte nonce is RFC 8439's cipher,
// a 24-byte one XChaCha20 (the key and the first 16 bytes of the nonce
// through HChaCha20 to a new key, the last 8 bytes the nonce).
//
// Unauthenticated: whoever can change the ciphertext changes the plaintext
// bit for bit, undetected. A program encrypting data takes
// chacha20_poly1305; this type is for a protocol that authenticates by
// other means and for tests. The key and nonce pair must never be used for
// two messages.
namespace sgcl::crypto {
    class chacha20 {
    public:
        static constexpr size_t key_size = 32;
        static constexpr size_t nonce_size = 12;
        static constexpr size_t x_nonce_size = 24;
        static constexpr size_t block_size = 64;

        // The key, 32 bytes, and the nonce, 12 bytes or 24 (XChaCha20);
        // another length of either is std::invalid_argument. The keystream
        // starts at block 0.
        chacha20(const slice<const byte>& key, const slice<const byte>& nonce) {
            if (key.size() != key_size) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::chacha20", key.size()));
            }
            _setup(key, nonce);
        }

        // The key from data: a wrong length is errc::invalid_key (the nonce
        // is the program's, and a wrong one still throws)
        static expected<chacha20, error> from_key(const slice<const byte>& key, const slice<const byte>& nonce) {
            if (key.size() != key_size) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("chacha20", key.size()))));
            }
            return chacha20(key, nonce);
        }

        chacha20(const chacha20&) = delete;
        chacha20& operator=(const chacha20&) = delete;

        chacha20(chacha20&& other) noexcept {
            _take(other);
        }

        chacha20& operator=(chacha20&& other) noexcept {
            if (this != &other) {
                _take(other);
            }
            return *this;
        }

        ~chacha20() {
            _wipe();
        }

        chacha20 clone() const {
            _check();
            chacha20 c;
            c._copy(*this);
            return c;
        }

        // in XORed with the keystream into out, which holds at least
        // in.size() bytes and may be in itself (any other overlap is
        // std::invalid_argument). Calls continue one another: two calls of
        // 10 and 20 bytes are one of 30. The keystream is 2^32 blocks
        // (256 GiB) long; a call past its end is std::length_error.
        void xor_key_stream(const slice<byte>& out, const slice<const byte>& in) {
            _check();
            const size_t n = in.size();
            if (out.size() < n) {
                throw length_error("sgcl::crypto::chacha20::xor_key_stream: the output is shorter than the input");
            }
            if (detail::inexact_overlap(out.data(), n, in.data(), n)) {
                throw invalid_argument("sgcl::crypto::chacha20::xor_key_stream: the output overlaps the input other than exactly");
            }
            const unsigned char* src = detail::bytes(in.data());
            unsigned char* dst = detail::bytes(out.data());
            size_t left = n;
            const size_t have = 64 - _used;
            if (left > have) {
                const uint64_t blocks = (left - have + 63) / 64;
                if (_counter + blocks > (uint64_t(1) << 32)) {
                    throw length_error("sgcl::crypto::chacha20::xor_key_stream: past the end of the keystream (2^32 blocks)");
                }
            }
            while (left > 0 && _used < 64) {
                *dst++ = (unsigned char)(*src++ ^ _stream[_used++]);
                --left;
            }
            const size_t blocks = left / 64;
            detail::chacha_xor_blocks(_state, uint32_t(_counter), src, dst, blocks);
            _counter += blocks;
            src += blocks * 64;
            dst += blocks * 64;
            left -= blocks * 64;
            if (left > 0) {
                detail::chacha_block(_state, uint32_t(_counter), _stream);
                ++_counter;
                _used = 0;
                while (left > 0) {
                    *dst++ = (unsigned char)(*src++ ^ _stream[_used++]);
                    --left;
                }
            }
        }

        // The keystream from the start of block `counter` on (Go's
        // SetCounter, without its refusal to go back: decrypting from the
        // middle is what a seek is for)
        void seek(uint32_t counter) {
            _check();
            _counter = counter;
            _used = 64;
            detail::secure_zero(_stream, sizeof _stream);
        }

    private:
        detail::ChachaState _state = {};
        uint64_t _counter = 0;           // the next block to make
        unsigned char _stream[64] = {};  // the keystream of the last block made
        size_t _used = 64;               // its bytes used
        bool _keyed = false;

        chacha20() = default;

        void _setup(const slice<const byte>& key, const slice<const byte>& nonce) {
            if (nonce.size() == nonce_size) {
                detail::chacha_load(_state, detail::bytes(key.data()), detail::bytes(nonce.data()));
            } else if (nonce.size() == x_nonce_size) {
                uint32_t sub[8];
                detail::hchacha20(detail::bytes(key.data()), detail::bytes(nonce.data()), sub);
                for (int i = 0; i < 8; ++i) {
                    _state.key[i] = sub[i];
                }
                detail::secure_zero(sub, sizeof sub);
                _state.nonce[0] = 0;
                _state.nonce[1] = detail::load_le32(detail::bytes(nonce.data()) + 16);
                _state.nonce[2] = detail::load_le32(detail::bytes(nonce.data()) + 20);
            } else {
                throw invalid_argument("sgcl::crypto::chacha20: a nonce of " + std::to_string(nonce.size()) + " bytes, not 12 or 24");
            }
            _keyed = true;
        }

        void _copy(const chacha20& o) noexcept {
            _state = o._state;
            _counter = o._counter;
            std::memcpy(_stream, o._stream, sizeof _stream);
            _used = o._used;
            _keyed = o._keyed;
        }

        void _take(chacha20& o) noexcept {
            _copy(o);
            o._wipe();
        }

        void _check() const {
            if (!_keyed) {
                detail::moved_from("sgcl::crypto::chacha20");
            }
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_state);
            detail::secure_zero(_stream, sizeof _stream);
            _counter = 0;
            _used = 64;
            _keyed = false;
        }
    };
}
