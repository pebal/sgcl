[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::generate

```cpp
static private_key generate(hpke::kem k);
```

A new key of the KEM, of the module's random bytes.

## Parameters

| Parameter | Description |
|---|---|
| `k` | the KEM |

## Return value

The key.

## Complexity

Constant.

## Exceptions

`std::invalid_argument` for a KEM of no value of its enumeration.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_p384);
    println("{}", key.public_key().bytes().size());
}
```

Output:

```text
97
```

## See also

- [derive](derive.md): the same key every time
- [sgcl::crypto::hpke::private_key](README.md)
