[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::private_key

```cpp
private_key(private_key&& other) noexcept;    // (1)
private_key(const private_key&) = delete;     // (2)
```

1. Takes the key of `other` over: the scalar and the public point are copied into the new object, and `other` is
   zeroed. `other` holds no key after: every operation on it throws `std::logic_error` until a key is assigned to it.
2. There is no copy: a second key of the same scalar is made by name, [clone](clone.md).

A key is made by [generate](generate.md), [from_bytes](from_bytes.md), [from_pkcs8_der](from_pkcs8_der.md),
[from_sec1_der](from_sec1_der.md) or [from_pem](from_pem.md); there is no other public constructor.

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
    auto first = crypto::p256::private_key::generate();
    auto digest = crypto::sha256::of("the message");

    crypto::p256::private_key second = std::move(first);
    println("{}", second.public_key().verify_digest(digest, second.sign_digest(digest)));
    try {
        first.sign_digest(digest);
    } catch (const logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::p256::private_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): the move assignment
- [clone](clone.md): a second key of the same scalar
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
