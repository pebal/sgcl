//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/xxh3.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// XXH3 (xxHash 0.8), 64 and 128 bits, with a seed: a fast hash whose values
// stay the same across versions, machines and runs, for what is written
// down — a file's identity, a key of a cache on disk, a checksum sent over
// a network. Its format has been frozen since 0.8.0 (2020), and xxhsum,
// Go's github.com/zeebo/xxh3, Rust's xxhash-rust and Python's xxhash give
// the same numbers. It is not a defence against keys chosen by an
// adversary: a table in memory keyed by what a client sends takes siphash,
// or maphash where the hashes never leave the process.
//
// A hasher holds its eight lanes, a buffer of four stripes (256 bytes) and,
// with a seed other than 0, the secret made from it: about 540 bytes, a
// plain value that a copy branches. of() hashes in one call with no state
// made. detail/xxh3.h says how the algorithm goes.
namespace sgcl::hash {
    class xxh3_64 : public mixin::hasher<xxh3_64> {
        friend class mixin::hasher<xxh3_64>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 64;   // a stripe

        xxh3_64() noexcept = default;

        explicit xxh3_64(uint64_t seed) noexcept
        : _state(seed) {
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        uint64_t value() const noexcept {
            return _state.value64();
        }

        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(value());
        }

        // As new, with the seed it was made with
        void reset() noexcept {
            _state.reset();
        }

    private:
        detail::Xxh3Stream _state;

        // the one-shot form of() calls: `xxh3_64::of(data)`, `xxh3_64::of(data, seed)`
        static uint64_t _of(const slice<const byte>& data, uint64_t seed = 0) noexcept {
            return detail::xxh3_64(detail::bytes(data.data()), data.size(), seed);
        }
    };

    // The 128-bit XXH3: the value is the digest, 16 bytes, the high half
    // first, as xxhsum writes it (XXH128_canonical_t)
    class xxh3_128 : public mixin::hasher<xxh3_128> {
        friend class mixin::hasher<xxh3_128>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 16;
        static constexpr size_t block_size = 64;

        xxh3_128() noexcept = default;

        explicit xxh3_128(uint64_t seed) noexcept
        : _state(seed) {
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        array<byte, 16> value() const noexcept {
            return detail::xxh3_128_bytes(_state.value128());
        }

        array<byte, 16> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.reset();
        }

    private:
        detail::Xxh3Stream _state;

        static array<byte, 16> _of(const slice<const byte>& data, uint64_t seed = 0) noexcept {
            return detail::xxh3_128_bytes(detail::xxh3_128(detail::bytes(data.data()), data.size(), seed));
        }
    };
}
