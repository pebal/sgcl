[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](README.md)

# sgcl::crypto::ed25519::private_key::seed

```cpp
secret<32> seed() const;
```

Returns the seed, RFC 8032's private key and what [from_seed](from_seed.md) takes back, Go's `PrivateKey.Seed()`. It
comes as a [secret\<32\>](../secret/README.md), move-only and zeroed when it goes.

## Parameters

None.

## Return value

The seed, 32 bytes.

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    crypto::secret<32> seed = key.seed();

    auto restored = crypto::ed25519::private_key::from_seed(seed);
    println("{} {}", seed.size, restored == key);
}
```

Output:

```text
32 true
```

## See also

- [from_seed](from_seed.md): the reverse
- [bytes](bytes.md): the seed and the public key
- [sgcl::crypto::ed25519::private_key](README.md)
