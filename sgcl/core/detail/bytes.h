//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "os.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__aarch64__) && defined(__ARM_NEON)
#include <arm_neon.h>
#define SGCL_BYTES_NEON 1
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#define SGCL_BYTES_SSE2 1
#endif

#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define SGCL_BYTES_ZVA 1
#endif

// Copying, filling and moving a run of bytes whose length the compiler
// does not know. The rule (DESIGN 394): our own code for short runs, where
// libc is dear, and libc from the length where it stops losing. The caller
// does not choose; it calls copy_bytes, fill_bytes or move_bytes, and the
// length chooses.
//
// A memcpy or memset of a length the compiler knows is not a call but the
// moves it would emit anyway — `ldp q0, q1` / `stp q0, q1` for thirty-two
// bytes, a `dup` and the stores for a fill — so every one below with a
// constant length is not a call into libc; the calls into libc are the
// ones with n.
//
// Short runs are where libc is dear: below 64 bytes its memset and memcpy
// store eight bytes and then one byte at a time, in loops whose trip
// counts a mixture of lengths mispredicts. Here a run goes as two blocks
// that overlap in the middle, which touches nothing outside it: the end
// block starts at n - w, so it never passes the last byte. From 64 bytes
// up libc stores 32-byte pairs at an aligned destination, and that our
// own blocks and loops do not beat. Measured on an M2 Ultra, ns a call,
// ours / libc, the destination in cache and fresh (a new line each call),
// lengths drawn from 1..64 ("mix") and single ones (2026-10-03):
//
//                  in cache                     fresh
//   copy  mix      5.6 / 11.8                   7.1 / 13.7
//         32, 48   1.35, 1.30 / 3.68, 4.29      2.44, 2.58 / 3.52, 4.13
//         64       1.55 / 2.10                  2.68 / 2.81
//   fill  mix      5.2 / 11.2                   6.6 / 13.1
//         32, 48   1.16, 1.11 / 3.46, 4.13      2.38, 2.33 / 3.74, 4.42
//         64       1.24 / 2.15                  2.30 / 2.71
//   move  mix      5.9 / 12.5                   7.7 / 13.5
//         48, 64   1.76, 1.80 / 4.89, 2.67      2.48, 2.55 / 5.11, 2.78
//         80..128  2.21..2.39 / 2.90..3.11      4.70..4.97 / 4.83..5.94
//
// Above those lengths our blocks lost in cache — copy 80..256 by 1.06 to
// 1.31x, fill 80..112 with two 64-byte blocks by 1.04 to 1.18x — and the
// 128-byte loops that were here before by up to 1.6x at 1 to 64 KB and 2x
// for a zero fill of a megabyte. So fill is ours to 64 bytes and move to
// 128 (eight q registers read before any is written), and libc's above,
// with one exception: a zero fill of 16 to 32 KB, which dc zva does in
// whole 64-byte blocks without reading the lines first. libc's memset
// turns to dc zva only from 32 KB (below it stores pairs whatever the
// value): fresh, ours is 0.42x of it at 16 and 24 KB (242 against 559 ns,
// 277 against 665), in cache level within the noise (0.75 to 1.09x over
// five runs); from 32 KB the two are the same. A copy is ours to 128
// bytes (two blocks of 32 to 64, four from 65, without a loop), the base
// loop's from 129 bytes to 4 KB and libc's from 4 KB: the
// destination of a copy is often a slot the allocator has just handed
// out, and there, at 160 to 256 bytes, the loop beat libc's memcpy
// (bench_string make 160 and 200: libc +9.2% and +7.3%, DESIGN 403),
// while from 4 KB libc wins cold and in cache (DESIGN 402).
//
// What a call site carries is the short path alone: a run under 32 bytes
// (to 32 for a move) meets one compare and the ladder; a longer one is one
// call into a function out of line, the same for every caller, which picks
// our blocks, the base loop or libc by the length. When that choice was
// made at the call site (notes 394 and 407: two compares more and three
// calls in place of one), the small functions that copy grew past what
// the compiler inlines: txt's format_sink::put was called out of line from
// 46 places of bench_format, for every literal run and every number's
// digits (format +5 to 8%), and a vector's growth called its _relocate.
// The blocks of 33 to 128 bytes in place at the call site cost the same:
// four blocks of 32 to 128 took put out of line again (50 calls from 29
// functions of bench_format), and two to 64 alone still took make_buffer
// out of make and vector<long>'s _relocate out of its growth; so they
// stay in copy_long, at its head, before the loop.
// Where the compiler knows the length, the choice is made when compiling
// and costs nothing: libc's lengths call libc with the constant, which
// the compiler may expand in place (a vector of 16 longs zeroed by four
// paired stores, not two calls). The zero lines are tested only where the value
// is a zero the compiler sees (a vector's zeroing): such a call goes to
// zero_long in place of fill_long, chosen when compiling, not at run time.
// A test of a value known at run time at every call site took a padded
// field in format from 15.0 ns to 15.9 and a centred one from 15.7 to
// 17.0, because the caller grew past what the compiler keeps in registers
// (notes 162 and 167).
//
// A short fill read back at once (SHA-2's padding before the compression)
// is slower than libc's memset, which stores eight bytes and then single
// bytes without overlap: here the two blocks overlap, and a load over
// bytes two stores in flight both wrote waits for them.
namespace sgcl::detail {
    // The lengths up to which the work is our own blocks
    inline constexpr size_t CopyOwnUpTo = 128;
    // From CopyOwnUpTo to CopyLibcFrom the base loop stays: into a slot the
    // allocator has just handed out (the string's work) libc is slower there
    // (bench_string make 160 +9.2%, 200 +7.3%, DESIGN 403: never slower);
    // from 4 KB libc wins cold and in cache (DESIGN 402).
    inline constexpr size_t CopyLibcFrom = 4096;
    inline constexpr size_t FillOwnUpTo = 64;
    inline constexpr size_t MoveOwnUpTo = 128;
    // A zero fill in [ZeroLinesFrom, ZeroLinesBelow), of a zero the
    // compiler sees, goes by dc zva (arm64)
    inline constexpr size_t ZeroLinesFrom = 16384;
    inline constexpr size_t ZeroLinesBelow = 32768;

