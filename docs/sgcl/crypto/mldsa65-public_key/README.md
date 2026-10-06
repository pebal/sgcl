[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md)

# sgcl::crypto::mldsa65::public_key

```cpp
#include "sgcl/crypto/mldsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mldsa65 {
    class public_key;
}

namespace sgcl::crypto::mldsa44 {
    class public_key;
}

namespace sgcl::crypto::mldsa87 {
    class public_key;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::mldsa65::public_key` is the public half of an [ML-DSA](../mldsa.md) key pair, Go's `mldsa.PublicKey`: it
[verifies](verify.md) the signatures of its [private_key](../mldsa65-private_key/README.md). It is read from its bytes
by [from_bytes](from_bytes.md) or taken from the private key by its
[public_key()](../mldsa65-private_key/public_key.md).

The key is a handle of one word: copies share one state, made once when the key is made — its bytes, tr = H(pk), the
matrix Â and t1 in the transform's domain, 39 KB for ML-DSA-65 (22 KB for 44, 68 KB for 87) in plain memory owned by
the handle's managed state —
so a verification only hashes, samples and multiplies. `mldsa44::public_key` and `mldsa87::public_key` are the same
class over the other parameter sets.

## Rules

- **Public**: nothing in the key is a secret; its comparison and its verifications take the time the bytes ask for.
- **Every string of the right length is a key** (FIPS 204 has no check of a public key): [from_bytes](from_bytes.md)
  refuses only another length.
- **Thread safety**: the methods are `const` and the state never changes, so any number of threads may verify with
  one key, or its copies, at once.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the copy and the move, of the handle |

#### Creation

| Function | Description |
|---|---|
| [from_bytes](from_bytes.md) | reads a key from its bytes (static) |

#### Verification

| Function | Description |
|---|---|
| [verify](verify.md) | whether a signature of a message verifies |

#### Encodings

| Function | Description |
|---|---|
| [bytes](bytes.md) | the key's bytes, the form it is published in |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares the bytes of two keys |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    vector<byte> published = key.public_key().bytes();
    auto signature = key.sign("pay 10 to Bob");

    auto verifier = crypto::mldsa65::public_key::from_bytes(published).value();
    println("{}", verifier.verify("pay 10 to Bob", signature));
    println("{}", verifier.verify("pay 99 to Bob", signature));
}
```

Output:

```text
true
false
```

## See also

- [mldsa65::private_key](../mldsa65-private_key/README.md): the secret half
- [ML-DSA](../mldsa.md): the parameter sets, the sizes, the rules
- [ed25519::public_key](../ed25519-public_key/README.md): the classical counterpart
