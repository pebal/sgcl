[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](README.md)

# sgcl::crypto::rsa::private_key::public_key

```cpp
public_key public_key() const;
```

Returns the public key of this key, n and e: a plain value, given to whoever verifies the signatures or encrypts for
the key's holder.

## Parameters

None.

## Return value

A copy of the [public key](../rsa-public_key/README.md).

## Complexity

Linear in the bits of the modulus.

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

    crypto::rsa::public_key pub = key.public_key();
    println("{} bits, e = {}", pub.bits(), pub.exponent());
}
```

Output:

```text
2048 bits, e = 65537
```

## See also

- [public_key::to_pkix_der](../rsa-public_key/to_pkix_der.md): the public key as it is published
- [sgcl::crypto::rsa::private_key](README.md)
