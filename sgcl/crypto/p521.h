//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/ec_keys.h"

// NIST P-521 (secp521r1; FIPS 186-5, SP 800-186 §3.2.1.5): ECDSA and ECDH
// as p256.h has them, on the largest of the NIST curves, 256 bits of
// security; its digest is SHA-512 by convention (ES512). A coordinate and
// a scalar are 66 bytes (521 bits), a raw signature 132, an uncompressed
// point 133. The same types, the same rules: a private key signs, an
// ecdh_key agrees, both move-only and zeroed when they go, a public key a
// plain value. The implementation has not been through an independent
// cryptographic audit.
namespace sgcl::crypto::p521 {
    using public_key = detail::EcPublicKey<detail::P521>;
    using private_key = detail::EcdsaPrivateKey<detail::P521>;
    using ecdh_key = detail::EcdhKey<detail::P521>;
}
