//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"

#include <cstddef>
#include <cstdint>

// The engine of the four CRCs of the module, one template over the width
// of the register (uint32_t or uint64_t) and its polynomial. All four are
// reflected CRCs with an all-ones start and an all-ones final XOR, which
// is what makes one engine enough: the register holds the remainder with
// its lowest-degree coefficient in the top bit, a byte of the message goes
// in at the bottom, and the value a caller sees is the register inverted.
//
// Three things live here. The portable path is slicing by eight: a word of
// eight bytes XORed into the register and looked up in eight tables of 256
// entries, where table s says what a byte does to the register with s
// bytes after it — eight lookups a word where the byte-wise loop has eight
// dependent ones. The tables are computed by the compiler from the
// polynomial (8 KB for a 32-bit CRC, 16 KB for a 64-bit one), so there is
// no generator and no table file. The multiplication modulo the polynomial
// is what combine() needs: the CRC of A followed by B is the CRC of A moved
// past |B| zero bytes, XORed with the CRC of B, and moving a remainder past
// n bytes is multiplying it by x^(8n) mod P. The powers x^(8·2^k) mod P
// are a table too, so a combine is one multiplication per set bit of n.
// And the powers x^e mod P themselves, which arm64.h builds its folding
// constants from.
namespace sgcl::hash::detail {
    template<class T>
    inline constexpr int RegisterBits = int(sizeof(T) * 8);

    // x^0 in the reflected representation: the top bit
    template<class T>
    inline constexpr T ReflectedOne = T(1) << (RegisterBits<T> - 1);

    template<class T>
    struct CrcTables {
        T table[8][256];
    };

    template<class T, T Poly>
    constexpr CrcTables<T> make_crc_tables() noexcept {
        CrcTables<T> t {};
        for (unsigned i = 0; i < 256; ++i) {
            T r = T(i);
            for (int bit = 0; bit < 8; ++bit) {
                r = T((r >> 1) ^ (Poly & (T(0) - T(r & 1))));
            }
            t.table[0][i] = r;
        }
        for (int s = 1; s < 8; ++s) {
            for (unsigned i = 0; i < 256; ++i) {
                T prev = t.table[s - 1][i];
                t.table[s][i] = T((prev >> 8) ^ t.table[0][prev & 0xff]);
            }
        }
        return t;
    }

    template<class T, T Poly>
    inline constexpr CrcTables<T> crc_tables = make_crc_tables<T, Poly>();

    // The register after p[0, n), slicing by eight; the bytes after the last
    // whole word one at a time
    template<class T, T Poly>
    inline T crc_update_portable(T reg, const unsigned char* p, size_t n) noexcept {
        const auto& t = crc_tables<T, Poly>.table;
        for (; n >= 8; p += 8, n -= 8) {
            uint64_t w = load_le64(p) ^ uint64_t(reg);
            reg = T(t[7][w & 0xff] ^ t[6][(w >> 8) & 0xff] ^ t[5][(w >> 16) & 0xff] ^ t[4][(w >> 24) & 0xff]
                  ^ t[3][(w >> 32) & 0xff] ^ t[2][(w >> 40) & 0xff] ^ t[1][(w >> 48) & 0xff] ^ t[0][w >> 56]);
        }
        for (; n; ++p, --n) {
            reg = T(t[0][(reg ^ *p) & 0xff] ^ (reg >> 8));
        }
        return reg;
    }

    // x^e mod P, reflected, one step a power: for the folding constants,
    // whose exponents are a few hundred and which the compiler computes
    template<class T, T Poly>
    constexpr T crc_xpow(unsigned e) noexcept {
        T v = ReflectedOne<T>;
        for (unsigned i = 0; i < e; ++i) {
            v = T((v >> 1) ^ (Poly & (T(0) - T(v & 1))));
        }
        return v;
    }

    // a·b mod P, reflected: b multiplied by x once a step, added where a has
    // the coefficient; the same work whatever the values
    template<class T, T Poly>
    constexpr T crc_multiply(T a, T b) noexcept {
        T product = 0;
        for (int i = 0; i < RegisterBits<T>; ++i) {
            product ^= b & (T(0) - T((a >> (RegisterBits<T> - 1 - i)) & 1));
            b = T((b >> 1) ^ (Poly & (T(0) - T(b & 1))));
        }
        return product;
    }

    // x^(8·2^k) mod P for k = 0…63: every length a uint64_t can hold
    template<class T>
    struct CrcPowers {
        T power[64];
    };

    template<class T, T Poly>
    constexpr CrcPowers<T> make_crc_powers() noexcept {
        CrcPowers<T> p {};
        p.power[0] = T(ReflectedOne<T> >> 8);   // x^8, below the degree of P
        for (int k = 1; k < 64; ++k) {
            p.power[k] = crc_multiply<T, Poly>(p.power[k - 1], p.power[k - 1]);
        }
        return p;
    }

    template<class T, T Poly>
    inline constexpr CrcPowers<T> crc_powers = make_crc_powers<T, Poly>();

    // The CRC of A followed by B from the CRC of A, the CRC of B and the
    // length of B. With an all-ones start and final XOR, the two cancel
    // out: crc(A‖B) = crc(A)·x^(8|B|) mod P ⊕ crc(B), on the values
    // themselves, not the registers
    template<class T, T Poly>
    constexpr T crc_combine(T first, T second, uint64_t second_length) noexcept {
        const auto& p = crc_powers<T, Poly>.power;
        for (int k = 0; second_length; ++k, second_length >>= 1) {
            if (second_length & 1) {
                first = crc_multiply<T, Poly>(first, p[k]);
            }
        }
        return T(first ^ second);
    }
}
