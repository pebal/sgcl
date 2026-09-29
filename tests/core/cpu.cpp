//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The gates of sgcl/core/detail/cpu.h: each a constant or one question kept
// (the same answer every time), and on the machines they are known on, the
// answer the machine gives. The run prints them.
#include <gtest/gtest.h>

#include "sgcl/core/detail/cpu.h"

#include <cstdio>

namespace cpu = sgcl::detail::cpu;

TEST(Core_Cpu, TheGates) {
#if defined(SGCL_CPU_ARM64)
    std::printf("[ gates    ] arm64: crypto %d sha512 %d sha3 %d\n", cpu::crypto(), cpu::sha512(), cpu::sha3());
    EXPECT_EQ(cpu::crypto(), cpu::crypto());
    EXPECT_EQ(cpu::sha512(), cpu::sha512());
    EXPECT_EQ(cpu::sha3(), cpu::sha3());
#if defined(__APPLE__)
    // every Apple silicon processor has the crypto extension; M1 and later
    // SHA-512 and SHA-3 too
    EXPECT_TRUE(cpu::crypto());
    EXPECT_TRUE(cpu::sha512());
    EXPECT_TRUE(cpu::sha3());
#endif
#elif defined(SGCL_CPU_X86)
    std::printf("[ gates    ] x86-64: wide %d aes %d sha %d avx512 %d\n", cpu::wide(), cpu::aes(), cpu::sha(), cpu::avx512());
    EXPECT_EQ(cpu::wide(), cpu::wide());
    EXPECT_EQ(cpu::aes(), cpu::aes());
    EXPECT_EQ(cpu::sha(), cpu::sha());
    EXPECT_EQ(cpu::avx512(), cpu::avx512());
    // the gates as cpuid says them, apart
    const auto l1 = cpu::cpuid(1), l7 = cpu::cpuid(7);
    EXPECT_EQ(cpu::aes(), bool(l1.c & (1u << 25)) && bool(l1.c & (1u << 1)) && bool(l1.c & (1u << 9)) && bool(l1.c & (1u << 19)));
    EXPECT_EQ(cpu::sha(), bool(l7.b & (1u << 29)) && bool(l1.c & (1u << 9)) && bool(l1.c & (1u << 19)));
    // AVX-512 is never there without AVX2
    EXPECT_TRUE(!cpu::avx512() || bool(l7.b & (1u << 5)));
#else
    std::printf("[ gates    ] none: every path the portable one\n");
    SUCCEED();
#endif
}