    // A copy of 32 bytes and more: to 64 two blocks of 32 that overlap in
    // the middle, to CopyOwnUpTo four (the two at each end overlapping),
    // to CopyLibcFrom the base loop, 128 bytes a turn, from there libc. From
    // 65 to 128 bytes the loop took two or three turns of 32 and the tail,
    // a branch each, which a mixture of lengths mispredicts; the four blocks
    // take none
    SGCL_NOINLINE inline void copy_long(unsigned char* d, const unsigned char* s, size_t n) noexcept {
        if (n <= CopyOwnUpTo) {
            if (n <= 64) {
                std::memcpy(d, s, 32);
                std::memcpy(d + n - 32, s + n - 32, 32);
            } else {
                std::memcpy(d, s, 32);
                std::memcpy(d + 32, s + 32, 32);
                std::memcpy(d + n - 64, s + n - 64, 32);
                std::memcpy(d + n - 32, s + n - 32, 32);
            }
            return;
        }
        if (n >= CopyLibcFrom) {
            std::memcpy(d, s, n);
            return;
        }
        size_t at = 0;
#if defined(__ARM_NEON)
        for (; at + 128 <= n; at += 128) {
            uint8x16x4_t a = vld1q_u8_x4(s + at);
            uint8x16x4_t b = vld1q_u8_x4(s + at + 64);
            vst1q_u8_x4(d + at, a);
            vst1q_u8_x4(d + at + 64, b);
        }
#endif
        for (; at + 32 <= n; at += 32) {
            std::memcpy(d + at, s + at, 32);
        }
        if (at < n) {
            std::memcpy(d + n - 32, s + n - 32, 32);
        }
    }

