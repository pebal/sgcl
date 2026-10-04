//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/array.h"
#include "detail/words.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

// The nonces of one key, never twice the same: a 96-bit counter, each
// next() the one after the last, big-endian. The safe way to give aes_gcm
// and chacha20_poly1305 their nonces (SP 800-38D §8.2.1 calls it the
// deterministic construction): a counter cannot collide, where 96 random
// bits collide with a probability that grows with the square of the number
// of messages. One counter per key, kept for as long as the key is used —
// a counter started again from zero under the same key repeats every nonce.
//
// Move-only, and the counter moved from is spent: a copy would hand out the
// same nonces twice, which is the one thing the type is there to prevent.
namespace sgcl::crypto {
    class nonce_counter {
    public:
        static constexpr size_t nonce_size = 12;

        // From zero
        nonce_counter() noexcept = default;

        // From a given nonce on: where a program resumes the counter it
        // kept (the last nonce used, plus one), or starts the nonces of a
        // second sender of the same key in a range of their own
        SGCL_INLINE_HOT explicit nonce_counter(const array<byte, 12>& start) noexcept
        : _high(detail::load_be32(detail::bytes(start.data())))
        , _low(detail::load_be64(detail::bytes(start.data()) + 4)) {
        }

        nonce_counter(const nonce_counter&) = delete;
        nonce_counter& operator=(const nonce_counter&) = delete;

        SGCL_INLINE_HOT nonce_counter(nonce_counter&& other) noexcept
        : _high(other._high), _low(other._low), _spent(other._spent) {
            other._spent = true;
        }

        SGCL_INLINE_HOT nonce_counter& operator=(nonce_counter&& other) noexcept {
            if (this != &other) {
                _high = other._high;
                _low = other._low;
                _spent = other._spent;
                other._spent = true;
            }
            return *this;
        }

        // The next nonce. After 2^96 of them (or on a counter moved from)
        // std::out_of_range: the key must be replaced.
        array<byte, 12> next() {
            if (_spent) {
                throw out_of_range("sgcl::crypto::nonce_counter: every nonce has been used (or the counter was moved from)");
            }
            array<byte, 12> n;
            detail::store_be32(detail::bytes(n.data()), _high);
            detail::store_be64(detail::bytes(n.data()) + 4, _low);
            if (++_low == 0 && ++_high == 0) {
                _spent = true;
            }
            return n;
        }

    private:
        uint32_t _high = 0;
        uint64_t _low = 0;
        bool _spent = false;
    };
}
