[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [public_key](README.md)

# sgcl::crypto::hpke::public_key::bytes

```cpp
vector<byte> bytes() const;
```

The key's bytes (SerializePublicKey): 32 of X25519, a point uncompressed of 65, 97 or 133 bytes.

## Parameters

None.

## Return value

The bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::derive(crypto::hpke::kem::dhkem_x25519, "a seed of thirty-two bytes ....");
    println("{}", encoding::hex::encode(key.public_key().bytes()));
}
```

Output:

```text
93886c5c0b5d8897ec0640fd33a95cbb89bddd3b21f825ca7e86747ce2459912
```

## See also

- [from_bytes](from_bytes.md): the other way
- [sgcl::crypto::hpke::public_key](README.md)
