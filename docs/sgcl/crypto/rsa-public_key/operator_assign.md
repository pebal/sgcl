[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::operator=

```cpp
public_key& operator=(const public_key& other) noexcept;    // (1)
public_key& operator=(public_key&& other) noexcept;         // (2)
```

Makes this key another one.

1. Copies the modulus, the exponent and the Montgomery constants of `other`.
2. Takes the numbers of `other` over and leaves `other` empty.

- (1–2) A key assigned to itself stays the key it was.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key assigned |

## Return value

`*this`.

## Complexity

- (1) Linear in the bits of the modulus.
- (2) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto other = crypto::rsa::private_key::generate(2048);
    crypto::rsa::public_key pub = other.public_key();
    println("{}", pub == key.public_key());
    pub = key.public_key();
    println("{}", pub == key.public_key());
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](rsa-public_key.md)
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
