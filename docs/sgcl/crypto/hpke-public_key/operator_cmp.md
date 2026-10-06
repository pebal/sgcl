[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [public_key](README.md)

# sgcl::crypto::hpke::public_key::operator==

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Whether the two are the same key of the same KEM.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same key.

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
    auto a = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto b = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    println("{} {}", a.public_key() == a.public_key(), a.public_key() == b.public_key());
}
```

Output:

```text
true false
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::hpke::public_key](README.md)
