//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/ec_keys.h"

// NIST P-256 (secp256r1, prime256v1; FIPS 186-5, SP 800-186 §3.2.1.3):
// ECDSA signatures and ECDH key agreement, Go's crypto/ecdsa and
// crypto/ecdh over elliptic.P256(). The curve of TLS, of WebAuthn and
// passkeys, of JWS's ES256, of most certificates signed with ECDSA.
//
//   auto key = crypto::p256::private_key::generate();
//   auto sig = key.sign_digest(crypto::sha256::of(message));
//   bool ok = key.public_key().verify_digest(crypto::sha256::of(message), sig);
//
// A private key is for ECDSA and an ecdh_key for key agreement, apart as
// in Go (to_ecdh() gives the one from the other); both are move-only and
// zero their scalar when they go. A public key is a plain value, the peer
// of an ECDH key and the verifier of a signature. Everything that touches
// a private scalar runs in constant time (detail/ec_curve.h). The
// implementation has not been through an independent cryptographic audit.
namespace sgcl::crypto::p256 {
    using public_key = detail::EcPublicKey<detail::P256>;
    using private_key = detail::EcdsaPrivateKey<detail::P256>;
    using ecdh_key = detail::EcdhKey<detail::P256>;
}
