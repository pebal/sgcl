//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "hmac.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

// PBKDF2 (RFC 8018 §5.2), Go's crypto/pbkdf2: a key from a password, made
// slow on purpose by `iterations` rounds of HMAC per block, so that each
// guess of an attacker who has the salt and the result costs as much as
// the derivation did. It is for deriving a key from a password where a
// format or a protocol names PBKDF2 (PKCS #5, PKCS #12, WPA2, a password
// manager's vault); for storing passwords to be checked, a memory-hard
// function (Argon2id, scrypt) is the better choice, and the module will
// have one after version 1.
//
// One block of the result is F(P, S, c, i) = U1 ^ U2 ^ ... ^ Uc, with U1 =
// HMAC(P, S || i) (i as four bytes, big-endian, from 1) and Uj =
// HMAC(P, Uj-1). The HMAC's two keyed states are made once from the
// password and every Uj costs two runs of the digest's compression.
namespace sgcl::crypto {
    template<class H>
    class pbkdf2 {
        static_assert(detail::digest_type<H>, "pbkdf2<H> takes a digest of the module: sha256, sha512...");

    public:
        // n bytes derived from password and salt (bytes or text) in
        // `iterations` rounds. iterations of 0 is std::invalid_argument
        // (RFC 8018 asks for at least 1, and 600 000 of SHA-256 is what
        // OWASP names in 2023); so is n past (2^32 - 1) blocks
        static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, uint32_t iterations, size_t n) {
            _check(iterations, n);
            secret_bytes out(n);
            _derive(out.as_slice().data(), n, password, salt, iterations);
            return out;
        }

        // derive() into the caller's buffer: out.size() bytes, no
        // allocation
        static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt, uint32_t iterations) {
            _check(iterations, out.size());
            _derive(out.data(), out.size(), password, salt, iterations);
        }

    private:
        static void _check(uint32_t iterations, size_t n) {
            if (iterations == 0) {
                throw invalid_argument("sgcl::crypto::pbkdf2: no iterations");
            }
            // RFC 8018 §5.2: at most 2^32 - 1 blocks of hLen bytes
            if ((uint64_t(n) + H::digest_size - 1) / H::digest_size > 0xffffffffull) {
                throw invalid_argument("sgcl::crypto::pbkdf2: more than 2^32 - 1 blocks asked");
            }
        }

        static void _derive(byte* out, size_t n, const slice<const byte>& password, const slice<const byte>& salt,
                            uint32_t iterations) noexcept {
            constexpr size_t size = H::digest_size;
            hmac<H> mac(password);
            unsigned char u[size], t[size];
            H inner, outer;
            uint32_t block = 1;
            for (size_t done = 0; done < n; ++block) {
                unsigned char index[4] = {static_cast<unsigned char>(block >> 24), static_cast<unsigned char>(block >> 16),
                                          static_cast<unsigned char>(block >> 8), static_cast<unsigned char>(block)};
                inner = mac._start;
                inner.update(salt);
                inner.update(slice<const byte>(reinterpret_cast<const byte*>(index), 4));
                outer = mac._outer;
                hmac<H>::_tag(inner, outer, u);
                std::memcpy(t, u, size);
                for (uint32_t j = 1; j < iterations; ++j) {
                    inner = mac._start;
                    inner.update(slice<const byte>(reinterpret_cast<const byte*>(u), size));
                    outer = mac._outer;
                    hmac<H>::_tag(inner, outer, u);
                    for (size_t k = 0; k < size; ++k) {
                        t[k] ^= u[k];
                    }
                }
                size_t take = std::min(size, n - done);
                std::memcpy(out + done, t, take);
                done += take;
            }
            detail::secure_zero(u, sizeof u);
            detail::secure_zero(t, sizeof t);
            detail::secure_zero_object(inner);
            detail::secure_zero_object(outer);
        }
    };
}
