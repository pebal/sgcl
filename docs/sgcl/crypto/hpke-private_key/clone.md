[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::clone

```cpp
private_key clone() const;
```

A copy of the key, asked for by name: the key is move-only, as every secret of the module is.

## Parameters

None.

## Return value

The copy.

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
    auto copy = key.clone();
    println("{}", copy.public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [(constructor)](hpke-private_key.md)
- [sgcl::crypto::hpke::private_key](README.md)
