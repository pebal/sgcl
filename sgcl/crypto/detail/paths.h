//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/cpu.h"

// The paths of the module. Every algorithm with a fast path has a body in
// portable C++ beside it, which SGCL_CRYPTO_PORTABLE leaves alone: for the
// tests, which run every vector on both, and for a program that wants the
// one path everywhere.
//
//   SGCL_CRYPTO_ARM64  the paths on arm64's cryptographic instructions
//                      (AESE/AESD, PMULL, SHA1/SHA256, SHA512, SHA3),
//                      compiled on every little-endian arm64 target, each
//                      function marked with what it needs, and chosen at
//                      run time by the gates of sgcl/core/detail/cpu.h
//                      (cpu::crypto(), sha512(), sha3()): constants where
//                      the target promises the features, as macOS's does
//   SGCL_CRYPTO_NEON   ChaCha20 on NEON, which is in arm64's minimum: no gate
//   SGCL_CRYPTO_X86    the paths of x86-64: AES-NI and PCLMULQDQ (cpu::aes()),
//                      SHA-NI (cpu::sha()), AVX2 (cpu::wide()), each
//                      function marked with what it needs; and ChaCha20 on
//                      SSE2, which is in x86-64's minimum: no gate
//
// A cipher's path is chosen with its key, from the processor alone, and
// kept in the key (AesEncryptKey::path): never from the key's value.
#if defined(SGCL_CPU_ARM64) && defined(__ARM_NEON) && !defined(SGCL_CRYPTO_PORTABLE)
#define SGCL_CRYPTO_ARM64 1
#define SGCL_CRYPTO_NEON 1
#include <arm_neon.h>
#elif defined(SGCL_CPU_X86) && !defined(SGCL_CRYPTO_PORTABLE)
#define SGCL_CRYPTO_X86 1
#include <immintrin.h>
#endif
