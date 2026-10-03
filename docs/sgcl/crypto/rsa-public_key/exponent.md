[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::exponent

```cpp
uint64_t exponent() const;
```

Returns the public exponent e: 65537 for nearly every key, and for every key [generate](../rsa-private_key/generate.md)
makes.

## Parameters

None.

## Return value

e, odd, 3 to 2³¹ − 1.

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

    println("{}", key.public_key().exponent());
}
```

Output:

```text
65537
```

## See also

- [modulus](modulus.md): n
- [sgcl::crypto::rsa::public_key](README.md)
