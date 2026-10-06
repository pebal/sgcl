[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md)

# sgcl::crypto::hpke::public_key

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    class public_key;
}
```

`sgcl::crypto::hpke::public_key` is a KEM public key of HPKE: the recipient's, which messages are sealed to, or a
sender's of the auth modes. A value of 32 to 133 bytes held in the object, copied and compared freely; read from its
bytes with its KEM checked, written back by [bytes](bytes.md). Go's `hpke.PublicKey`.

## Member functions

| Function | Description |
|---|---|
| [from_bytes](from_bytes.md) | a key of its bytes, checked (static) |
| [kem](kem.md) | the key's KEM |
| [bytes](bytes.md) | the key's bytes (SerializePublicKey) |
| [operator==](operator_cmp.md) | checks whether two are the same key |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto published = key.public_key().bytes();
    auto read = crypto::hpke::public_key::from_bytes(crypto::hpke::kem::dhkem_x25519, published);
    println("{}", read.value() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [private_key](../hpke-private_key/README.md)
- [sgcl::crypto::hpke](../hpke.md)
