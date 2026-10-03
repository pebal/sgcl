[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md)

# sgcl::crypto::p256::private_key

```cpp
#include "sgcl/crypto/p256.h"   // or "sgcl/crypto.h"
#include "sgcl/crypto/p384.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::p256 {
    using private_key = /* unspecified */;
}

namespace sgcl::crypto::p384 {
    using private_key = /* unspecified */;
}
```

`sgcl::crypto::p256::private_key` is an [ECDSA](../ecdsa.md) key of the curve P-256 (FIPS 186-5 §6), Go's
`ecdsa.PrivateKey`: the secret scalar d and the public point d·G, kept so that
[public_key](public_key.md) costs nothing. It [signs](sign_digest.md) digests, and
its [public_key](../p256-public_key/README.md) verifies them. A key is made by [generate](generate.md) or read
from its scalar, from PKCS #8, from SEC 1 or from PEM; it is written back in the same forms.

The key is for signing alone, as in Go: an agreement takes an [ecdh_key](../p256-ecdh_key/README.md), which
[to_ecdh](to_ecdh.md) makes from the same scalar for a protocol that needs both. `p384::private_key`
is the same class on P-384: a scalar of 48 bytes, signatures of 96 bytes r ‖ s or at most 104 in DER, and the nonce's
HMAC-DRBG over SHA-384 ([p384](../p384.md)).

## Rules

- **A secret, in the object's own memory**: the scalar lives in the object (no allocation), never in managed memory,
  which the collector frees without zeroing. Keep a key on the stack or in a `unique_ptr`; in a managed object it
  stays in memory until the cycle that finds the object dead.
- **Move-only**: there is no copy; [clone](clone.md) makes a second key by name. A move zeroes the
  source, the destructor zeroes the scalar with stores the compiler cannot drop ([secure_zero](../secure_zero.md)), and a
  key moved from is used by no operation: signing, `clone()`, `bytes()`, `public_key()`, `to_ecdh()` and the DER and
  PEM forms throw `std::logic_error`, whose message names the curve and the type
  (`sgcl::crypto::p256::private_key: used after being moved from`).
- **What comes out of it is a secret too**: the scalar as a [secret\<32\>](../secret/README.md), the DER and PEM forms as a
  [secret_bytes](../secret_bytes/README.md), never in managed memory.
- **Constant time**: everything that touches d, the nonce k and its inverse runs in constant time
  ([p256](../p256.md#rules)).
- **Thread safety**: the methods are `const` and a signature draws its own random bytes, so any number of threads may
  sign with one key at once.

## Member objects

| Constant | P-256 | P-384 | Description |
|---|---|---|---|
| `size` | 32 | 48 | the bytes of the scalar, `static constexpr size_t` |
| `signature_size` | 64 | 96 | a signature r ‖ s, as `sign_digest_raw` gives it, `static constexpr size_t` |
| `max_signature_size` | 72 | 104 | the longest signature in DER, as `sign_digest` gives it, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](p256-private_key.md) | the move; there is no copy |
| `(destructor)` | zeroes the scalar |
| [operator=](operator_assign.md) | the move assignment |
| [clone](clone.md) | a second key of the same scalar |

#### Creation

| Function | Description |
|---|---|
| [generate](generate.md) | a new key from crypto::random (static) |
| [from_bytes](from_bytes.md) | the key of its scalar (static) |
| [from_pkcs8_der](from_pkcs8_der.md) | reads a PKCS #8 PrivateKeyInfo (static) |
| [from_sec1_der](from_sec1_der.md) | reads a SEC 1 ECPrivateKey (static) |
| [from_pem](from_pem.md) | reads the first private key block of PEM text (static) |

#### Signing

| Function | Description |
|---|---|
| [sign_digest](sign_digest.md) | the signature of a digest, in DER |
| [sign_digest_raw](sign_digest_raw.md) | the signature of a digest, r ‖ s |

#### Observers

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public key that verifies the signatures |
| [to_ecdh](to_ecdh.md) | the same scalar as an ECDH key |

#### Encodings

| Function | Description |
|---|---|
| [bytes](bytes.md) | the scalar, as a secret |
| [to_pkcs8_der](to_pkcs8_der.md) | the PKCS #8 PrivateKeyInfo, as a secret |
| [to_sec1_der](to_sec1_der.md) | the SEC 1 ECPrivateKey, as a secret |
| [to_pem](to_pem.md) | the PKCS #8 as a `PRIVATE KEY` block of PEM, as a secret |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::private_key::generate();

    // the signer signs the digest of its message
    auto message = "{\"amount\":100}";
    auto signature = key.sign_digest(crypto::sha256::of(message));

    // the verifier has the message, the signature and the public key
    auto verifier = key.public_key();
    println("{}", verifier.verify_digest(crypto::sha256::of(message), signature));
    println("{}", verifier.verify_digest(crypto::sha256::of("{\"amount\":900}"), signature));

    // the key kept as PKCS #8, and read back
    auto stored = key.to_pkcs8_der();
    auto read = crypto::p256::private_key::from_pkcs8_der(stored);
    println("{}", read->public_key() == verifier);
}
```

Output:

```text
true
false
true
```

## See also

- [p256::public_key](../p256-public_key/README.md): the key that verifies
- [p256::ecdh_key](../p256-ecdh_key/README.md): the key of an agreement
- [ECDSA](../ecdsa.md): the digest, the nonce, the encodings
- [p256](../p256.md), [p384](../p384.md): the curves and their rules
- [ed25519::private_key](../ed25519-private_key/README.md): signatures on Curve25519
