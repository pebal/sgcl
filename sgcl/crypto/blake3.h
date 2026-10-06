//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/blake3.h"
#include "detail/bytes.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/slice.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// BLAKE3 (its specification, 2021): a hash, a MAC (keyed_hash) and a key
// derivation function (derive_key) in one, with an output of any length
// read from any position, over a binary tree of 1 KiB chunks, whose
// independent chunks the processor's vector units hash four at a time.
// Neither Go's standard library nor golang.org/x/crypto has it; the C
// library of its authors is the reference the tests hold it to.
//
// A hasher in the shape of the hash module's (hash::mixin::hasher): the
// key words, the chunk being filled, the stack of chaining values of the
// subtrees to its left, about 1.9 KB, a copy a branch. A keyed state and a
// derive_key state hold the key's equivalent and are zeroed by the
// destructor; a plain one is left as sha256 leaves its state.
namespace sgcl::crypto {
    class blake3 : public hash::mixin::hasher<blake3> {
        friend class hash::mixin::hasher<blake3>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;
        using hasher::of;

        static constexpr size_t digest_size = 32;
        static constexpr size_t block_size = 64;
        static constexpr size_t key_size = 32;

        // The hash mode
        SGCL_INLINE_HOT blake3() noexcept {
            _state.init(detail::sha256_iv, 0, false);
        }

        // The keyed_hash mode: a MAC under a key of 32 bytes; another length
        // is std::invalid_argument
        explicit blake3(const slice<const byte>& key) {
            if (key.size() != key_size) {
                throw invalid_argument("sgcl::crypto::blake3: a key of another length than 32 bytes");
            }
            _init_keyed(key);
        }

        blake3(const blake3&) noexcept = default;
        blake3& operator=(const blake3&) noexcept = default;

        SGCL_INLINE_HOT ~blake3() {
            if (_state.wipe) {
                detail::secure_zero_object(_state);
            }
        }

        SGCL_INLINE_HOT void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        // The first 32 bytes of the output; the hasher goes on
        SGCL_INLINE_HOT array<byte, 32> value() const noexcept {
            array<byte, 32> out;
            _state.output(detail::bytes(out.data()), 32, 0);
            return out;
        }

        SGCL_INLINE_HOT array<byte, 32> digest() const noexcept {
            return value();
        }

        // out.size() bytes of the output from byte `position` on: the
        // extendable output, any length, read from anywhere; the hasher
        // goes on. The first 32 bytes from position 0 are value()
        SGCL_INLINE_HOT void value_to(const slice<byte>& out, uint64_t position = 0) const noexcept {
            _state.output(detail::bytes(out.data()), out.size(), position);
        }

        // As just made: the input dropped, the key and the mode kept
        SGCL_INLINE_HOT void reset() noexcept {
            _state.reset();
        }

        // Whether tag is the first tag.size() bytes of the output, compared
        // in constant time; an empty tag or one past 64 bytes is false
        [[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept {
            if (tag.size() == 0 || tag.size() > 64) {
                return false;
            }
            unsigned char mine[64];
            _state.output(mine, tag.size(), 0);
            bool ok = detail::equal_bytes(mine, detail::bytes(tag.data()), tag.size());
            detail::secure_zero(mine, sizeof mine);
            return ok;
        }

        // The keyed_hash of data under a key of 32 bytes in one call, the
        // data first, as hmac::of(data, key)
        static array<byte, 32> of(const slice<const byte>& data, const slice<const byte>& key) {
            blake3 h(key);
            h.update(data);
            return h.value();
        }

        // The derive_key mode in one call: n bytes of key (32 by default)
        // from key material, under a context string — hard-coded, unique to
        // the application and the purpose ("example.com 2026-10-05 session
        // tokens v1"), never itself a secret or a variable. A secret_bytes
        SGCL_INLINE_HOT static secret_bytes derive_key(const slice<const byte>& context, const slice<const byte>& key_material,
                                                       size_t n = 32) noexcept {
            blake3 h = for_derive_key(context);
            h.update(key_material);
            secret_bytes out(n);
            h.value_to(out);
            return out;
        }

        // The derive_key mode for key material in pieces: a hasher whose
        // key is the context's, every update key material
        static blake3 for_derive_key(const slice<const byte>& context) noexcept {
            detail::Blake3State c;
            c.init(detail::sha256_iv, detail::blake3_derive_key_context, true);
            c.update(detail::bytes(context.data()), context.size());
            unsigned char context_key[32];
            c.output(context_key, 32, 0);
            uint32_t words[8];
            for (int i = 0; i < 8; ++i) {
                words[i] = detail::load_le32(context_key + 4 * i);
            }
            blake3 h(Mode{});
            h._state.init(words, detail::blake3_derive_key_material, true);
            detail::secure_zero(context_key, sizeof context_key);
            detail::secure_zero(words, sizeof words);
            detail::secure_zero_object(c);
            return h;
        }

    private:
        struct Mode {};

        detail::Blake3State _state;

        explicit blake3(Mode) noexcept {
        }

        SGCL_INLINE_HOT void _init_keyed(const slice<const byte>& key) noexcept {
            uint32_t words[8];
            for (int i = 0; i < 8; ++i) {
                words[i] = detail::load_le32(detail::bytes(key.data()) + 4 * i);
            }
            _state.init(words, detail::blake3_keyed_hash, true);
            detail::secure_zero(words, sizeof words);
        }

        SGCL_INLINE_HOT void _finish(unsigned char* out) noexcept {
            _state.output(out, 32, 0);
        }
    };
}
