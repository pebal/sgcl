[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::bytes

```cpp
secret<32> bytes() const;
```

Returns the 32 secret bytes of the key, as they were given or generated, not clamped, as Go's
`ecdh.PrivateKey.Bytes` gives them: what [from_bytes](from_bytes.md) takes back. They come as a
[secret\<32\>](../secret.md), move-only and zeroed when it goes.

## Parameters

None.

## Return value

The bytes of the key.

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
    auto key = crypto::x25519::private_key::generate();
    crypto::secret<32> saved = key.bytes();

    auto restored = crypto::x25519::private_key::from_bytes(saved);
    println("{} {}", saved.size, restored == key);
}
```

Output:

```text
32 true
```

## See also

- [from_bytes](from_bytes.md): the reverse
- [to_pkcs8_der](to_pkcs8_der.md): the key in a PKCS #8
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
