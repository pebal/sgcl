[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md)

# sgcl::crypto::x25519::private_key

```cpp
#include "sgcl/crypto/x25519.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::x25519 {
    class private_key;
}
```

`sgcl::crypto::x25519::private_key` is one side's X25519 key: 32 secret bytes, and the
[public_key](../x25519-public_key/README.md) they give, computed once when the key is made. It computes the secret shared with the owner of a peer's public key,
[shared_secret](shared_secret.md). A key is made by [generate](generate.md)
from [random](../random/README.md), or read: from 32 bytes, a PKCS #8 or a PEM. Any 32 bytes are a key: they are clamped
(RFC 7748 §5) each time they are used, not when stored, so [bytes](bytes.md) gives back what was
given, as Go's `ecdh.PrivateKey.Bytes` does.

Go's `*ecdh.PrivateKey` is a pointer to bytes left to the garbage collector; this is a value that holds its bytes
in itself and zeroes them when it goes.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **A secret, so no copy.** The key is move-only: a second one is asked for by name with
  [clone](clone.md), a move leaves the key moved from zeroed, and the destructor zeroes the key
  with stores the compiler cannot drop ([secure_zero](../secure_zero.md)). What it gives out of its secret is a
  [secret\<32\>](../secret/README.md) or a [secret_bytes](../secret_bytes/README.md), which zero themselves in turn.
- **Where it lives.** On the stack or in a `unique_ptr`. In a managed object it stays in memory until the cycle
  that finds the object dead, and after it, since managed memory is not zeroed when an object dies.
- **A key moved from is no key.** Every operation on it but the assignment and the destructor throws `logic_error`,
  `clone` and `==` included, as on every key of the module.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](x25519-private_key.md) | takes another key over and zeroes it |
| `(destructor)` | zeroes the key |
| [operator=](operator_assign.md) | takes another key over and zeroes it |
| [clone](clone.md) | a second key of the same bytes |

#### Making a key

| Function | Description |
|---|---|
| [generate](generate.md) | a new key from [random](../random/README.md) (static) |
| [from_bytes](from_bytes.md) | the key of 32 bytes (static) |
| [from_pkcs8_der](from_pkcs8_der.md) | the key of a PKCS #8 PrivateKeyInfo (static) |
| [from_pem](from_pem.md) | the key of a `PRIVATE KEY` block of PEM (static) |

#### Key agreement

| Function | Description |
|---|---|
| [shared_secret](shared_secret.md) | the secret shared with the owner of a peer's public key |

#### Observers

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public key, to send to the peer |
| [bytes](bytes.md) | the 32 secret bytes, as a secret |

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
    auto ours = crypto::x25519::private_key::generate();
    auto theirs = crypto::x25519::private_key::generate();

    // the public keys cross; each side computes the same secret
    auto shared = ours.shared_secret(theirs.public_key());
    println("{}", shared == theirs.shared_secret(ours.public_key()));

    // the key kept for later, as PKCS #8 in a secret_bytes
    crypto::secret_bytes saved = ours.to_pkcs8_der();
    auto restored = crypto::x25519::private_key::from_pkcs8_der(saved);
    println("{} {}", saved.size(), restored == ours);
}
```

Output:

```text
true
48 true
```

## See also

- [x25519::public_key](../x25519-public_key/README.md): the peer's key
- [hkdf](../hkdf/README.md): what the shared secret goes through
- [sgcl::crypto::x25519](../x25519.md)
