//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The hash module (namespace sgcl::hash): checksums and hashes that are not
// cryptographic, Go's hash/crc32, hash/crc64, hash/adler32, hash/fnv and
// hash/maphash, and XXH3 and SipHash-2-4.
// Every algorithm is one type with the same methods (mixin/hasher.h).
// README: docs/sgcl/hash/README.md
#pragma once

#include "adler32.h"
#include "crc32.h"
#include "crc64.h"
#include "fnv.h"
#include "maphash.h"
#include "mixin/hasher.h"
#include "siphash.h"
#include "xxh3.h"
