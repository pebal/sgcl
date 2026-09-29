//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The vector roads of the codec module: 128-bit vectors on what the
// compiler assumes without a flag, NEON on arm64 and SSE2 on x86-64 (the
// minimum of each family: no gate). Every kernel on them has a plain loop
// beside it; SGCL_CODEC_PORTABLE takes the vector roads out, so that the
// tests check both on the same files, and the two give the same bytes, bit
// for bit.
#if !defined(SGCL_CODEC_PORTABLE) && defined(__aarch64__) && defined(__AARCH64EL__) && defined(__ARM_NEON)
#define SGCL_CODEC_NEON 1
#include <arm_neon.h>
#elif !defined(SGCL_CODEC_PORTABLE) && (defined(__SSE2__) || defined(_M_X64)) && !defined(__BIG_ENDIAN__)
#define SGCL_CODEC_SSE2 1
#include <emmintrin.h>
#endif
