[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::bytes

```cpp
secret<64> bytes() const;
```

Returns the 64 bytes of Go's `ed25519.PrivateKey`: the seed, then the public key.
[from_private_bytes](from_private_bytes.md) takes them back. They come as a [secret\<64\>](../secret.md), move-only
and zeroed when it goes.

## Parameters

None.

## Return value

The seed and the public key, 64 bytes.

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
    crypto::secret<64> pair = key.bytes();

    auto public_half = pair.bytes().subslice(32);
    bool same = crypto::constant_time::equal(public_half, key.public_key().bytes());
    println("{} {}", pair.size, same);

    auto restored = crypto::ed25519::private_key::from_private_bytes(pair);
    println("{}", restored == key);
}
```

Output:

```text
64 true
true
```

## See also

- [from_private_bytes](from_private_bytes.md): the reverse
- [seed](seed.md): the seed alone
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
