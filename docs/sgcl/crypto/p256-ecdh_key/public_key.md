[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::public_key

```cpp
p256::public_key public_key() const;
```

The public key of this key, the point d·G, Go's `ecdh.PrivateKey.PublicKey`: what one side sends the other, whose
[shared_secret](shared_secret.md) with it gives the secret both share. The point is computed when the key is made and
kept in it, so this costs a copy of it. `p384::ecdh_key::public_key` gives a `p384::public_key`.

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
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::ecdh_key::generate();
    auto sent = key.public_key().bytes();  // 04 || X || Y, as TLS sends it
    println("{} bytes, starting with {}", sent.size(), sent[0]);
}
```

Output:

```text
65 bytes, starting with 4
```

## See also

- [p256::public_key](../p256-public_key.md): what it gives
- [shared_secret](shared_secret.md): the secret of the peer's public key
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
