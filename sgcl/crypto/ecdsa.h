//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "p256.h"
#include "p384.h"

// ECDSA (FIPS 186-5 §6) over the NIST curves, Go's crypto/ecdsa: the keys
// are p256::private_key and p384::private_key, their public keys verify.
// Signing takes a digest the program made (sign_digest), not a message:
// ECDSA signs any digest, its leftmost bits as many as the curve's order
// has; the signature is DER (Go's SignASN1) or r || s (sign_digest_raw).
// k is RFC 6979's, hedged with random bytes as Go does it.
// The implementation has not been through an independent cryptographic
// audit. README: docs/sgcl/crypto/ecdsa.md
