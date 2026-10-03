[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::ecdsa

```cpp
#include "sgcl/crypto/ecdsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    struct deterministic_t {
        explicit deterministic_t() = default;
    };
    inline constexpr deterministic_t deterministic{};

    namespace p256 {
        using private_key = /* unspecified */;
        using public_key = /* unspecified */;
    }

    namespace p384 {
        using private_key = /* unspecified */;
        using public_key = /* unspecified */;
    }
}
```

ECDSA (FIPS 186-5 §6), Go's `crypto/ecdsa`, over the NIST curves [P-256](p256.md) and [P-384](p384.md): a
[private_key](p256-private_key/README.md) signs a digest, its [public_key](p256-public_key/README.md) verifies. This page is how
signing and verifying work, on both curves; the keys, their formats and their members are on the pages of the
classes. The header `ecdsa.h` brings both curves; the tag `deterministic` comes with either
curve's header.

Signing takes a digest the program made, not a message: [sign_digest](p256-private_key/sign_digest.md) gives the
signature in DER, Go's `SignASN1`, and [sign_digest_raw](p256-private_key/sign_digest_raw.md) as r ‖ s;
[verify_digest](p256-public_key/verify_digest.md) and [verify_digest_raw](p256-public_key/verify_digest_raw.md) check
them. The nonce k is RFC 6979's, hedged with random bytes as Go makes it. **The implementation has not been through
an independent cryptographic audit.**

## Rules

- **A digest, not a message**: `sign_digest(digest)` signs what the program hashed — `sha256::of(message)` for ES256,
  `sha384::of(message)` for ES384, a certificate's TBS digest by the algorithm the certificate names
  ([hash_id](hash_id.md)). ECDSA takes a digest of any length of one byte or more and uses its leftmost bits, as many
  as the curve's order has (256 or 384), as FIPS 186-5 §6.4.1 has it: a longer digest is cut, a shorter one is the
  number it is. The length is not checked against any hash, as neither Go nor OpenSSL checks it; pair the curve with
  the digest the protocol names. An empty digest is `std::invalid_argument` and verifies nothing, as Go has it: a
  signature of nothing is a program's mistake (the arguments swapped, a digest never made).
- **Two encodings of a signature**: `sign_digest` gives the DER `ECDSA-Sig-Value` (a SEQUENCE of two INTEGERs, what
  X.509, TLS and Go's `SignASN1` use; at most 72 bytes for P-256, 104 for P-384, `max_signature_size`),
  `sign_digest_raw` the fixed-size r ‖ s (IEEE P1363: JWS, WebAuthn, PKCS #11, COSE; 64 or 96 bytes,
  `signature_size`). `verify_digest` takes only DER and `verify_digest_raw` only r ‖ s: a signature in the other form
  is false.
- **The nonce k is RFC 6979's, hedged**: an HMAC-DRBG (SHA-256 for P-256, SHA-384 for P-384) over the private key,
  the reduced digest and fresh random bytes (RFC 6979 §3.6), as Go makes it. If the system's random bytes were ever
  bad, k is still unpredictable and never repeats for two digests (the failure that exposed the keys of the
  PlayStation 3 and of Android Bitcoin wallets); when they are good, two signatures of the same digest differ.
- **The deterministic signature** is RFC 6979 without the random bytes, asked for by the tag:

  | Name | Description |
  |---|---|
  | `deterministic_t` | the type of the tag, with an explicit default constructor, so that `{}` is never taken for it |
  | `deterministic` | the tag: `sign_digest(digest, crypto::deterministic)`, and the same for `sign_digest_raw` |

  The same key and digest always give the same signature, byte for byte OpenSSL's, its HMAC the hash that made the
  digest. Without a [hash_id](hash_id.md) the hash is taken from the digest's length: SHA-1, SHA-224, SHA-256,
  SHA-384, SHA-512 for 20, 28, 32, 48, 64 bytes, the curve's own hash for any other; for SHA-3 and SHA-512/256, whose
  lengths are SHA-2's, name it — `sign_digest(digest, crypto::deterministic, hash_id::sha3_256)` — or the signature
  is valid but not the one every other implementation makes (a digest of another length than the named hash's is
  `std::invalid_argument`). It is for test vectors and for protocols that ask for it: it tells whoever sees two
  signatures whether the digests were equal, and a fault of the hardware while signing is not hidden by fresh random
  bytes as the hedged k hides it, so the hedged form stays the default.
- **Constant time**: everything that touches d, k and k⁻¹ runs in constant time: the base-point multiplication scans
  its tables through masks, the inverse of k is Fermat's. The digest, the signature, r and the public key are public.
- **Verification is strict and never throws**: r and s must be in [1, n − 1], the DER must be DER (the shortest
  lengths and integers, no negative numbers, nothing after the sequence), and x(u₁G + u₂Q) mod n must equal r.
  Anything else is false. Both `verify_digest` and `verify_digest_raw` are `[[nodiscard]]`: a check whose result is
  dropped was never made.
- **Malleability**: as in every ECDSA, (r, n − s) verifies wherever (r, s) does. A protocol that uses a signature's
  bytes as an identifier must not rely on them being unique (Bitcoin's low-S rule is the protocol's, not ECDSA's).
- **Signing a message someone else chose** is safe: the digest is not secret. Signing the same digest twice is safe
  too (the hedged k differs).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the signer keeps its key as PKCS#8 and publishes the public key as SPKI
    auto stored = crypto::p384::private_key::generate().to_pkcs8_der();
    auto key = crypto::p384::private_key::from_pkcs8_der(stored);
    if (!key) {
        eprintln(key.error().message());
        return 1;
    }
    auto published = key->public_key().to_pkix_der();

    // a JWS-style signature: r || s of a SHA-384 digest (ES384)
    auto digest = crypto::sha384::of("{\"sub\":\"1234567890\"}");
    auto sig = key->sign_digest_raw(digest);

    // the verifier reads the published key and checks the signature
    auto pub = crypto::p384::public_key::from_pkix_der(published);
    println(pub && pub->verify_digest_raw(digest, sig) ? "valid" : "forged");
    sig[10] ^= byte(1);
    println(pub && pub->verify_digest_raw(digest, sig) ? "valid" : "forged");

    // a point off the curve is not a key
    array<byte, 97> point{};
    point[0] = byte(4);  // 04 || X = 0 || Y = 0
    auto bad = crypto::p384::public_key::from_bytes(point);
    println(bad ? "a key" : bad.error().message());
}
```

Output:

```text
valid
forged
sgcl::crypto::p384: the point is not on the curve
```

## See also

- [p256::private_key](p256-private_key/README.md): signing, the key's formats
- [p256::public_key](p256-public_key/README.md): verifying
- [p256](p256.md), [p384](p384.md): the curves
- [hash_id](hash_id.md): a digest named when the program runs
- [sha256](sha256/README.md), [sha512](sha512/README.md) (SHA-384), [error](error/README.md)
- [ed25519](ed25519.md), [rsa](rsa.md): the other signatures of the module
