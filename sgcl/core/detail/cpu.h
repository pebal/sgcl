//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The instructions above each family's minimum, asked of the processor once
// per process: the gates of every fast path in the library (crypto, hash,
// compress, codec). The rule (DESIGN 297, .agent/cpu-baseline.md):
//
//   - the minimum is what a compiler assumes with no flag (x86-64: SSE2;
//     arm64: ARMv8.0-A with NEON); code on it needs no gate;
//   - everything above is compiled always, each function marked with what
//     its body needs (__attribute__((target(...))), the SGCL_TARGET_*
//     macros below), and chosen at run time by a gate here: a static const
//     bool from one query, asked at the first call and never again;
//   - where the target itself promises the features (__ARM_FEATURE_AES,
//     __AVX2__… — the default target of macOS on Apple silicon promises
//     every arm64 one), a gate is a constant and the choice folds away at
//     compile time.
//
// Every translation unit compiles the same bodies whatever its flags, so a
// program may mix units built with different -march (no two bodies of one
// inline function). A gate is asked after a path's length check, never on a
// short path; a fast path's whole loop sits inside its target function.
//
// Four gates a family, no more:
//
//   x86-64  wide     SSSE3, SSE4.1, SSE4.2, POPCNT, AVX2, BMI1, BMI2, LZCNT,
//                    and the system saving the YMM registers (Haswell 2013+,
//                    Zen 2017+)
//           aes      AES-NI, PCLMULQDQ, SSSE3, SSE4.1
//           sha      SHA-NI, SSSE3, SSE4.1
//           avx512   F, BW, VL, VAES, VPCLMULQDQ and the ZMM state (no path
//                    uses it until a measurement says so)
//   arm64   crypto   FEAT_AES, FEAT_PMULL, FEAT_SHA1, FEAT_SHA256, CRC32
//           sha512   FEAT_SHA512
//           sha3     FEAT_SHA3
//
// The queries: cpuid (leaves 1, 7 and 0x80000001) and XGETBV on x86-64;
// sysctlbyname("hw.optional...") on macOS and getauxval(AT_HWCAP) on Linux
// for arm64. Only the compiler's own headers are included (<cpuid.h>), and
// sysctlbyname is declared here, as <sys/sysctl.h> would bring <sys/proc.h>
// and its `struct user` into every program (the rule for platform headers).
// Elsewhere (MSVC, other systems until the platform step) every gate above
// the minimum is false.
#include <cstddef>
#include <cstdint>
#include <initializer_list>

#if defined(__aarch64__) && defined(__AARCH64EL__) && (defined(__clang__) || defined(__GNUC__))
#define SGCL_CPU_ARM64 1
#if defined(__APPLE__)
extern "C" int sysctlbyname(const char*, void*, size_t*, void*, size_t);
#elif defined(__linux__)
#include <sys/auxv.h>
#endif
#elif defined(__x86_64__) && (defined(__clang__) || defined(__GNUC__))
#define SGCL_CPU_X86 1
#include <cpuid.h>
#endif

// What a function's body needs, where the target may not have it; the
// _INLINE forms for the helpers and lambdas a path unrolls its rounds with,
// which must melt into the function around them so that the state stays in
// registers (a helper left as a call takes its operands through memory)
#if defined(SGCL_CPU_ARM64)
#define SGCL_TARGET_ARM64_CRYPTO __attribute__((target("+aes,+sha2,+crc")))
#define SGCL_INLINE_ARM64_CRYPTO __attribute__((target("+aes,+sha2,+crc"), always_inline))
// (clang ties the SHA-512 instructions to +sha3: SHA-512's body keeps to
// add, ext and the SHA512* intrinsics, none of FEAT_SHA3's own, EOR3,
// RAX1, XAR and BCAX, which a machine with SHA-512 but not SHA-3 lacks,
// and its gate is sha512())
#define SGCL_TARGET_ARM64_SHA3 __attribute__((target("+sha3")))
#define SGCL_INLINE_ARM64_SHA3 __attribute__((target("+sha3"), always_inline))
#endif
#if defined(SGCL_CPU_X86)
#define SGCL_TARGET_X86_WIDE __attribute__((target("avx2,bmi,bmi2,lzcnt,popcnt,sse4.2")))
#define SGCL_INLINE_X86_WIDE __attribute__((target("avx2,bmi,bmi2,lzcnt,popcnt,sse4.2"), always_inline))
#define SGCL_TARGET_X86_AES __attribute__((target("aes,pclmul,ssse3,sse4.1")))
#define SGCL_INLINE_X86_AES __attribute__((target("aes,pclmul,ssse3,sse4.1"), always_inline))
#define SGCL_TARGET_X86_SHA __attribute__((target("sha,ssse3,sse4.1")))
#define SGCL_INLINE_X86_SHA __attribute__((target("sha,ssse3,sse4.1"), always_inline))
#endif

