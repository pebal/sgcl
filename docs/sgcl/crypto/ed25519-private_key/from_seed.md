[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::from_seed

```cpp
static expected<private_key, error> from_seed(const slice<const byte>& seed) noexcept;
```

Makes the key of a seed of 32 bytes, RFC 8032's private key (§5.1.5): SHA-512 of the seed gives the secret scalar,
its first half clamped, and the prefix, its second half; the public key is the point the scalar gives. Any 32 bytes
are a seed. Go's `ed25519.NewKeyFromSeed`, which panics on a wrong length.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the 32 bytes of the seed |

## Return value

The key, or an [error](../error.md) `errc::invalid_key` when `seed` is not 32 bytes long.

## Complexity

Constant: one SHA-512 and one fixed-base multiplication.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 seed and the public key it gives
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    println("{}", encoding::hex::encode(key->public_key().bytes()));

    auto short_seed = crypto::ed25519::private_key::from_seed(encoding::hex::decode("9d61b19d"));
    println("{}", short_seed.error().message());
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
an Ed25519 seed is 32 bytes
```

## See also

- [seed](seed.md): the reverse
- [generate](generate.md): a new key
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
