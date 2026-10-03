[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::ecdh_key

```cpp
/*(1)*/ ecdh_key(ecdh_key&& other) noexcept;
/*(2)*/ ecdh_key(const ecdh_key&) = delete;
```

1. Takes the key of `other` over: the scalar and the public point are copied into the new object, and `other` is
   zeroed. `other` holds no key after: every operation on it throws `std::logic_error` until a key is assigned to it.
2. There is no copy: a second key of the same scalar is made by name, [clone](clone.md).

A key is made by [generate](generate.md), [from_bytes](from_bytes.md), [from_pkcs8_der](from_pkcs8_der.md),
[from_pem](from_pem.md) or an ECDSA key's [to_ecdh](../p256-private_key/to_ecdh.md); there is no other public
constructor.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

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
    auto first = crypto::p256::ecdh_key::generate();
    auto peer = crypto::p256::ecdh_key::generate().public_key();

    crypto::p256::ecdh_key second = std::move(first);
    println("{}", second.shared_secret(peer).has_value());
    try {
        first.public_key();
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::p256::ecdh_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): the move assignment
- [clone](clone.md): a second key of the same scalar
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
