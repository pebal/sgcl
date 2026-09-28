//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "hmac.h"
#include "secure_zero.h"
#include "sha256.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

// HKDF (RFC 5869), Go's crypto/hkdf: keys made from a secret that is not
// yet one — the output of a key exchange, a password already stretched, a
// master key — in two steps. extract() concentrates the input's entropy
// into a pseudorandom key (PRK) of the digest's size, under a salt;
// expand() makes from a PRK as many bytes as asked, bound to an `info`
// that names what they are for, so that one PRK gives independent keys
// for independent uses. derive() is both at once.
//
// The PRK is a secret, held by hkdf<H>::prk: its bytes in the object,
// no copy (clone() when one is meant), zeroed when it dies and when moved
// from. What expand() gives is a vector<byte>, a managed buffer that stays
// in memory until the collector reuses it; expand_to() writes into a
// buffer of the caller's (a stack array, a key's own storage) for keys
// that must not linger.
namespace sgcl::crypto {
    template<class H>
    requires detail::digest_type<H>
    class hkdf {
    public:
        // The most expand() makes from one PRK: 255 blocks of the digest
        static constexpr size_t max_size = 255 * H::digest_size;

        // A pseudorandom key: extract()'s result, expand()'s key
        class prk {
            friend class hkdf;

        public:
            static constexpr size_t size = H::digest_size;

            prk(const prk&) = delete;
            prk& operator=(const prk&) = delete;

            prk(prk&& other) noexcept {
                std::memcpy(_key, other._key, size);
                other._wipe();
            }

            prk& operator=(prk&& other) noexcept {
                if (this != &other) {
                    std::memcpy(_key, other._key, size);
                    other._wipe();
                }
                return *this;
            }

            ~prk() {
                _wipe();
            }

            prk clone() const noexcept {
                prk k;
                std::memcpy(k._key, _key, size);
                return k;
            }

            // The key's bytes, where they lie in this object: valid while
            // it lives, for a protocol that writes the PRK down or feeds it
            // on (TLS 1.3 takes one secret from another)
            slice<const byte> bytes() const noexcept {
                return slice<const byte>(_key, size);
            }

        private:
            byte _key[size];

            prk() noexcept = default;

            void _wipe() noexcept {
                detail::secure_zero(_key, size);
            }
        };

        // The PRK of input keying material ikm under salt: HMAC(salt, ikm).
        // An empty salt is the digest's size of zeros (RFC 5869 §2.2),
        // which HMAC's padding of the key makes the same thing
        static prk extract(const slice<const byte>& salt, const slice<const byte>& ikm) noexcept {
            hmac<H> mac(salt);
            mac.update(ikm);
            prk k;
            array<byte, H::digest_size> t = mac.value();
            std::memcpy(k._key, t.data(), prk::size);
            detail::secure_zero(t.data(), t.size());
            return k;
        }

        // n bytes of output keying material from the PRK, bound to info:
        // T(1) || T(2) || ... cut to n, T(i) = HMAC(PRK, T(i-1) || info ||
        // i). n up to max_size, past it std::invalid_argument
        static vector<byte> expand(const prk& key, const slice<const byte>& info, size_t n) {
            vector<byte> out(_checked(n));
            _expand(out.data(), n, key.bytes(), info);
            return out;
        }

        // The same from a PRK given as bytes (a secret a protocol computed
        // otherwise): RFC 5869 asks for at least the digest's size, which
        // is the caller's to keep
        static vector<byte> expand(const slice<const byte>& key, const slice<const byte>& info, size_t n) {
            vector<byte> out(_checked(n));
            _expand(out.data(), n, key, info);
            return out;
        }

        // expand() into the caller's buffer: out.size() bytes, no
        // allocation
        static void expand_to(const slice<byte>& out, const prk& key, const slice<const byte>& info) {
            _expand(out.data(), _checked(out.size()), key.bytes(), info);
        }

        static void expand_to(const slice<byte>& out, const slice<const byte>& key, const slice<const byte>& info) {
            _expand(out.data(), _checked(out.size()), key, info);
        }

        // extract() and expand() in one: n bytes from ikm under salt and info
        static vector<byte> derive(const slice<const byte>& salt, const slice<const byte>& ikm, const slice<const byte>& info, size_t n) {
            _checked(n);
            prk k = extract(salt, ikm);
            return expand(k, info, n);
        }

        static void derive_to(const slice<byte>& out, const slice<const byte>& salt, const slice<const byte>& ikm, const slice<const byte>& info) {
            _checked(out.size());
            prk k = extract(salt, ikm);
            expand_to(out, k, info);
        }

    private:
        static size_t _checked(size_t n) {
            if (n > max_size) {
                throw invalid_argument("sgcl::crypto::hkdf: more than 255 blocks asked of expand");
            }
            return n;
        }

        static void _expand(byte* out, size_t n, const slice<const byte>& key, const slice<const byte>& info) noexcept {
            hmac<H> mac(key);
            array<byte, H::digest_size> t;
            unsigned char counter = 1;
            for (size_t done = 0; done < n; ++counter) {
                if (counter > 1) {
                    mac.reset();
                    mac.update(t);
                }
                mac.update(info);
                mac.update(slice<const byte>(reinterpret_cast<const byte*>(&counter), 1));
                t = mac.value();
                size_t take = std::min(H::digest_size, n - done);
                std::memcpy(out + done, t.data(), take);
                done += take;
            }
            detail::secure_zero(t.data(), t.size());
        }
    };

    using hkdf_sha256 = hkdf<sha256>;
}
