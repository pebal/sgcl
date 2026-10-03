//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/bytes.h"
#include "secure_zero.h"
#include "sha256.h"
#include "sha512.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/detail/bytes.h"
#include "../core/slice.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>
#include <cstring>

// HMAC (FIPS 198-1, RFC 2104), Go's crypto/hmac: a tag over a message under
// a secret key, over any digest of the module — hmac<sha256>, hmac<sha3_256>
// — the digest a parameter of the template because it is known in the
// code, as Go's hmac.New(sha256.New, key) takes it.
//
// The key is not kept: what is kept is the digest's state after the key
// XOR ipad (where every message starts, and where reset() goes back) and
// after the key XOR opad, the two states the key's block leaves, plus the
// running inner state. They are the key's equivalent, so an hmac is a
// secret: it cannot be copied (clone() says so when a copy is meant), a
// move zeroes the object moved from, and the destructor zeroes all three
// states. Otherwise it is a hasher in the hash module's shape
// (hash::mixin::hasher): update() takes bytes and text, value() is the tag
// and the hmac goes on, copy_from() reads a stream into it.
namespace sgcl::crypto {
    template<class H>
    requires detail::digest_type<H>
    class pbkdf2;

    template<class H>
    requires detail::digest_type<H>
    class hmac : public hash::mixin::hasher<hmac<H>> {
        template<class D>
        requires detail::digest_type<D>
        friend class pbkdf2;

    public:
        using hash::mixin::hasher<hmac<H>>::update;

        static constexpr size_t digest_size = H::digest_size;
        static constexpr size_t block_size = H::block_size;

        // A key of any length, as bytes or text: one longer than the
        // digest's block is hashed first, a shorter one padded with zeros
        // (RFC 2104 §2). An empty key is allowed, as the standard has it,
        // and is no secret
        explicit hmac(const slice<const byte>& key) noexcept {
            _init(key);
        }

        hmac(const hmac&) = delete;
        hmac& operator=(const hmac&) = delete;

        // The states taken over; the object moved from is zeroed and gives
        // no tag of any use until assigned again
        hmac(hmac&& other) noexcept
        : _start(other._start), _inner(other._inner), _outer(other._outer) {
            other._wipe();
        }

        hmac& operator=(hmac&& other) noexcept {
            if (this != &other) {
                _start = other._start;
                _inner = other._inner;
                _outer = other._outer;
                other._wipe();
            }
            return *this;
        }

        ~hmac() {
            _wipe();
        }

        // A second hmac under the same key, at the same point of its
        // message: the copy a hasher makes by value, asked for by name here
        hmac clone() const noexcept {
            return hmac(*this, Clone());
        }

        void update(const slice<const byte>& data) noexcept {
            _inner.update(data);
        }

        // The tag of the message so far; the hmac goes on
        array<byte, digest_size> value() const noexcept {
            array<byte, digest_size> out;
            H inner = _inner;
            H outer = _outer;
            _tag(inner, outer, detail::bytes(out.data()));
            detail::secure_zero_object(inner);
            detail::secure_zero_object(outer);
            return out;
        }

        array<byte, digest_size> digest() const noexcept {
            return value();
        }

        // As just made with the key: the message dropped
        void reset() noexcept {
            _inner = _start;
        }

        // Whether tag is the tag of the message so far, compared in
        // constant time (constant_time::equal): what a received tag is
        // checked with, never ==. A tag of another length is false.
        // [[nodiscard]]: a check whose result is dropped was never made
        [[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept {
            array<byte, digest_size> mine = value();
            bool ok = constant_time::equal(mine, tag);
            detail::secure_zero(mine.data(), mine.size());
            return ok;
        }

        // The tag of data under key in one call: the data first and the key
        // after it, as every keyed type of the hash module has it
        // (siphash::of(data, key)); the constructor takes the key alone
        static array<byte, digest_size> of(const slice<const byte>& data, const slice<const byte>& key) noexcept {
            hmac mac(key);
            mac.update(data);
            return mac.value();
        }

    private:
        struct Clone {};

        H _start;   // the digest after the key XOR ipad: where a message starts
        H _inner;   // _start and the message so far
        H _outer;   // the digest after the key XOR opad

        hmac(const hmac& other, Clone) noexcept
        : _start(other._start), _inner(other._inner), _outer(other._outer) {
        }

        void _init(const slice<const byte>& key) noexcept {
            unsigned char k[block_size] = {};
            if (key.size() > block_size) {
                H h;
                h.update(key);
                detail::HashAccess::finish(h, k);
                detail::secure_zero_object(h);
            } else if (key.size() != 0) {
                sgcl::detail::copy_bytes(k, key.data(), key.size());
            }
            unsigned char pad[block_size];
            for (size_t i = 0; i < block_size; ++i) {
                pad[i] = k[i] ^ 0x36;
            }
            _start = H();
            _start.update(slice<const byte>(reinterpret_cast<const byte*>(pad), block_size));
            for (size_t i = 0; i < block_size; ++i) {
                pad[i] = k[i] ^ 0x5c;
            }
            _outer = H();
            _outer.update(slice<const byte>(reinterpret_cast<const byte*>(pad), block_size));
            _inner = _start;
            detail::secure_zero(k, sizeof k);
            detail::secure_zero(pad, sizeof pad);
        }

        // The tag from an inner state and a copy of the outer one, both
        // used up in place: H(key ^ opad || H(key ^ ipad || message))
        static void _tag(H& inner, H& outer, unsigned char* out) noexcept {
            unsigned char d[digest_size];
            detail::HashAccess::finish(inner, d);
            outer.update(slice<const byte>(reinterpret_cast<const byte*>(d), digest_size));
            detail::HashAccess::finish(outer, out);
            detail::secure_zero(d, sizeof d);
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_start);
            detail::secure_zero_object(_inner);
            detail::secure_zero_object(_outer);
        }
    };

    using hmac_sha256 = hmac<sha256>;
    using hmac_sha512 = hmac<sha512>;
}
