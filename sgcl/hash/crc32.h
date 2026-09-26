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

// The two 32-bit CRCs. crc32 is CRC-32/ISO-HDLC of the CRC catalogue, the
// one zlib, gzip, zip, PNG and Ethernet carry and Go calls crc32.IEEE;
// crc32c is CRC-32/ISCSI, Castagnoli's polynomial, the one of iSCSI, ext4,
// Btrfs, SCTP, LevelDB and gRPC and Go's crc32.Castagnoli. Two types rather
// than one with a polynomial, since a program that needs one of them needs
// that one by name.
//
// Both hold four bytes and nothing else: the register. value() is the CRC
// and the hasher goes on after it; crc32::resume(v) goes on from a CRC saved
// earlier (Go's crc32.Update), and combine() makes the CRC of two pieces
// joined from the CRCs of the pieces, which is how a large buffer is
// hashed on several threads.
namespace sgcl::hash {
    class crc32 : public mixin::hasher<crc32> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 1;

        crc32() noexcept = default;

        // Going on from the CRC of what came before: crc.resume(v), where a
        // one-argument constructor elsewhere (xxh3, maphash) is a seed
        static crc32 resume(uint32_t value) noexcept {
            crc32 h;
            h._register = ~value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _register = detail::crc_update<uint32_t, Polynomial>(_register, detail::bytes(data.data()), data.size());
        }

        uint32_t value() const noexcept {
            return ~_register;
        }

        // The CRC as bytes, the most significant first (Go's Sum). gzip and
        // zip store it the other way round, from value()
        array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(value());
        }

        void reset() noexcept {
            _register = ~uint32_t(0);
        }

        // The CRC of A followed by B, from the CRC of A, the CRC of B and
        // the length of B in bytes
        static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept {
            return detail::crc_combine<uint32_t, Polynomial>(first, second, second_length);
        }

    private:
        static constexpr uint32_t Polynomial = 0xEDB88320u;   // 0x04C11DB7 reflected

        uint32_t _register = ~uint32_t(0);
    };

    class crc32c : public mixin::hasher<crc32c> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 1;

        crc32c() noexcept = default;

        static crc32c resume(uint32_t value) noexcept {
            crc32c h;
            h._register = ~value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _register = detail::crc_update<uint32_t, Polynomial>(_register, detail::bytes(data.data()), data.size());
        }

        uint32_t value() const noexcept {
            return ~_register;
        }

        array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(value());
        }

        void reset() noexcept {
            _register = ~uint32_t(0);
        }

        static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept {
            return detail::crc_combine<uint32_t, Polynomial>(first, second, second_length);
        }

    private:
        static constexpr uint32_t Polynomial = 0x82F63B78u;   // 0x1EDC6F41 reflected

        uint32_t _register = ~uint32_t(0);
    };
}
