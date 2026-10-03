[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::generate

```cpp
static private_key generate() noexcept;
```

A new key, Go's `ecdsa.GenerateKey(elliptic.P256(), rand.Reader)`: 32 random bytes from
[crypto::random](../random/README.md), drawn again until they are a scalar in [1, n − 1] (a draw is refused with probability
below 2⁻³²), and the public point of that scalar. `p384::private_key::generate` draws 48 bytes for P-384.

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
    auto a = crypto::p256::private_key::generate();
    auto b = crypto::p256::private_key::generate();
    println("{}", a.public_key() == b.public_key());

    auto digest = crypto::sha256::of("the message");
    println("{}", a.public_key().verify_digest(digest, a.sign_digest(digest)));
}
```

Output:

```text
false
true
```

## See also

- [from_bytes](from_bytes.md): the key of a known scalar
- [sgcl::crypto::p256::private_key](README.md)
