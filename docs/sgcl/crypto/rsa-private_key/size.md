[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::size

```cpp
size_t size() const;
```

Returns the length of the modulus in bytes: the length of every signature the key makes, 256 for a 2048-bit key.

## Parameters

None.

## Return value

The bytes of the modulus, [bits](bits.md) rounded up to whole bytes.

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

    auto sig = key.sign_digest(crypto::hash_id::sha256, crypto::sha256::of("abc"));
    println("{} {}", key.size(), sig.size());
}
```

Output:

```text
256 256
```

## See also

- [bits](bits.md): the bits of the modulus
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
