[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::bits

```cpp
size_t bits() const;
```

Returns the length of the modulus in bits, as [public_key::bits](../rsa-public_key/bits.md) does.

## Parameters

None.

## Return value

The bits of the modulus, 1024 to 16384.

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    println("{}", key.bits());
}
```

Output:

```text
2048
```

## See also

- [size](size.md): the bytes of the modulus
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
