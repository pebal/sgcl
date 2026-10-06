[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::public_key

```cpp
hpke::public_key public_key() const;
```

The key's public half: what messages are sealed to.

## Parameters

None.

## Return value

The public key.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    println("{}", key.public_key().bytes().size());
}
```

Output:

```text
32
```

## See also

- [public_key](../hpke-public_key/README.md)
- [sgcl::crypto::hpke::private_key](README.md)