    // A fill of 32 bytes and more: to FillOwnUpTo two overlapping blocks of
    // 32, from there libc
    SGCL_NOINLINE inline void fill_long(unsigned char* d, unsigned char value, size_t n) noexcept {
        if (n <= FillOwnUpTo) {
            std::memset(d, value, 32);
            std::memset(d + n - 32, value, 32);
            return;
        }
        std::memset(d, value, n);
    }

#if defined(SGCL_BYTES_ZVA)
    // Zeroing by whole 64-byte blocks with dc zva, the unaligned head and
    // tail by plain stores; libc's memset when DCZID_EL0 says the
    // instruction is prohibited or its block is not 64 bytes. n is at
    // least ZeroLinesFrom, so there are many blocks.
    SGCL_NOINLINE inline void zero_lines(unsigned char* d, size_t n) noexcept {
        uint64_t id;
        __asm__ volatile("mrs %0, dczid_el0" : "=r"(id));
        if ((id & 31) != 4) {
            std::memset(d, 0, n);
            return;
        }
        unsigned char* end = d + n;
        auto p = (unsigned char*)(((uintptr_t)d + 63) & ~uintptr_t(63));
        auto last = (unsigned char*)((uintptr_t)end & ~uintptr_t(63));
        std::memset(d, 0, 64);
        for (; p < last; p += 64) {
            __asm__ volatile("dc zva, %0" : : "r"(p) : "memory");
        }
        std::memset(end - 64, 0, 64);
    }

    // fill_long for a zero the compiler sees: the same, and the zero lines
    // in [ZeroLinesFrom, ZeroLinesBelow)
    SGCL_NOINLINE inline void zero_long(unsigned char* d, size_t n) noexcept {
        if (n <= FillOwnUpTo) {
            std::memset(d, 0, 32);
            std::memset(d + n - 32, 0, 32);
            return;
        }
        if (n - ZeroLinesFrom < ZeroLinesBelow - ZeroLinesFrom) {
            zero_lines(d, n);
            return;
        }
        std::memset(d, 0, n);
    }
#endif

    // A move of more than 32 bytes: to MoveOwnUpTo every block read into a
    // register before any is written, so an overlap either way cannot lose
    // a byte; from there, and without 128-bit registers, memmove
    SGCL_NOINLINE inline void move_long(unsigned char* d, const unsigned char* s, size_t n) noexcept {
#if defined(SGCL_BYTES_NEON) || defined(SGCL_BYTES_SSE2)
        if (n > MoveOwnUpTo) {
            std::memmove(d, s, n);
            return;
        }
#if defined(SGCL_BYTES_NEON)
        using Block = uint8x16_t;
        auto load = [](const unsigned char* p) noexcept { return vld1q_u8(p); };
        auto store = [](unsigned char* p, Block b) noexcept { vst1q_u8(p, b); };
#else
        using Block = __m128i;
        auto load = [](const unsigned char* p) noexcept { return _mm_loadu_si128((const __m128i*)p); };
        auto store = [](unsigned char* p, Block b) noexcept { _mm_storeu_si128((__m128i*)p, b); };
#endif
        if (n <= 64) {
            Block a = load(s), b = load(s + 16), c = load(s + n - 32), e = load(s + n - 16);
            store(d, a);
            store(d + 16, b);
            store(d + n - 32, c);
            store(d + n - 16, e);
            return;
        }
        Block a = load(s), b = load(s + 16), c = load(s + 32), e = load(s + 48);
        Block f = load(s + n - 64), g = load(s + n - 48), h = load(s + n - 32), i = load(s + n - 16);
        store(d, a);
        store(d + 16, b);
        store(d + 32, c);
        store(d + 48, e);
        store(d + n - 64, f);
        store(d + n - 48, g);
        store(d + n - 32, h);
        store(d + n - 16, i);
#else
        std::memmove(d, s, n);
#endif
    }

