//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "sha1.h"
#include "sha256.h"
#include "sha3.h"
#include "sha512.h"
#include "../core/aliases.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

// A digest named by a value rather than by a type, Go's crypto.Hash: for
// where the algorithm is known only at run time — from a certificate, from
// a TLS handshake, from a key's parameters — and a template cannot be
// chosen. RSA-PSS, OAEP and ECDSA over a digest the data names take one;
// where the digest is known in the code, its type is simpler and costs
// nothing (sha256::of, hmac<sha256>).
namespace sgcl::crypto {
    enum class hash_id : uint8_t {
        sha1 = 1,
        sha224,
        sha256,
        sha384,
        sha512,
        sha512_256,
        sha3_224,
        sha3_256,
        sha3_384,
        sha3_512
    };

    namespace detail {
        // f called with std::type_identity of the hasher id names (no hasher
        // made: a Keccak state is 200 bytes); an id that is none of the list
        // (a value cast from a number) is a broken contract
        template<class F>
        decltype(auto) visit_hash(hash_id id, F&& f) {
            switch (id) {
                case hash_id::sha1: return f(std::type_identity<sha1>());
                case hash_id::sha224: return f(std::type_identity<sha224>());
                case hash_id::sha256: return f(std::type_identity<sha256>());
                case hash_id::sha384: return f(std::type_identity<sha384>());
                case hash_id::sha512: return f(std::type_identity<sha512>());
                case hash_id::sha512_256: return f(std::type_identity<sha512_256>());
                case hash_id::sha3_224: return f(std::type_identity<sha3_224>());
                case hash_id::sha3_256: return f(std::type_identity<sha3_256>());
                case hash_id::sha3_384: return f(std::type_identity<sha3_384>());
                case hash_id::sha3_512: return f(std::type_identity<sha3_512>());
            }
            throw invalid_argument("sgcl::crypto: unknown hash_id");
        }
    }

    // The length of the digest in bytes: 32 for hash_id::sha256
    SGCL_INLINE_HOT size_t digest_size(hash_id id) {
        return detail::visit_hash(id, [](auto t) { return decltype(t)::type::digest_size; });
    }

    // The block of the digest in bytes (the rate, for SHA-3): 64 for
    // hash_id::sha256, what HMAC pads its key to
    SGCL_INLINE_HOT size_t block_size(hash_id id) {
        return detail::visit_hash(id, [](auto t) { return decltype(t)::type::block_size; });
    }

    // The digest of data (bytes or text, which a slice of bytes takes both)
    // by the algorithm id names: sha256::of(data) when id is
    // hash_id::sha256, as a vector of digest_size(id) bytes
    SGCL_INLINE_HOT vector<byte> digest(hash_id id, const slice<const byte>& data) {
        return detail::visit_hash(id, [&](auto t) {
            typename decltype(t)::type h;
            h.update(data);
            auto d = h.value();
            return vector<byte>(d.begin(), d.end());
        });
    }

    namespace detail {
        // H::async_of_file's digest as the bytes digest_file gives
        template<class H>
        async::task<expected<vector<byte>, io::error>> co_digest_file(string path) noexcept {   // by value: the task is lazy
            auto d = co_await H::async_of_file(path);
            if (!d) {
                co_return io::detail::fail(d);
            }
            co_return vector<byte>(d->begin(), d->end());
        }

        inline async::task<expected<vector<byte>, io::error>> co_digest_file(hash_id id, string path) noexcept {
            // inside the task: an unknown id throws out of its co_await, not out of a noexcept call
            co_return co_await visit_hash(id, [&](auto t) {
                return co_digest_file<typename decltype(t)::type>(path);
            });
        }
    }

    // The digest of the whole file at path by the algorithm id names, what
    // digest(id, data) gives of its bytes: H::of_file(path) for the hasher
    // H of id — the file read to its end through a block, never held whole
    // in memory — as a vector of digest_size(id) bytes, or the error of the
    // open or of a read.
    // `digest_file(...)` on this thread, `co_await async_digest_file(...)` in a task
    SGCL_INLINE_HOT expected<vector<byte>, io::error> digest_file(hash_id id, const string& path) {
        return detail::visit_hash(id, [&](auto t) -> expected<vector<byte>, io::error> {
            auto d = decltype(t)::type::of_file(path);
            if (!d) {
                return io::detail::fail(d);
            }
            return vector<byte>(d->begin(), d->end());
        });
    }

    SGCL_INLINE_HOT async::task<expected<vector<byte>, io::error>> async_digest_file(hash_id id, const string& path) noexcept {
        return detail::co_digest_file(id, path);
    }
}
