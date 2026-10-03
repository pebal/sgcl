[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::operator=

```cpp
/*(1)*/ private_key& operator=(private_key&& other) noexcept;
/*(2)*/ private_key& operator=(const private_key& other) = delete;
```

1. Takes the numbers of `other` over and leaves `other` empty; the numbers this key held are zeroed and freed. A
   key moved onto itself stays the key it was.
2. Deleted: a copy of a secret is made by name, with [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key moved in |

## Return value

`*this`.

## Complexity

Linear in the bits of the modulus: the old numbers are zeroed.

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
    auto pub = key.public_key();
    other = std::move(key);
    println("{}", other.public_key() == pub);
}
```

Output:

```text
true
```

## See also

- [(constructor)](rsa-private_key.md)
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
