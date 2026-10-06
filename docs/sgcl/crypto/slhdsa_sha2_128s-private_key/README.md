[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md)

# sgcl::crypto::slhdsa_sha2_128s::private_key

```cpp
#include "sgcl/crypto/slhdsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::slhdsa_sha2_128s {
    class private_key;
}

// and in each of the other eleven sets:
// slhdsa_sha2_128f, slhdsa_sha2_192s, slhdsa_sha2_192f, slhdsa_sha2_256s, slhdsa_sha2_256f,
// slhdsa_shake_128s, slhdsa_shake_128f, slhdsa_shake_192s, slhdsa_shake_192f, slhdsa_shake_256s, slhdsa_shake_256f
```

`sgcl::crypto::slhdsa_sha2_128s::private_key` is the secret half of an [SLH-DSA](../slhdsa.md) key pair: it [signs](sign.md)
messages, and its [public_key](public_key.md) verifies them. A key is made by [generate](generate.md) or from its
bytes by [from_bytes](from_bytes.md).

The key is its 4n bytes — SK.seed, SK.prf, PK.seed, PK.root — and the hash streams of PK.seed the SHA-2 sets start
every hash from, in the object itself. The private keys of the other sets are the same class over their parameters.

## Rules

- **A secret, in the object's own memory**: never in managed memory, zeroed by the destructor and by a move's source.
- **Move-only**: there is no copy; [clone](clone.md) makes a second key by name. An object moved from refuses every
  operation with `std::logic_error` until a key is assigned to it.
- **What is secret, and constant time**: SK.seed, SK.prf and the WOTS+ and FORS values derived of them; they only go
  through the hash functions ([SLH-DSA](../slhdsa.md#rules)).
- **Thread safety**: the methods are `const`, so any number of threads may sign with one key at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](slhdsa_sha2_128s-private_key.md) | the move; there is no copy |
| `(destructor)` | zeroes the key |
| [clone](clone.md) | a second key of the same bytes |

#### Creation

| Function | Description |
|---|---|
| [generate](generate.md) | a new key of random seeds (static) |
| [from_bytes](from_bytes.md) | the key of its 4n bytes, PK.root checked (static) |

#### Signing

| Function | Description |
|---|---|
| [sign](sign.md) | the signature of a message, hedged or deterministic, with a context |

#### Observers

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public key, to publish |
| [bytes](bytes.md) | the 4n bytes, as a secret |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two keys are the same, in constant time |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    auto pub = key.public_key();
    auto sig = key.sign("a message", {.context = "app/v1"});
    println("{} {}", pub.verify("a message", sig, {.context = "app/v1"}), pub.verify("a message", sig));
}
```

Output:

```text
true false
```

## See also

- [slhdsa_sha2_128s::public_key](../slhdsa_sha2_128s-public_key/README.md): the public half
- [slhdsa_sha2_128s::options](../slhdsa_sha2_128s-options.md): the context and the determinism
- [SLH-DSA](../slhdsa.md): the parameter sets, the sizes, the rules
- [mldsa65::private_key](../mldsa65-private_key/README.md): the lattice signature
