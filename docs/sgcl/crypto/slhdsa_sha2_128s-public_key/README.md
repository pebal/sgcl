[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md)

# sgcl::crypto::slhdsa_sha2_128s::public_key

```cpp
#include "sgcl/crypto/slhdsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::slhdsa_sha2_128s {
    class public_key;
}

// and in each of the other eleven sets
```

`sgcl::crypto::slhdsa_sha2_128s::public_key` is the public half of an [SLH-DSA](../slhdsa.md) key pair: it
[verifies](verify.md) the signatures of its [private_key](../slhdsa_sha2_128s-private_key/README.md). It is read from its bytes by
[from_bytes](from_bytes.md) or taken from the private key by its
[public_key()](../slhdsa_sha2_128s-private_key/public_key.md).

The key is a plain value: its 2n bytes, PK.seed and PK.root, and the hash streams of PK.seed the SHA-2 sets start
every hash from, in the object. It holds no tracked pointer: it lives anywhere, a global and a `std` container
included. The public keys of the other sets are the same class over their parameters.

## Rules

- **Public**: nothing in the key is a secret; its comparison and its verifications take the time the bytes ask for.
- **Every string of the right length is a key** (FIPS 205 has no check of a public key).
- **Thread safety**: the methods are `const`, so any number of threads may verify with one key at once.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the copy and the move |

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
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    vector<byte> published = key.public_key().bytes();
    auto signature = key.sign("firmware 2.1");

    auto verifier = crypto::slhdsa_sha2_128s::public_key::from_bytes(published).value();
    println("{}", verifier.verify("firmware 2.1", signature));
    println("{}", verifier.verify("firmware 2.2", signature));
}
```

Output:

```text
true
false
```

## See also

- [slhdsa_sha2_128s::private_key](../slhdsa_sha2_128s-private_key/README.md): the secret half
- [SLH-DSA](../slhdsa.md): the parameter sets, the sizes, the rules
