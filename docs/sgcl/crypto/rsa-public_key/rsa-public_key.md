[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](../rsa-public_key.md)

# sgcl::crypto::rsa::public_key::public_key

```cpp
/*(1)*/ public_key(const public_key& other) noexcept;
/*(2)*/ public_key(public_key&& other) noexcept;
```

Copies or moves a key. There is no other constructor: a key is made by
[from_modulus](from_modulus.md), [from_pkcs1_der](from_pkcs1_der.md), [from_pkix_der](from_pkix_der.md) or
[private_key::public_key](../rsa-private_key/public_key.md).

1. A key of its own with the modulus and the exponent of `other`, and its own copy of the Montgomery constants.
2. Takes the numbers of `other` over and leaves `other` empty: a call on it that reads the modulus is
   `logic_error`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key copied or moved |

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

    crypto::rsa::public_key pub = key.public_key();
    crypto::rsa::public_key copy = pub;
    crypto::rsa::public_key moved = std::move(pub);
    println("{} {}", copy == moved, copy.bits());
    try {
        pub.to_pkix_der();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true 2048
sgcl::crypto::rsa::public_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): assigns another key
- [from_pkix_der](from_pkix_der.md): a key read from its encoding
- [sgcl::crypto::rsa::public_key](../rsa-public_key.md)