    inline void copy_bytes(void* to, const void* from, size_t n) noexcept {
        auto d = (unsigned char*)to;
        auto s = (const unsigned char*)from;
        if (n >= 32) {
            if (__builtin_constant_p(n) && n >= CopyLibcFrom) {
                std::memcpy(d, s, n);
            } else {
                copy_long(d, s, n);
            }
            return;
        }
        if (n >= 16) {
            std::memcpy(d, s, 16);
            std::memcpy(d + n - 16, s + n - 16, 16);
        } else if (n >= 8) {
            std::memcpy(d, s, 8);
            std::memcpy(d + n - 8, s + n - 8, 8);
        } else if (n >= 4) {
            std::memcpy(d, s, 4);
            std::memcpy(d + n - 4, s + n - 4, 4);
        } else if (n) {
            d[0] = s[0];
            d[n / 2] = s[n / 2];
            d[n - 1] = s[n - 1];
        }
    }

    inline void fill_bytes(void* to, unsigned char value, size_t n) noexcept {
        auto d = (unsigned char*)to;
        if (n >= 32) {
            if (__builtin_constant_p(n) && n > FillOwnUpTo) {
#if defined(SGCL_BYTES_ZVA)
                if (__builtin_constant_p(value) && value == 0 && n - ZeroLinesFrom < ZeroLinesBelow - ZeroLinesFrom) {
                    zero_lines(d, n);
                    return;
                }
#endif
                std::memset(d, value, n);
                return;
            }
#if defined(SGCL_BYTES_ZVA)
            if (__builtin_constant_p(value) && value == 0) {
                zero_long(d, n);
                return;
            }
#endif
            fill_long(d, value, n);
            return;
        }
        uint64_t w = 0x0101010101010101ull * value;
        if (n >= 16) {
            std::memcpy(d, &w, 8);
            std::memcpy(d + 8, &w, 8);
            std::memcpy(d + n - 16, &w, 8);
            std::memcpy(d + n - 8, &w, 8);
        } else if (n >= 8) {
            std::memcpy(d, &w, 8);
            std::memcpy(d + n - 8, &w, 8);
        } else if (n >= 4) {
            std::memcpy(d, &w, 4);
            std::memcpy(d + n - 4, &w, 4);
        } else if (n) {
            d[0] = value;
            d[n / 2] = value;
            d[n - 1] = value;
        }
    }

    // The same where the two runs may overlap. Every block is read into a
    // register before any of them is written, so an overlap cannot lose a
    // byte; past MoveOwnUpTo the run is memmove's.
    inline void move_bytes(void* to, const void* from, size_t n) noexcept {
        auto d = (unsigned char*)to;
        auto s = (const unsigned char*)from;
        if (n > 32) {
            if (__builtin_constant_p(n) && n > MoveOwnUpTo) {
                std::memmove(d, s, n);
            } else {
                move_long(d, s, n);
            }
            return;
        }
        if (n >= 16) {
            unsigned char head[16];
            unsigned char tail[16];
            std::memcpy(head, s, 16);
            std::memcpy(tail, s + n - 16, 16);
            std::memcpy(d, head, 16);
            std::memcpy(d + n - 16, tail, 16);
        } else if (n >= 8) {
            uint64_t head;
            uint64_t tail;
            std::memcpy(&head, s, 8);
            std::memcpy(&tail, s + n - 8, 8);
            std::memcpy(d, &head, 8);
            std::memcpy(d + n - 8, &tail, 8);
        } else if (n >= 4) {
            uint32_t head;
            uint32_t tail;
            std::memcpy(&head, s, 4);
            std::memcpy(&tail, s + n - 4, 4);
            std::memcpy(d, &head, 4);
            std::memcpy(d + n - 4, &tail, 4);
        } else if (n) {
            unsigned char a = s[0];
            unsigned char b = s[n / 2];
            unsigned char c = s[n - 1];
            d[0] = a;
            d[n / 2] = b;
            d[n - 1] = c;
        }
    }
}
