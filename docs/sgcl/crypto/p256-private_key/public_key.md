[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::public_key

```cpp
p256::public_key public_key() const;
```

The public key of this key, the point d·G, Go's `PrivateKey.Public`: what the signer publishes and a verifier
checks its signatures with. The point is computed when the key is made and kept in it, so this costs a copy of it.
`p384::private_key::public_key` gives a `p384::public_key`.

## Parameters

None.

## Return value

The [public_key](../p256-public_key.md).

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the key of RFC 6979, A.2.5: its public key is the RFC's Ux and Uy
    auto key = crypto::p256::private_key::from_bytes(encoding::hex::decode(
        "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721"));
    auto point = key->public_key().bytes();
    println("Ux {}", encoding::hex::encode(point.as_slice(1, 32)));
    println("Uy {}", encoding::hex::encode(point.as_slice(33, 32)));
}
```

Output:

```text
Ux 60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
Uy 7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299
```

## See also

- [p256::public_key](../p256-public_key.md): what it gives
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
