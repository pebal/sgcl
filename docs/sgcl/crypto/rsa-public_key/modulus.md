[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::modulus

```cpp
vector<byte> modulus() const;
```

Returns the modulus n as big-endian bytes, [size](size.md) of them, without a sign byte: what a JWK's `n` holds
(base64url of these bytes) and what [from_modulus](from_modulus.md) takes back.

## Parameters

None.

## Return value

n, [size](size.md) bytes, big-endian.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    auto n = key.public_key().modulus();
    println("{} bytes, {}...", n.size(), encoding::hex::encode(n.as_slice().subslice(0, 8)));
}
```

Output:

```text
256 bytes, c510933da42bedba...
```

## See also

- [exponent](exponent.md): e
- [from_modulus](from_modulus.md): a key from the two numbers
- [sgcl::crypto::rsa::public_key](README.md)
