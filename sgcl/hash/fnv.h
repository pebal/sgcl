//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// The Fowler/Noll/Vo hashes (draft-eastlake-fnv): a start value, and for
// every byte a multiplication by a prime and an XOR of the byte. FNV-1
// multiplies first, FNV-1a XORs first, and each comes in 32, 64 and 128
// bits: six types, since nobody picks FNV today for speed — it is one
// multiplication a byte, one after another — but to agree with values
// somebody already computed, and then the variant is not a choice.
//
// The state of an FNV is its value, so each type has resume(v) that
// goes on from a value saved earlier, as crc32 and adler32 have (Go does it
// through UnmarshalBinary). There is no combine: the value after A says
// nothing that would let a hash of B be joined to it.
//
// The 128-bit prime is 2^88 + 2^8 + 0x3b, so its product is the state
// times 0x13b plus the state moved up 88 bits: two words, a small
// multiplication and a shift, no 128-bit arithmetic.
namespace sgcl::hash {
    namespace detail {
        inline constexpr uint32_t Fnv32Offset = 0x811c9dc5u;
        inline constexpr uint32_t Fnv32Prime = 0x01000193u;
        inline constexpr uint64_t Fnv64Offset = 0xcbf29ce484222325ull;
        inline constexpr uint64_t Fnv64Prime = 0x00000100000001b3ull;
        inline constexpr uint64_t Fnv128OffsetHigh = 0x6c62272e07bb0142ull;
        inline constexpr uint64_t Fnv128OffsetLow = 0x62b821756295c58dull;
        inline constexpr uint64_t Fnv128PrimeLow = 0x13b;   // the prime less its 2^88

        template<class T>
        inline T fnv1(T h, T prime, const unsigned char* p, size_t n) noexcept {
            for (size_t i = 0; i < n; ++i) {
                h = T(h * prime);
                h ^= p[i];
            }
            return h;
        }

        template<class T>
        inline T fnv1a(T h, T prime, const unsigned char* p, size_t n) noexcept {
            for (size_t i = 0; i < n; ++i) {
                h ^= p[i];
                h = T(h * prime);
            }
            return h;
        }

        // A 128-bit state as two words
        struct Fnv128 {
            uint64_t high = Fnv128OffsetHigh;
            uint64_t low = Fnv128OffsetLow;

            // The state times the prime, modulo 2^128: the low word times
            // 0x13b spread over both words, the high word's product added,
            // and the low word moved up 88 bits, which lands in the high word
            // 24 bits up
            void multiply() noexcept {
                uint64_t low_product = low * Fnv128PrimeLow;
                uint64_t carry = ((low >> 32) * Fnv128PrimeLow + ((low & 0xffffffffu) * Fnv128PrimeLow >> 32)) >> 32;
                high = high * Fnv128PrimeLow + carry + (low << 24);
                low = low_product;
            }

            // The state whose bytes() are these
            static Fnv128 from(const array<byte, 16>& value) noexcept {
                Fnv128 f;
                f.high = 0;
                f.low = 0;
                for (size_t i = 0; i < 8; ++i) {
                    f.high = f.high << 8 | uint64_t(value[i]);
                    f.low = f.low << 8 | uint64_t(value[8 + i]);
                }
                return f;
            }

            array<byte, 16> bytes() const noexcept {
                array<byte, 16> out;
                for (size_t i = 0; i < 8; ++i) {
                    out[i] = byte(high >> (56 - 8 * i));
                    out[8 + i] = byte(low >> (56 - 8 * i));
                }
                return out;
            }
        };
    }

    // FNV-1, 32 bits
    class fnv32 : public mixin::hasher<fnv32> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 1;

        fnv32() noexcept = default;

        // Going on from the value of what came before: the state of an
        // FNV is its value
        static fnv32 resume(uint32_t value) noexcept {
            fnv32 h;
            h._h = value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _h = detail::fnv1(_h, detail::Fnv32Prime, detail::bytes(data.data()), data.size());
        }

