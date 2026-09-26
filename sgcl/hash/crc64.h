//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/crc.h"
#include "detail/crc_arm64.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// The two 64-bit CRCs. crc64 is CRC-64/XZ of the CRC catalogue: the one xz
// and 7z write, and the one Go calls crc64.ECMA. The name is not Go's on
// purpose: the catalogue has a CRC-64/ECMA-182 too, over the same
// polynomial but neither reflected nor inverted, with another result for
// the same bytes, and a crc64_ecma here would send whoever checks the
// catalogue to the wrong one. crc64_iso is CRC-64/GO-ISO, Go's crc64.ISO,
// the polynomial of ISO 3309, for agreeing with data Go wrote.
//
// Everything else is as the 32-bit CRCs have it: the register and nothing
// more, value() without ending anything, resume(v) that goes on from a
// saved CRC, combine().
namespace sgcl::hash {
    class crc64 : public mixin::hasher<crc64> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 1;

        crc64() noexcept = default;

        // Going on from the CRC of what came before: crc.resume(v), where a
        // one-argument constructor elsewhere (xxh3, maphash) is a seed
        static crc64 resume(uint64_t value) noexcept {
            crc64 h;
            h._register = ~value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _register = detail::crc_update<uint64_t, Polynomial>(_register, detail::bytes(data.data()), data.size());
        }

        uint64_t value() const noexcept {
            return ~_register;
        }

        // The CRC as bytes, the most significant first (Go's Sum; xz stores
        // it the other way round, from value())
        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(value());
        }

        void reset() noexcept {
            _register = ~uint64_t(0);
        }

        // The CRC of A followed by B, from the CRC of A, the CRC of B and
        // the length of B in bytes
        static constexpr uint64_t combine(uint64_t first, uint64_t second, uint64_t second_length) noexcept {
            return detail::crc_combine<uint64_t, Polynomial>(first, second, second_length);
        }

    private:
        static constexpr uint64_t Polynomial = 0xC96C5795D7870F42ull;   // ECMA-182's 0x42F0E1EBA9EA3693 reflected

        uint64_t _register = ~uint64_t(0);
    };

    class crc64_iso : public mixin::hasher<crc64_iso> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 1;

        crc64_iso() noexcept = default;

        static crc64_iso resume(uint64_t value) noexcept {
            crc64_iso h;
            h._register = ~value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _register = detail::crc_update<uint64_t, Polynomial>(_register, detail::bytes(data.data()), data.size());
        }

        uint64_t value() const noexcept {
            return ~_register;
        }

        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(value());
        }

        void reset() noexcept {
            _register = ~uint64_t(0);
        }

        static constexpr uint64_t combine(uint64_t first, uint64_t second, uint64_t second_length) noexcept {
            return detail::crc_combine<uint64_t, Polynomial>(first, second, second_length);
        }

    private:
        static constexpr uint64_t Polynomial = 0xD800000000000000ull;   // x^64 + x^4 + x^3 + x + 1 (0x1B) reflected

        uint64_t _register = ~uint64_t(0);
    };
}
