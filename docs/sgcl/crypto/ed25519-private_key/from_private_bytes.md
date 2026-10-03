[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::from_private_bytes

```cpp
static expected<private_key, error> from_private_bytes(const slice<const byte>& bytes) noexcept;
```

Makes the key of 64 bytes, the seed and the public key, as Go's `ed25519.PrivateKey` and [bytes](bytes.md) hold
them. The public half must be the one the seed gives: it is checked, at the cost of one scalar multiplication, where
Go takes the 64 bytes as they are. A pair put together from two sources would sign under someone else's public key,
and signatures under a mismatched pair give the secret scalar away.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the 64 bytes: the seed, then the public key |

## Return value

The key, or an [error](../error.md) `errc::invalid_key` when `bytes` is not 64 bytes long or its public half is not
the seed's.

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
    // RFC 8032's TEST 1 seed with its public key, then with TEST 2's
    auto pair = crypto::ed25519::private_key::from_private_bytes(encoding::hex::decode(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"));
    println("{}", pair.has_value());

    auto mismatched = crypto::ed25519::private_key::from_private_bytes(encoding::hex::decode(
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"
        "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c"));
    println("{}", mismatched.error().message());
}
```

Output:

```text
true
the public half is not the seed's
```

## See also

- [bytes](bytes.md): the reverse
- [from_seed](from_seed.md): the key of the seed alone
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
