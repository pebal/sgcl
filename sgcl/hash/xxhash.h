//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/xxhash.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// XXH32 and XXH64, the hashes of xxHash before XXH3, with a seed: the
// checksums LZ4's frame (XXH32) and zstd's frame (XXH64) carry, and what
// many file formats and protocols written before 2020 name. Their values
// are fixed by the specification and are those of xxhsum -H0 and -H1.
// A new design that only needs a fast fixed hash takes xxh3_64, which is
// faster on short inputs and no slower on long ones.
//
// A hasher holds its four lanes, the seed, the length and up to a stripe
// of bytes not yet taken: 48 bytes for xxh32, 88 for xxh64, plain values
// that a copy branches. of() hashes in one call with no state made.
namespace sgcl::hash {
    class xxh32 : public mixin::hasher<xxh32> {
        friend class mixin::hasher<xxh32>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 16;   // a stripe: four lanes of four bytes

        xxh32() noexcept = default;

        SGCL_INLINE_HOT explicit xxh32(uint32_t seed) noexcept
        : _state(seed) {
        }

        SGCL_INLINE_HOT void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        SGCL_INLINE_HOT uint32_t value() const noexcept {
            return _state.value();
        }

        SGCL_INLINE_HOT array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(value());
        }

        // As new, with the seed it was made with
        SGCL_INLINE_HOT void reset() noexcept {
            _state.reset();
        }

    private:
        detail::Xxh32Stream _state;

        // the one-shot form of() calls: `xxh32::of(data)`, `xxh32::of(data, seed)`
        SGCL_INLINE_HOT static uint32_t _of(const slice<const byte>& data, uint32_t seed = 0) noexcept {
            return detail::xxh32(detail::bytes(data.data()), data.size(), seed);
        }
    };

    class xxh64 : public mixin::hasher<xxh64> {
        friend class mixin::hasher<xxh64>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 32;   // a stripe: four lanes of eight bytes

        xxh64() noexcept = default;

        SGCL_INLINE_HOT explicit xxh64(uint64_t seed) noexcept
        : _state(seed) {
        }

        SGCL_INLINE_HOT void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        SGCL_INLINE_HOT uint64_t value() const noexcept {
            return _state.value();
        }

        SGCL_INLINE_HOT array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(value());
        }

        SGCL_INLINE_HOT void reset() noexcept {
            _state.reset();
        }

    private:
        detail::Xxh64Stream _state;

        SGCL_INLINE_HOT static uint64_t _of(const slice<const byte>& data, uint64_t seed = 0) noexcept {
            return detail::xxh64(detail::bytes(data.data()), data.size(), seed);
        }
    };
}