namespace sgcl::detail::cpu {
#if defined(SGCL_CPU_ARM64)
    // One feature asked of the system: its name in sysctl on macOS, its bit
    // of AT_HWCAP on Linux; false anywhere else
    inline bool query([[maybe_unused]] const char* apple_name, [[maybe_unused]] unsigned long hwcap_bits) noexcept {
#if defined(__APPLE__)
        int value = 0;
        size_t size = sizeof value;
        return ::sysctlbyname(apple_name, &value, &size, nullptr, 0) == 0 && value != 0;
#elif defined(__linux__)
        return (::getauxval(AT_HWCAP) & hwcap_bits) == hwcap_bits;
#else
        return false;
#endif
    }

    // AESE/AESD/AESMC/AESIMC, PMULL, SHA1*, SHA256*, CRC32*
    inline bool crypto() noexcept {
#if defined(__ARM_FEATURE_AES) && defined(__ARM_FEATURE_SHA2) && defined(__ARM_FEATURE_CRC32)
        return true;
#else
        // HWCAP_AES, HWCAP_PMULL, HWCAP_SHA1, HWCAP_SHA2, HWCAP_CRC32: bits 3 to 7
        static const bool has = query("hw.optional.arm.FEAT_AES", 1ul << 3) && query("hw.optional.arm.FEAT_PMULL", 1ul << 4) &&
                                query("hw.optional.arm.FEAT_SHA1", 1ul << 5) && query("hw.optional.arm.FEAT_SHA256", 1ul << 6) &&
                                query("hw.optional.armv8_crc32", 1ul << 7);
        return has;
#endif
    }

    // SHA512H/H2 and SHA512SU0/SU1
    inline bool sha512() noexcept {
#if defined(__ARM_FEATURE_SHA512)
        return true;
#else
        static const bool has = query("hw.optional.arm.FEAT_SHA512", 1ul << 21);   // HWCAP_SHA512
        return has;
#endif
    }

    // EOR3, RAX1, XAR and BCAX, the four that Keccak takes
    inline bool sha3() noexcept {
#if defined(__ARM_FEATURE_SHA3)
        return true;
#else
        static const bool has = query("hw.optional.arm.FEAT_SHA3", 1ul << 17);   // HWCAP_SHA3
        return has;
#endif
    }
#endif

#if defined(SGCL_CPU_X86)
    // The registers of one cpuid leaf (and subleaf); zeros past the
    // processor's last leaf
    struct CpuidRegisters {
        unsigned a = 0, b = 0, c = 0, d = 0;
    };

    inline CpuidRegisters cpuid(unsigned leaf, unsigned subleaf = 0) noexcept {
        CpuidRegisters r;
        if (__get_cpuid_max(leaf & 0x80000000u, nullptr) >= leaf) {
            __cpuid_count(leaf, subleaf, r.a, r.b, r.c, r.d);
        }
        return r;
    }

    // The register state the system saves on a switch (XCR0): bit 1 XMM,
    // 2 YMM, 5-7 the AVX-512 state; 0 when the system does not say
    // (OSXSAVE clear)
    inline uint64_t saved_state() noexcept {
        if (!(cpuid(1).c & (1u << 27))) {
            return 0;
        }
        unsigned lo, hi;
        __asm__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        return uint64_t(hi) << 32 | lo;
    }

    inline bool bits(unsigned reg, std::initializer_list<unsigned> positions) noexcept {
        for (unsigned p : positions) {
            if (!(reg & (1u << p))) {
                return false;
            }
        }
        return true;
    }

    // SSSE3, SSE4.1, SSE4.2, POPCNT, AVX2, BMI1, BMI2, LZCNT, YMM saved
    inline bool wide() noexcept {
#if defined(__AVX2__) && defined(__BMI__) && defined(__BMI2__) && defined(__LZCNT__) && defined(__POPCNT__) && defined(__SSE4_2__)
        return true;
#else
        static const bool has = bits(cpuid(1).c, {9, 19, 20, 23, 28}) && bits(cpuid(7).b, {3, 5, 8}) && bits(cpuid(0x80000001u).c, {5}) &&
                                (saved_state() & 0x6) == 0x6;
        return has;
#endif
    }

    // AES-NI, PCLMULQDQ, SSSE3, SSE4.1
    inline bool aes() noexcept {
#if defined(__AES__) && defined(__PCLMUL__) && defined(__SSE4_1__)
        return true;
#else
        static const bool has = bits(cpuid(1).c, {1, 9, 19, 25});
        return has;
#endif
    }

    // SHA-NI, SSSE3, SSE4.1
    inline bool sha() noexcept {
#if defined(__SHA__) && defined(__SSE4_1__)
        return true;
#else
        static const bool has = bits(cpuid(7).b, {29}) && bits(cpuid(1).c, {9, 19});
        return has;
#endif
    }

    // AVX-512 F, BW, VL, VAES, VPCLMULQDQ, the ZMM state saved
    inline bool avx512() noexcept {
#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__) && defined(__VAES__) && defined(__VPCLMULQDQ__)
        return true;
#else
        static const bool has = bits(cpuid(7).b, {16, 30, 31}) && bits(cpuid(7).c, {9, 10}) && (saved_state() & 0xE6) == 0xE6;
        return has;
#endif
    }
#endif
}
