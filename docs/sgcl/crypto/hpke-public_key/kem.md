[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [public_key](README.md)

# sgcl::crypto::hpke::public_key::kem

```cpp
hpke::kem kem() const noexcept;
```

The KEM the key is of.

## Parameters

None.

## Return value

The KEM.

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
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p384).public_key();
    println("{}", key.kem() == crypto::hpke::kem::dhkem_p384);
}
```

Output:

```text
true
```

## See also

- [kem](../hpke-kem.md)
- [sgcl::crypto::hpke::public_key](README.md)
