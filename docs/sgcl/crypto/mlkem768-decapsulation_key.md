[sgcl](../README.md) › [crypto](README.md) › [mlkem768](mlkem.md)

# sgcl::crypto::mlkem768::decapsulation_key

```cpp
#include "sgcl/crypto/mlkem.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mlkem768 {
    class decapsulation_key;
}

namespace sgcl::crypto::mlkem512 {
    class decapsulation_key;
}

namespace sgcl::crypto::mlkem1024 {
    class decapsulation_key;
}
```

`sgcl::crypto::mlkem768::decapsulation_key` is the secret half of an [ML-KEM](mlkem.md) key pair, Go's
`mlkem.DecapsulationKey768`: its owner publishes the
[encapsulation_key](mlkem768-decapsulation_key/encapsulation_key.md) it gives, and
[decapsulates](mlkem768-decapsulation_key/decapsulate.md) every ciphertext sent to it into the shared key the sender
made. A key is made by [generate](mlkem768-decapsulation_key/generate.md) or from its seed by
[from_seed](mlkem768-decapsulation_key/from_seed.md).

The key is kept as its seed d‖z, 64 bytes (FIPS 203 §7.1, the form Go keeps), together with the expanded key made
from it once and the matrix Â of its encapsulation key. `mlkem512::decapsulation_key` and
`mlkem1024::decapsulation_key` are the same class over the other parameter sets: the seed is 64 bytes in every set,
the ciphertexts they take are 768 and 1568 bytes, and their encapsulation keys are of their own sets.

## Rules

- **A secret, in the object's own memory**: the seed, the expanded key and the matrix live in the object, never in
  managed memory, which the collector frees without zeroing. Keep a key on the stack or in a `unique_ptr`; in a
  managed object it stays in memory until the cycle that finds the object dead.
- **Move-only**: there is no copy; [clone](mlkem768-decapsulation_key/clone.md) makes a second key by name. A move
  leaves the object moved from zeroed, the destructor zeroes the whole object with stores the compiler cannot drop
  ([secure_zero](secure_zero.md)), and an object moved from refuses every operation with `std::logic_error` until a
  key is assigned to it.
- **What is secret, and constant time**: the seed, the vector s, everything made from them and the shared key. The
  ring's arithmetic neither branches nor divides on a value, and decapsulation computes the genuine key and the
  rejection's and chooses one by a mask ([ML-KEM](mlkem.md#rules)).
- **No DER and no expanded form**: the seed is the one form of the key; those come with TLS and X.509's certificates
  of ML-KEM.
- **Thread safety**: the methods are `const`, so any number of threads may decapsulate with one key at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](mlkem768-decapsulation_key/mlkem768-decapsulation_key.md) | the move; there is no copy |
| `(destructor)` | zeroes the whole object |
| [operator=](mlkem768-decapsulation_key/operator_assign.md) | the move assignment |
| [clone](mlkem768-decapsulation_key/clone.md) | a second key of the same seed |

#### Creation

| Function | Description |
|---|---|
| [generate](mlkem768-decapsulation_key/generate.md) | a new key from crypto::random (static) |
| [from_seed](mlkem768-decapsulation_key/from_seed.md) | the key of a seed of 64 bytes (static) |

#### Decapsulation

| Function | Description |
|---|---|
| [decapsulate](mlkem768-decapsulation_key/decapsulate.md) | the shared key of a ciphertext |

#### Observers

| Function | Description |
|---|---|
| [encapsulation_key](mlkem768-decapsulation_key/encapsulation_key.md) | the public key to publish |

#### Encodings

| Function | Description |
|---|---|
| [seed](mlkem768-decapsulation_key/seed.md) | the seed d‖z, as a secret |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](mlkem768-decapsulation_key/operator_cmp.md) | compares two keys in constant time |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto owner = crypto::mlkem768::decapsulation_key::generate();
    auto published = owner.encapsulation_key().bytes();

    // the other side reads the published key and encapsulates to it
    auto peer = crypto::mlkem768::encapsulation_key::from_bytes(published);
    auto sent = peer->encapsulate();

    // the owner decapsulates the ciphertext it receives
    auto received = owner.decapsulate(sent.ciphertext);
    println("{}", received == sent.shared_key);

    // the key kept as its seed, and made again from it
    auto again = crypto::mlkem768::decapsulation_key::from_seed(owner.seed());
    println("{}", again->decapsulate(sent.ciphertext) == sent.shared_key);
}
```

Output:

```text
true
true
```

## See also

- [mlkem768::encapsulation_key](mlkem768-encapsulation_key.md): the published key
- [mlkem768::encapsulation](mlkem768-encapsulation.md): the shared key and the ciphertext
- [ML-KEM](mlkem.md): the parameter sets, the sizes, the rules
- [secret](secret.md): the form of the shared key and of the seed
- [x25519::private_key](x25519-private_key.md): the classical counterpart
