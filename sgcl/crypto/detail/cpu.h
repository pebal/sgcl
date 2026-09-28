//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// Which cryptographic instructions of ARMv8 the processor has, for the
// paths of the digests. The paths are compiled on every little-endian arm64
// target, each function marked with the feature it needs
// (__attribute__((target("+sha2"))) and so on), so that a build for a plain
// armv8-a still carries them; which one runs is decided here:
//
//   - where the target itself promises the feature (__ARM_FEATURE_SHA2,
//     __ARM_FEATURE_SHA512, __ARM_FEATURE_SHA3 — the default target of
//     macOS on Apple silicon promises all three), the answer is a constant
//     and the choice folds away at compile time;
//   - otherwise the processor is asked once, at the first call, and the
//     answer kept: sysctlbyname("hw.optional.arm.FEAT_...") on macOS,
//     getauxval(AT_HWCAP) on Linux. SHA-512 and SHA-3 are asked about
//     apart (FEAT_SHA512, FEAT_SHA3; HWCAP_SHA512, HWCAP_SHA3): the
//     architecture makes them two options, and a processor may have one.
//
// SGCL_CRYPTO_PORTABLE takes every path out, leaving the plain C++ of each
// algorithm: for the tests, which run every vector on both, and for a
// program that wants the one path everywhere. As with the hash module, a
// program is built with one setting of the macro and one target
// throughout: two translation units that disagree would give the inline
// functions two bodies.
#if defined(__aarch64__) && defined(__AARCH64EL__) && !defined(SGCL_CRYPTO_PORTABLE) && (defined(__clang__) || defined(__GNUC__))
#define SGCL_CRYPTO_ARM64 1

#include <arm_neon.h>

#if defined(__APPLE__)
#include <cstddef>
// sysctlbyname alone, as <sys/sysctl.h> declares it: the header itself
// brings <sys/proc.h> and its `struct user` into every program that
// includes the library
extern "C" int sysctlbyname(const char*, void*, size_t*, void*, size_t);
#elif defined(__linux__)
#include <sys/auxv.h>
#endif

// the feature a function's body needs, where the target may not have it;
// the _INLINE forms for the lambdas a path unrolls its rounds with, which
// must melt into the function around them so that the state stays in
// registers (a lambda left as a call takes its captures by address)
#define SGCL_CRYPTO_TARGET_SHA2 __attribute__((target("+sha2")))
// (clang ties the SHA-512 instructions to +sha3, so sha512_compress_arm64
// carries TARGET_SHA3 while its gate is cpu::sha512(): its body must keep
// to add, ext and the SHA512* intrinsics, none of FEAT_SHA3's own, EOR3,
// RAX1, XAR and BCAX, which a machine with SHA-512 but not SHA-3 lacks)
#define SGCL_CRYPTO_TARGET_SHA3 __attribute__((target("+sha3")))
#define SGCL_CRYPTO_INLINE_SHA2 __attribute__((target("+sha2"), always_inline))
#define SGCL_CRYPTO_INLINE_SHA3 __attribute__((target("+sha3"), always_inline))

namespace sgcl::crypto::detail::cpu {
    // One feature asked of the system: its name in sysctl on macOS, its bit
    // of AT_HWCAP on Linux; false anywhere else
    inline bool query([[maybe_unused]] const char* apple_name, [[maybe_unused]] unsigned long hwcap_bit) noexcept {
#if defined(__APPLE__)
        int value = 0;
        size_t size = sizeof value;
        return ::sysctlbyname(apple_name, &value, &size, nullptr, 0) == 0 && value != 0;
#elif defined(__linux__)
        return (::getauxval(AT_HWCAP) & hwcap_bit) != 0;
#else
        return false;
#endif
    }

    // SHA1C/P/M/H and SHA1SU0/SU1
    inline bool sha1() noexcept {
#if defined(__ARM_FEATURE_SHA2)
        return true;
#else
        static const bool has = query("hw.optional.arm.FEAT_SHA1", 1ul << 5);   // HWCAP_SHA1
        return has;
#endif
    }

    // SHA256H/H2 and SHA256SU0/SU1
    inline bool sha256() noexcept {
#if defined(__ARM_FEATURE_SHA2)
        return true;
#else
        static const bool has = query("hw.optional.arm.FEAT_SHA256", 1ul << 6);   // HWCAP_SHA2
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
}
#endif
