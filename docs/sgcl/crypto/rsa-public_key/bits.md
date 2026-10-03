[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::bits

```cpp
size_t bits() const;
```

Returns the length of the modulus in bits: 2048 for a 2048-bit key. The strength of the key is told by it; a
signature and a ciphertext are [size](size.md) bytes.

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

    println("{}", key.public_key().bits());
}
```

Output:

```text
2048
```

## See also

- [size](size.md): the bytes of the modulus
- [sgcl::crypto::rsa::public_key](README.md)
