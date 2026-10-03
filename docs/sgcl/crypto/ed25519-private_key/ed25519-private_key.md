[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::private_key

```cpp
/*(1)*/ private_key(private_key&& other) noexcept;
/*(2)*/ private_key(const private_key&) = delete;
```

1. Takes the key of `other` over and zeroes it in `other`, the seed, the scalar, the prefix and the public key, with
   stores the compiler cannot drop. The key moved from is no key of any use until assigned again: every operation on
   it but the assignment, the destructor, `clone` and `==` throws `logic_error`.
2. A key is not copied by a constructor: a second one is asked for by name, with [clone](clone.md).

A key is made by the static functions: [generate](generate.md), [from_seed](from_seed.md),
[from_private_bytes](from_private_bytes.md), [from_pkcs8_der](from_pkcs8_der.md), [from_pem](from_pem.md). The
destructor, `~private_key()`, zeroes the key the same way.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

#include <stdexcept>

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    auto public_key = key.public_key();

    crypto::ed25519::private_key moved(std::move(key));
    println("{}", public_key.verify("a message", moved.sign("a message")));
    try {
        (void)key.sign("a message");
    } catch (const std::logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::ed25519::private_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): the same for an assignment
- [clone](clone.md): a second key by name
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