        uint32_t value() const noexcept {
            return _h;
        }

        array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(_h);
        }

        void reset() noexcept {
            _h = detail::Fnv32Offset;
        }

    private:
        uint32_t _h = detail::Fnv32Offset;
    };

    // FNV-1a, 32 bits
    class fnv32a : public mixin::hasher<fnv32a> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 4;
        static constexpr size_t block_size = 1;

        fnv32a() noexcept = default;

        static fnv32a resume(uint32_t value) noexcept {
            fnv32a h;
            h._h = value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _h = detail::fnv1a(_h, detail::Fnv32Prime, detail::bytes(data.data()), data.size());
        }

        uint32_t value() const noexcept {
            return _h;
        }

        array<byte, 4> digest() const noexcept {
            return detail::big_endian<4>(_h);
        }

        void reset() noexcept {
            _h = detail::Fnv32Offset;
        }

    private:
        uint32_t _h = detail::Fnv32Offset;
    };

    // FNV-1, 64 bits
    class fnv64 : public mixin::hasher<fnv64> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 1;

        fnv64() noexcept = default;

        static fnv64 resume(uint64_t value) noexcept {
            fnv64 h;
            h._h = value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _h = detail::fnv1(_h, detail::Fnv64Prime, detail::bytes(data.data()), data.size());
        }

        uint64_t value() const noexcept {
            return _h;
        }

        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(_h);
        }

        void reset() noexcept {
            _h = detail::Fnv64Offset;
        }

    private:
        uint64_t _h = detail::Fnv64Offset;
    };

    // FNV-1a, 64 bits
    class fnv64a : public mixin::hasher<fnv64a> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 1;

        fnv64a() noexcept = default;

        static fnv64a resume(uint64_t value) noexcept {
            fnv64a h;
            h._h = value;
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            _h = detail::fnv1a(_h, detail::Fnv64Prime, detail::bytes(data.data()), data.size());
        }

        uint64_t value() const noexcept {
            return _h;
        }

        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(_h);
        }

        void reset() noexcept {
            _h = detail::Fnv64Offset;
        }

    private:
        uint64_t _h = detail::Fnv64Offset;
    };

    // FNV-1, 128 bits. The result is sixteen bytes, the most significant
    // first, so value() and digest() are the same thing: there is no
    // 128-bit integer to give
    class fnv128 : public mixin::hasher<fnv128> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 16;
        static constexpr size_t block_size = 1;

        fnv128() noexcept = default;

        static fnv128 resume(const array<byte, 16>& value) noexcept {
            fnv128 h;
            h._h = detail::Fnv128::from(value);
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            auto p = detail::bytes(data.data());
            for (size_t i = 0, n = data.size(); i < n; ++i) {
                _h.multiply();
                _h.low ^= p[i];
            }
        }

        array<byte, 16> value() const noexcept {
            return _h.bytes();
        }

        array<byte, 16> digest() const noexcept {
            return _h.bytes();
        }

        void reset() noexcept {
            _h = detail::Fnv128();
        }

    private:
        detail::Fnv128 _h;
    };

    // FNV-1a, 128 bits
    class fnv128a : public mixin::hasher<fnv128a> {
    public:
        using hasher::update;

        static constexpr size_t digest_size = 16;
        static constexpr size_t block_size = 1;

        fnv128a() noexcept = default;

        static fnv128a resume(const array<byte, 16>& value) noexcept {
            fnv128a h;
            h._h = detail::Fnv128::from(value);
            return h;
        }

        void update(const slice<const byte>& data) noexcept {
            auto p = detail::bytes(data.data());
            for (size_t i = 0, n = data.size(); i < n; ++i) {
                _h.low ^= p[i];
                _h.multiply();
            }
        }

        array<byte, 16> value() const noexcept {
            return _h.bytes();
        }

        array<byte, 16> digest() const noexcept {
            return _h.bytes();
        }

        void reset() noexcept {
            _h = detail::Fnv128();
        }

    private:
        detail::Fnv128 _h;
    };
}
