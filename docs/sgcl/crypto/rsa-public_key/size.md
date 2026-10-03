[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::size

```cpp
size_t size() const;
```

Returns the length of the modulus in bytes: the length of every signature and of every ciphertext under the key, 256
for a 2048-bit key.

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

    crypto::rsa::public_key pub = key.public_key();
    auto sig = key.sign_digest(crypto::hash_id::sha256, crypto::sha256::of("abc"));
    println("{} {}", pub.size(), sig.size());
}
```

Output:

```text
256 256
```

## See also

- [bits](bits.md): the bits of the modulus
- [max_oaep_message_size](max_oaep_message_size.md): the longest message OAEP takes
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
