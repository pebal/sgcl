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
#include "detail/aes_core.h"
#include "detail/keys.h"
#include "error.h"
#include "secret.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

// AES key wrap (RFC 3394, SP 800-38F's KW) and key wrap with padding
// (RFC 5649, KWP): a key encrypted under a key-encryption key with an
// integrity check, deterministic, for keys and nothing else — JOSE's
// A128KW and A256KW, CMS's KEK recipients, PKCS #11, the key stores of
// cloud KMSs. Six passes over the key's 64-bit halves, each step one AES
// encryption of a check register and one half, the step's number XORed
// into the register; unwrapping runs them back and checks that the
// register came out as it started: RFC 3394's A6A6A6A6A6A6A6A6, or RFC
// 5649's A65959A6 and the key's length, its padding zeros.
//
// The key-encryption key is held as an aes key is: in the object,
// move-only, zeroed when it goes. What is unwrapped is a key, and comes as
// a secret_bytes.
namespace sgcl::crypto {
    namespace detail {
        inline constexpr uint64_t kw_iv = 0xA6A6A6A6A6A6A6A6ull;
        inline constexpr uint32_t kwp_iv = 0xA65959A6u;

        // W of RFC 3394 §2.2.1 over n >= 2 halves in r (64-bit, big-endian
        // bytes), the register a; in place
        inline void kw_wrap(const AesEncryptKey& k, uint64_t& a, unsigned char* r, size_t n) noexcept {
            unsigned char b[16];
            for (uint64_t j = 0; j < 6; ++j) {
                for (size_t i = 0; i < n; ++i) {
                    store_be64(b, a);
                    std::memcpy(b + 8, r + 8 * i, 8);
                    aes_encrypt_block(k, b, b);
                    a = load_be64(b) ^ (n * j + i + 1);
                    std::memcpy(r + 8 * i, b + 8, 8);
                }
            }
            secure_zero(b, sizeof b);
        }

        // W^-1 of RFC 3394 §2.2.2
        inline void kw_unwrap(const AesEncryptKey& k, const AesDecryptKey& d, uint64_t& a, unsigned char* r, size_t n) noexcept {
            unsigned char b[16];
            for (uint64_t j = 6; j-- > 0;) {
                for (size_t i = n; i-- > 0;) {
                    store_be64(b, a ^ (n * j + i + 1));
                    std::memcpy(b + 8, r + 8 * i, 8);
                    aes_decrypt_block(k, d, b, b);
                    a = load_be64(b);
                    std::memcpy(r + 8 * i, b + 8, 8);
                }
            }
            secure_zero(b, sizeof b);
        }

        // 1 when x is zero, 0 otherwise, with no branch
        SGCL_INLINE_HOT uint64_t kw_is_zero(uint64_t x) noexcept {
            return ((x | (0 - x)) >> 63) ^ 1;
        }
    }

    class aes_kw {
    public:
        // The key-encryption key, 16, 24 or 32 bytes; another length is
        // std::invalid_argument
        SGCL_INLINE_HOT explicit aes_kw(const slice<const byte>& kek) {
            if (!detail::is_aes_key_size(kek.size())) {
                throw invalid_argument(detail::key_size_message("sgcl::crypto::aes_kw", kek.size()));
            }
            _setup(kek);
        }

