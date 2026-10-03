[sgcl](../README.md) › [crypto](README.md) › [p256](p256.md)

# sgcl::crypto::p256::ecdh_key

```cpp
#include "sgcl/crypto/p256.h"   // or "sgcl/crypto.h"
#include "sgcl/crypto/p384.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::p256 {
    using ecdh_key = /* unspecified */;
}

namespace sgcl::crypto::p384 {
    using ecdh_key = /* unspecified */;
}
```

`sgcl::crypto::p256::ecdh_key` is an ECDH key of the curve P-256 (SP 800-56A §5.7.1.2), Go's `ecdh.PrivateKey` over
`ecdh.P256()`: the secret scalar d and the public point d·G. Each side of an agreement has one and sends the other
its [public_key](p256-ecdh_key/public_key.md); each computes the same
[shared secret](p256-ecdh_key/shared_secret.md) from its own key and the other's public one. A key is made by
[generate](p256-ecdh_key/generate.md) or read from its scalar, from PKCS #8 or from PEM.

The key is for agreement alone, as in Go: signing takes a [private_key](p256-private_key.md), whose
[to_ecdh](p256-private_key/to_ecdh.md) makes an ECDH key of the same scalar. `p384::ecdh_key` is the same class on
P-384: a scalar and a shared secret of 48 bytes ([p384](p384.md)).

## Rules

- **A secret, in the object's own memory**: the scalar lives in the object (no allocation), never in managed memory,
  which the collector frees without zeroing. Keep a key on the stack or in a `unique_ptr`; in a managed object it
  stays in memory until the cycle that finds the object dead.
- **Move-only**: there is no copy; [clone](p256-ecdh_key/clone.md) makes a second key by name. A move zeroes the
  source, the destructor zeroes the scalar with stores the compiler cannot drop ([secure_zero](secure_zero.md)), and a
  key moved from is used by no operation: `shared_secret`, `clone()`, `bytes()`, `public_key()` and the DER and PEM
  forms throw `std::logic_error`, whose message names the curve and the type
  (`sgcl::crypto::p256::ecdh_key: used after being moved from`).
- **The shared secret is not a key**: it is the x-coordinate of d·Q, a [secret\<32\>](secret.md), to be put through
  [hkdf](hkdf.md) with the protocol's salt and info before it keys anything.
- **Constant time**: the multiplication by the secret scalar takes each window's multiple by scanning the whole table
  through masks, on field arithmetic with no branch and no address that depends on a value ([p256](p256.md#rules)).
  The peer's key is public: it was checked when it was made.
- **Thread safety**: the methods are `const`, so any number of threads may agree with one key at once.

## Member objects

| Constant | P-256 | P-384 | Description |
|---|---|---|---|
| `size` | 32 | 48 | the bytes of the scalar and of the shared secret, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](p256-ecdh_key/p256-ecdh_key.md) | the move; there is no copy |
| `(destructor)` | zeroes the scalar |
| [operator=](p256-ecdh_key/operator_assign.md) | the move assignment |
| [clone](p256-ecdh_key/clone.md) | a second key of the same scalar |

#### Creation

| Function | Description |
|---|---|
| [generate](p256-ecdh_key/generate.md) | a new key from crypto::random (static) |
| [from_bytes](p256-ecdh_key/from_bytes.md) | the key of its scalar (static) |
| [from_pkcs8_der](p256-ecdh_key/from_pkcs8_der.md) | reads a PKCS #8 PrivateKeyInfo (static) |
| [from_pem](p256-ecdh_key/from_pem.md) | reads the first `PRIVATE KEY` block of PEM text (static) |

#### Key agreement

| Function | Description |
|---|---|
| [shared_secret](p256-ecdh_key/shared_secret.md) | the secret this key shares with a peer |

#### Observers

| Function | Description |
|---|---|
| [public_key](p256-ecdh_key/public_key.md) | the public key to send to the peer |

#### Encodings

| Function | Description |
|---|---|
| [bytes](p256-ecdh_key/bytes.md) | the scalar, as a secret |
| [to_pkcs8_der](p256-ecdh_key/to_pkcs8_der.md) | the PKCS #8 PrivateKeyInfo, as a secret |
| [to_pem](p256-ecdh_key/to_pem.md) | the PKCS #8 as a `PRIVATE KEY` block of PEM, as a secret |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto alice = crypto::p256::ecdh_key::generate();
    auto bob = crypto::p256::ecdh_key::generate();

    // each sends the other its public key, 65 bytes
    auto to_bob = alice.public_key().bytes();
    auto to_alice = bob.public_key().bytes();

    // and each computes the same secret from what it received
    auto alice_secret = alice.shared_secret(crypto::p256::public_key::from_bytes(to_alice));
    auto bob_secret = bob.shared_secret(crypto::p256::public_key::from_bytes(to_bob));
    println("{}", alice_secret == bob_secret);

    // a key for a cipher is derived from the secret, never the secret itself
    auto key = crypto::hkdf_sha256::derive("salt", alice_secret, "chat v1 key", 32);
    println("{} bytes", key.size());
}
```

Output:

```text
true
32 bytes
```

## See also

- [p256::public_key](p256-public_key.md): the peer's key
- [p256::private_key](p256-private_key.md): the key that signs
- [hkdf](hkdf.md): the key derived from the shared secret
- [p256](p256.md), [p384](p384.md): the curves and their rules
- [x25519::private_key](x25519-private_key.md): agreement on Curve25519
