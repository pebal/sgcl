[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::private_key

```cpp
private_key(private_key&& other) noexcept;         // (1)
private_key(const private_key& other) = delete;    // (2)
```

Moves a key. There is no other constructor: a key is made by [generate](generate.md), read by
[from_pkcs1_der](from_pkcs1_der.md), [from_pkcs8_der](from_pkcs8_der.md) or [from_pem](from_pem.md), or copied by
[clone](clone.md).

1. Takes the numbers of `other` over and leaves `other` empty: a call on it that uses the numbers is `logic_error`.
   Nothing is copied: the block of the numbers changes hands.
2. Deleted: a copy of a secret is made by name, with [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key moved |

## Complexity

Constant.

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

    crypto::rsa::private_key moved = std::move(key);
    println("{}", moved.public_key().bits());
    try {
        key.sign_digest(crypto::hash_id::sha256, crypto::sha256::of("abc"));
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
2048
sgcl::crypto::rsa::private_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): moves another key in
- [clone](clone.md): a copy by name
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
