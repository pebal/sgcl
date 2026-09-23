//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

// Copying, filling and moving a run of bytes whose length the compiler
// does not know. A memcpy of a length it does know is not a call at all
// but the move it would emit anyway — `ldr q0` / `str q0` for sixteen
// bytes and `ldp q0, q1` / `stp q0, q1` for thirty-two, which is what an
// intrinsic would give on every target and with no #ifdef — so every
// memcpy below has a constant length and none of them is a call.
//
// Short runs are most of what the library does, and there libc is dear:
// it dispatches on the length inside, with branches that a mixture of
// them mispredicts. Where it shows is not a microbenchmark but the work
// itself — making two million strings, which is a copy into a slot the
// allocator has just handed out:
//
//   characters   memcpy   here
//           20    17.7    12.0
//           40    20.7    13.9
//           60    22.2    15.5
//          200    29.7    27.6
//
// A run under thirty-two bytes goes as two blocks that overlap in the
// middle, which touches nothing outside either object: the end block
// starts at n - w, so it never passes the last byte. Longer runs go to
// copy_wide and fill_wide, which are libc's business nowhere.
//
// Two things about the shape are worth writing down, because a
// microbenchmark says otherwise about both.
//
// The destination is cold in the work that matters, and that changes
// which shape wins. Into a buffer already in cache, a ladder of fixed
// widths beats a loop four times over at forty bytes; into a fresh slot
// the two are level. The cache miss is most of the cost and the shape
// only what is left.
//
// And the wide loop is kept out of line. Four q registers in flight each
// way is what keeps the loads and the stores overlapping — one pair does
// not — but it is also a dozen instructions that every call site would
// otherwise carry: inlined everywhere it took a padded field in format
// from 15.8 ns to 17.2 and three of them from 47.7 to 54.1, because the
// caller grew past what the compiler will keep in registers.
//
// Above a few hundred bytes none of this matters much: the copy is bound
// by memory and every reasonable shape converges. Measured against libc
// from four kilobytes to a megabyte the two trade places from run to run
// depending on where the buffers fall, so there is nothing there to win
// and nothing to lose by keeping our own.
namespace sgcl::detail {
    // The wide part, kept out of line on purpose. It is four q registers
    // in flight each way, which is what keeps the loads and the stores
    // overlapping — one pair does not — but it is also a dozen
    // instructions that every call site would otherwise carry, and that
    // is dear where the run is short: inlined everywhere, it took a
    // padded field in format from 15.3 ns to 17.2 and three of them from
    // 45.7 to 54.1, because the caller grew past what the compiler will
    // keep in registers.
    __attribute__((noinline)) inline void copy_wide(unsigned char* d, const unsigned char* s, size_t n) noexcept {
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

    __attribute__((noinline)) inline void fill_wide(unsigned char* d, unsigned char value, size_t n) noexcept {
        uint64_t w = 0x0101010101010101ull * value;
        size_t at = 0;
#if defined(__ARM_NEON)
        uint8x16_t v = vdupq_n_u8(value);
        uint8x16x4_t q = {v, v, v, v};
        for (; at + 128 <= n; at += 128) {
            vst1q_u8_x4(d + at, q);
            vst1q_u8_x4(d + at + 64, q);
        }
#endif
        for (; at + 32 <= n; at += 32) {
            std::memcpy(d + at, &w, 8);
            std::memcpy(d + at + 8, &w, 8);
            std::memcpy(d + at + 16, &w, 8);
            std::memcpy(d + at + 24, &w, 8);
        }
        if (at < n) {
            unsigned char* end = d + n - 32;
            std::memcpy(end, &w, 8);
            std::memcpy(end + 8, &w, 8);
            std::memcpy(end + 16, &w, 8);
            std::memcpy(end + 24, &w, 8);
        }
    }

    inline void copy_bytes(void* to, const void* from, size_t n) noexcept {
        auto d = (unsigned char*)to;
        auto s = (const unsigned char*)from;
        if (n >= 32) {
            copy_wide(d, s, n);
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
            fill_wide(d, value, n);
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
    // byte; past what the registers hold the question is libc's again.
    inline void move_bytes(void* to, const void* from, size_t n) noexcept {
        auto d = (unsigned char*)to;
        auto s = (const unsigned char*)from;
        if (n > 32) {
            std::memmove(d, s, n);
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