        // The key from data: a wrong length is errc::invalid_key
        SGCL_INLINE_HOT static expected<aes_kw, error> from_key(const slice<const byte>& kek) noexcept {
            if (!detail::is_aes_key_size(kek.size())) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("aes_kw", kek.size()))));
            }
            return aes_kw(kek);
        }

        aes_kw(const aes_kw&) = delete;
        aes_kw& operator=(const aes_kw&) = delete;

        SGCL_INLINE_HOT aes_kw(aes_kw&& other) noexcept
        : _enc(other._enc), _dec(other._dec), _key_size(other._key_size) {
            other._wipe();
        }

        SGCL_INLINE_HOT aes_kw& operator=(aes_kw&& other) noexcept {
            if (this != &other) {
                _enc = other._enc;
                _dec = other._dec;
                _key_size = other._key_size;
                other._wipe();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~aes_kw() {
            _wipe();
        }

        SGCL_INLINE_HOT aes_kw clone() const {
            _check();
            aes_kw c;
            c._enc = _enc;
            c._dec = _dec;
            c._key_size = _key_size;
            return c;
        }

        SGCL_INLINE_HOT size_t key_size() const noexcept {
            return _key_size;
        }

        // RFC 3394: key, 16 bytes or more and a multiple of 8 (another
        // length is std::invalid_argument), wrapped into key.size() + 8 bytes
        vector<byte> wrap(const slice<const byte>& key) const {
            _check();
            if (key.size() < 16 || key.size() % 8 != 0) {
                throw invalid_argument("sgcl::crypto::aes_kw::wrap: a key of other than 8 n bytes, n >= 2 (wrap_padded takes any)");
            }
            vector<byte> out(key.size() + 8);
            unsigned char* p = detail::bytes(out.data());
            sgcl::detail::copy_bytes(p + 8, key.data(), key.size());
            uint64_t a = detail::kw_iv;
            detail::kw_wrap(_enc, a, p + 8, key.size() / 8);
            detail::store_be64(p, a);
            return out;
        }

        // The key wrap() wrapped: errc::malformed for a length that is not
        // 24 bytes or more and a multiple of 8, errc::authentication when
        // the check register does not come out as it started (another
        // key-encryption key, a changed byte)
        [[nodiscard]] expected<secret_bytes, error> unwrap(const slice<const byte>& wrapped) const {
            _check();
            const size_t m = wrapped.size();
            if (m < 24 || m % 8 != 0) {
                return unexpected(error(errc::malformed, "sgcl::crypto::aes_kw::unwrap: a length of other than 8 n bytes, n >= 3"));
            }
            secret_bytes out(m - 8);
            unsigned char* r = detail::bytes(out.as_slice().data());
            sgcl::detail::copy_bytes(r, wrapped.data() + 8, m - 8);
            uint64_t a = detail::load_be64(detail::bytes(wrapped.data()));
            detail::kw_unwrap(_enc, _dec, a, r, (m - 8) / 8);
            if (!detail::kw_is_zero(a ^ detail::kw_iv)) {
                return _refused();
            }
            return out;
        }

        // RFC 5649: a key of any length from 1 byte to 2^32 - 1 (nothing,
        // or more, is std::invalid_argument), padded with zeros to whole
        // halves and wrapped with its length in the check register
        vector<byte> wrap_padded(const slice<const byte>& key) const {
            _check();
            const size_t n = key.size();
            if (n == 0 || uint64_t(n) > 0xffffffffull) {
                throw invalid_argument("sgcl::crypto::aes_kw::wrap_padded: a key of no bytes or of 2^32 or more");
            }
            const size_t padded = (n + 7) / 8 * 8;
            vector<byte> out(padded + 8);
            unsigned char* p = detail::bytes(out.data());
            std::memset(p, 0, padded + 8);
            sgcl::detail::copy_bytes(p + 8, key.data(), n);
            uint64_t a = uint64_t(detail::kwp_iv) << 32 | uint64_t(n);
            if (padded == 8) {
                // one half: the register and it, one AES block (§4.1)
                detail::store_be64(p, a);
                detail::aes_encrypt_block(_enc, p, p);
                return out;
            }
            detail::kw_wrap(_enc, a, p + 8, padded / 8);
            detail::store_be64(p, a);
            return out;
        }

        // The key wrap_padded() wrapped: errc::malformed for a length that
        // is not 16 bytes or more and a multiple of 8, errc::authentication
        // when the register's constant, the length it holds or the padding
        // zeros are wrong (all checked, one error whichever failed)
        [[nodiscard]] expected<secret_bytes, error> unwrap_padded(const slice<const byte>& wrapped) const {
            _check();
            const size_t m = wrapped.size();
            if (m < 16 || m % 8 != 0) {
                return unexpected(error(errc::malformed, "sgcl::crypto::aes_kw::unwrap_padded: a length of other than 8 n bytes, n >= 2"));
            }
            const size_t padded = m - 8;
            secret_bytes full(padded);
            unsigned char* r = detail::bytes(full.as_slice().data());
            uint64_t a;
            if (m == 16) {
                unsigned char b[16];
                detail::aes_decrypt_block(_enc, _dec, detail::bytes(wrapped.data()), b);
                a = detail::load_be64(b);
                std::memcpy(r, b + 8, 8);
                detail::secure_zero(b, sizeof b);
            } else {
                sgcl::detail::copy_bytes(r, wrapped.data() + 8, padded);
                a = detail::load_be64(detail::bytes(wrapped.data()));
                detail::kw_unwrap(_enc, _dec, a, r, padded / 8);
            }
            // the constant, 8 (n - 1) < MLI <= 8 n, and the bytes past MLI
            // zero: every check made, the results ORed, no branch on them
            // (the values are below 2^33: a difference's sign bit is the
            // comparison)
            const uint64_t mli = a & 0xffffffffull;
            uint64_t bad = detail::kw_is_zero((a >> 32) ^ detail::kwp_iv) ^ 1;
            bad |= (uint64_t(padded) - mli) >> 63;         // MLI > 8 n
            bad |= (mli - (uint64_t(padded) - 7)) >> 63;   // MLI <= 8 (n - 1)
            uint64_t tail = 0;
            for (size_t i = padded - 8; i < padded; ++i) {
                // byte i is padding when i >= MLI
                uint64_t is_pad = ((mli - uint64_t(i) - 1) >> 63);
                tail |= (0 - is_pad) & r[i];
            }
            bad |= detail::kw_is_zero(tail) ^ 1;
            if (bad != 0) {
                return _refused();
            }
            full.resize(size_t(mli));
            return full;
        }

    private:
        detail::AesEncryptKey _enc;
        detail::AesDecryptKey _dec;
        size_t _key_size = 0;

        aes_kw() = default;

        static expected<secret_bytes, error> _refused() noexcept {
            return unexpected(error(errc::authentication, "sgcl::crypto::aes_kw: the wrapped key does not check: another key-encryption key, or changed"));
        }

        SGCL_INLINE_HOT void _setup(const slice<const byte>& kek) noexcept {
            detail::aes_setup(_enc, detail::bytes(kek.data()), kek.size());
            detail::aes_setup_decrypt(_dec, _enc);
            _key_size = kek.size();
        }

        SGCL_INLINE_HOT void _check() const {
            if (_key_size == 0) {
                detail::moved_from("sgcl::crypto::aes_kw");
            }
        }

        SGCL_INLINE_HOT void _wipe() noexcept {
            detail::secure_zero_object(_enc);
            detail::secure_zero_object(_dec);
            _key_size = 0;
        }
    };
}
