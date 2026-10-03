[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md)

# sgcl::crypto::ed25519::private_key

```cpp
#include "sgcl/crypto/ed25519.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::ed25519 {
    class private_key;
}
```

`sgcl::crypto::ed25519::private_key` is an Ed25519 key that signs: a seed of 32 secret bytes, and what SHA-512 of
the seed gives — the secret scalar, the prefix the nonces are derived from, and the [public_key](../ed25519-public_key/README.md)
— all made once when the key is made. It signs messages with [sign](sign.md), deterministically:
the same key and message give the same signature, and signing needs no random numbers. A key is made by
[generate](generate.md) from [random](../random/README.md), or read: from a seed (RFC 8032's private key),
from the 64 bytes of Go's `PrivateKey`, from a PKCS #8 or a PEM.

Go's `ed25519.PrivateKey` is a slice of 64 bytes, the seed and the public key, left to the garbage collector; this
is a value that holds the seed, the scalar and the prefix in itself and zeroes them when it goes.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A secret, so no copy.** The key is move-only: a second one is asked for by name with
  [clone](clone.md), a move leaves the key moved from zeroed, and the destructor zeroes the key
  with stores the compiler cannot drop ([secure_zero](../secure_zero.md)). What it gives out of its secret is a
  [secret\<N\>](../secret/README.md) or a [secret_bytes](../secret_bytes/README.md), which zero themselves in turn.
- **Where it lives.** On the stack or in a `unique_ptr`. In a managed object it stays in memory until the cycle
  that finds the object dead, and after it, since managed memory is not zeroed when an object dies.
- **A key moved from is no key.** Every operation on it but the assignment and the destructor throws `logic_error`,
  `clone` and `==` included, as on every key of the module.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ed25519-private_key.md) | takes another key over and zeroes it |
| `(destructor)` | zeroes the key |
| [operator=](operator_assign.md) | takes another key over and zeroes it |
| [clone](clone.md) | a second key of the same seed |

#### Making a key

| Function | Description |
|---|---|
| [generate](generate.md) | a new key from [random](../random/README.md) (static) |
| [from_seed](from_seed.md) | the key of a seed of 32 bytes (static) |
| [from_private_bytes](from_private_bytes.md) | the key of 64 bytes, the seed and the public key (static) |
| [from_pkcs8_der](from_pkcs8_der.md) | the key of a PKCS #8 PrivateKeyInfo (static) |
| [from_pem](from_pem.md) | the key of a `PRIVATE KEY` block of PEM (static) |

#### Signatures

| Function | Description |
|---|---|
| [sign](sign.md) | the signature of a message, 64 bytes |

#### Observers

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public key, that verifies |
| [seed](seed.md) | the seed, as a secret |
| [bytes](bytes.md) | the seed and the public key, 64 bytes, as a secret |

#### Conversions

| Function | Description |
|---|---|
| [to_pkcs8_der](to_pkcs8_der.md) | the PKCS #8 PrivateKeyInfo, 48 bytes, as a secret |
| [to_pem](to_pem.md) | the `PRIVATE KEY` block of PEM, as a secret |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two keys in constant time |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    auto signature = key.sign("release 1.4.2");
    println("{} {}", signature.size(), key.public_key().verify("release 1.4.2", signature));

    // the key kept for later, as PEM in a secret_bytes
    crypto::secret_bytes pem = key.to_pem();
    auto restored = crypto::ed25519::private_key::from_pem(pem);
    println("{}", restored->sign("release 1.4.2") == signature);
}
```

Output:

```text
64 true
true
```

## See also

- [ed25519::public_key](../ed25519-public_key/README.md): the key that verifies
- [read_secret](../read_secret.md): a key file read as a secret
- [sgcl::crypto::ed25519](../ed25519.md)
