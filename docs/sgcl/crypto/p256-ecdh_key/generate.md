[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::generate

```cpp
static ecdh_key generate() noexcept;
```

A new key, Go's `ecdh.P256().GenerateKey(rand.Reader)`: 32 random bytes from [crypto::random](../random.md), drawn
again until they are a scalar in [1, n − 1] (a draw is refused with probability below 2⁻³²), and the public point of
that scalar. `p384::ecdh_key::generate` draws 48 bytes for P-384. A key of its own for every session gives the
agreement forward secrecy, as TLS's ECDHE does.

## Parameters

None.

## Return value

The new key.

## Complexity

Constant: one multiplication of the base point.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto alice = crypto::p256::ecdh_key::generate();
    auto bob = crypto::p256::ecdh_key::generate();
    println("{}", alice.public_key() == bob.public_key());
    println("{}", alice.shared_secret(bob.public_key()) == bob.shared_secret(alice.public_key()));
}
```

Output:

```text
false
true
```

## See also

- [from_bytes](from_bytes.md): the key of a known scalar
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
