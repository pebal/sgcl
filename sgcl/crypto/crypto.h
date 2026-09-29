//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The crypto module (namespace sgcl::crypto): Go's crypto/* written from
// the specifications — digests (SHA-1, SHA-2, SHA-3, SHAKE), HMAC, HKDF,
// PBKDF2, random bytes from the system, constant-time comparison; the
// ciphers AES (GCM, CTR) and ChaCha20 (Poly1305, XChaCha20); X25519 and
// Ed25519; ML-KEM (FIPS 203); ECDSA and ECDH on P-256 and P-384; RSA (PKCS #1 v1.5 and PSS
// signatures, OAEP); X.509 certificates and the verification of their
// chains. The implementation has not been through an independent
// cryptographic audit.
// README: docs/sgcl/crypto/README.md
#pragma once

#include "aes.h"
#include "chacha20.h"
#include "chacha20_poly1305.h"
#include "constant_time.h"
#include "ctr.h"
#include "ecdsa.h"
#include "ed25519.h"
#include "error.h"
#include "gcm.h"
#include "hash_id.h"
#include "hkdf.h"
#include "hmac.h"
#include "mlkem.h"
#include "nonce_counter.h"
#include "p256.h"
#include "p384.h"
#include "pbkdf2.h"
#include "random.h"
#include "read_secret.h"
#include "rsa.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha1.h"
#include "sha256.h"
#include "sha3.h"
#include "sha512.h"
#include "x25519.h"
#include "x509.h"
