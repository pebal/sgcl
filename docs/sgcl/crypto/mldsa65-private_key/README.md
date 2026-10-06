[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md)

# sgcl::crypto::mldsa65::private_key

```cpp
#include "sgcl/crypto/mldsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mldsa65 {
    class private_key;
}

namespace sgcl::crypto::mldsa44 {
    class private_key;
}

namespace sgcl::crypto::mldsa87 {
    class private_key;
}
```

`sgcl::crypto::mldsa65::private_key` is the secret half of an [ML-DSA](../mldsa.md) key pair, Go's
`mldsa.PrivateKey`: it [signs](sign.md) messages, and its [public_key](public_key.md) verifies them. A key is made by
[generate](generate.md) or from its seed by [from_seed](from_seed.md).

The key is kept as its seed ξ, 32 bytes (the form RFC 9881 recommends and Go keeps), together with what key
generation makes of it once — s1, s2 and t0 in the transform's domain, the matrix Â, K, tr and the public key's bytes —
in plain memory of its own: 50 KB for ML-DSA-65, 30 KB for 44, 82 KB for 87. `mldsa44::private_key` and
`mldsa87::private_key` are the same class over the other parameter sets.

## Rules

- **A secret, in plain memory**: the seed and the expanded key never lie in managed memory, which the collector frees
  without zeroing; the key's memory is zeroed whole when the key goes. The object itself is one pointer, which lives
  anywhere.
- **Move-only**: there is no copy; [clone](clone.md) makes a second key by name. An object moved from refuses every
  operation with `std::logic_error` until a key is assigned to it.
- **What is secret, and constant time**: the seed, ρ', K, s1, s2, t0, the masks y and everything made of them.
  Signing computes over them without a branch, an index or a division on a value ([ML-DSA](../mldsa.md#rules)); what
  branches is a round's rejection, public by FIPS 204's design.
- **No DER and no expanded form**: the seed is the one form of the key; PKCS #8 of ML-DSA (RFC 9881) comes with the
  public ASN.1 of the encoding module.
- **Thread safety**: the methods are `const`, and a hedged signature draws its own random bytes, so any number of
  threads may sign with one key at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mldsa65-private_key.md) | the move; there is no copy |
| `(destructor)` | zeroes the expanded key |
| [clone](clone.md) | a second key of the same seed |

#### Creation

| Function | Description |
|---|---|
| [generate](generate.md) | a new key of a random seed (static) |
| [from_seed](from_seed.md) | the key of a seed of 32 bytes (static) |

#### Signing

| Function | Description |
|---|---|
| [sign](sign.md) | the signature of a message, hedged or deterministic, with a context |

#### Observers

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public key, to publish |
| [seed](seed.md) | the seed, as a secret |

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
    auto key = crypto::mldsa65::private_key::generate();
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

- [mldsa65::public_key](../mldsa65-public_key/README.md): the public half
- [mldsa65::options](../mldsa65-options.md): the context and the determinism
- [ML-DSA](../mldsa.md): the parameter sets, the sizes, the rules
- [ed25519::private_key](../ed25519-private_key/README.md): the classical counterpart
