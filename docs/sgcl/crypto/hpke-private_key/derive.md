[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [private_key](README.md)

# sgcl::crypto::hpke::private_key::derive

```cpp
static private_key derive(hpke::kem k, const slice<const byte>& ikm);
```

The key of a seed (DeriveKeyPair, RFC 9180 §7.1.3): the same key every time for the same seed, of the KEM's KDF over
the seed; for the NIST curves the first of the candidates that is a scalar of the curve (P-521's first byte masked to
its last bit). The seed should have as many bytes of entropy as the key has (32, 48, 66).

## Parameters

| Parameter | Description |
|---|---|
| `k` | the KEM |
| `ikm` | the seed, bytes or text |

## Return value

The key.

## Complexity

Constant.

## Exceptions

`std::invalid_argument` for a KEM of no value of its enumeration, and when none of 256 candidates is a scalar of the
curve (DeriveKeyPairError, which no seed is known to give).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto a = crypto::hpke::private_key::derive(crypto::hpke::kem::dhkem_x25519, "a seed of thirty-two bytes ....");
    auto b = crypto::hpke::private_key::derive(crypto::hpke::kem::dhkem_x25519, "a seed of thirty-two bytes ....");
    println("{}", a.public_key() == b.public_key());
}
```

Output:

```text
true
```

## See also

- [generate](generate.md)
- [sgcl::crypto::hpke::private_key](README.md)
