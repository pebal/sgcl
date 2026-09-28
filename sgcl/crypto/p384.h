//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/ec_keys.h"

// NIST P-384 (secp384r1; FIPS 186-5, SP 800-186 §3.2.1.4): ECDSA and ECDH
// as p256.h has them, on the curve of CNSA and of certificates that want
// 192 bits of security; its digest is SHA-384 by convention (ES384).
// The same types, the same rules: a private key signs, an ecdh_key agrees,
// both move-only and zeroed when they go, a public key a plain value. The
// implementation has not been through an independent cryptographic audit.
namespace sgcl::crypto::p384 {
    using public_key = detail::EcPublicKey<detail::P384>;
    using private_key = detail::EcdsaPrivateKey<detail::P384>;
    using ecdh_key = detail::EcdhKey<detail::P384>;
}
