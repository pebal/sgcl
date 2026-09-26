//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/hash_bytes.h"
#include "detail/bytes.h"
#include "detail/xxh3.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

// The hash of a hash table in the process (Go's hash/maphash): fast, and
// seeded once per process with a random seed, so that which keys share a
// bucket is not known outside the process and differs from run to run.
// Its values are for the process that made them: the algorithm behind them
// is not promised and may change between versions (today it is XXH3-64
// with the process's seed), and a value written to a file or sent away is
// the job of xxh3_64. An explicit seed gives the same values in every run,
// for a test.
//
// The seed of the process comes from the key of the library's own hash of
// strings (core/detail/hash_bytes.h, drawn once from std::random_device),
// through one folded product of two of its words, so that the key is not
// read back from it: one draw of entropy a process, and the environment
// variable SGCL_HASH_SEED, which fixes that key, fixes this seed too.
//
// Not a defence that can be argued: a seeded fast hash makes collisions
// hard to guess from outside, as Go's and Rust's tables are seeded, but
// its values can give the seed away to whoever sees them next to their
// keys. Keys from an adversary who may see hashes take siphash.
namespace sgcl::hash {
    namespace detail {
        struct MaphashSeed {
            uint64_t seed;
            unsigned char secret[Xxh3SecretSize];   // the seed's, made once
        };

        inline const MaphashSeed& maphash_seed() noexcept {
            static const MaphashSeed process = [] {
                const auto& key = sgcl::detail::hash_key();
                MaphashSeed s;
                s.seed = fold_product(key.k[1] ^ 0x243F6A8885A308D3ull, key.k[3] ^ 0x13198A2E03707344ull);
                xxh3_derive_secret(s.secret, s.seed);
                return s;
            }();
            return process;
        }
    }

    class maphash : public mixin::hasher<maphash> {
        friend class mixin::hasher<maphash>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 64;

        // With the process's seed: every maphash in the process agrees
        maphash() noexcept
        : _state(detail::maphash_seed().seed) {
        }

        // With this seed: the same values in every run
        explicit maphash(uint64_t seed) noexcept
        : _state(seed) {
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        // The bytes of a value as they lie in memory, in the machine's
        // order: the parts of a key that are not text (a number, a pair of
        // coordinates) without writing them out first. Only here, since a
        // maphash value never leaves the process; and only for a type every
        // byte of which is its value (no padding, no float, whose +0 and −0
        // are equal and differ in their bytes)
        template<class T>
        requires std::has_unique_object_representations_v<T>
        void update_value(const T& value) noexcept {
            _state.update(reinterpret_cast<const unsigned char*>(std::addressof(value)), sizeof(T));
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

        // the one-shot form of() calls: `maphash::of(key)`, `maphash::of(key, seed)`
        static uint64_t _of(const slice<const byte>& data) noexcept {
            const auto& process = detail::maphash_seed();
            return detail::xxh3_64(detail::bytes(data.data()), data.size(), process.seed, process.secret);
        }

        static uint64_t _of(const slice<const byte>& data, uint64_t seed) noexcept {
            return detail::xxh3_64(detail::bytes(data.data()), data.size(), seed);
        }
    };
}
