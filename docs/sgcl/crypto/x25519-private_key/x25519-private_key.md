[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::private_key::private_key

```cpp
/*(1)*/ private_key(private_key&& other) noexcept;
/*(2)*/ private_key(const private_key&) = delete;
```

1. Takes the key of `other` over and zeroes it in `other`, with stores the compiler cannot drop. The key moved from
   is no key of any use until assigned again: every operation on it but the assignment, the destructor, `clone` and
   `==` throws `logic_error`.
2. A key is not copied by a constructor: a second one is asked for by name, with [clone](clone.md).

A key is made by the static functions: [generate](generate.md), [from_bytes](from_bytes.md),
[from_pkcs8_der](from_pkcs8_der.md), [from_pem](from_pem.md). The destructor, `~private_key()`, zeroes the key the
same way.

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
    auto key = crypto::x25519::private_key::generate();
    auto public_key = key.public_key();

    crypto::x25519::private_key moved(std::move(key));
    println("{}", moved.public_key() == public_key);
    try {
        (void)key.public_key();
    } catch (const std::logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true
sgcl::crypto::x25519::private_key: used after being moved from
```

## See also

- [operator=](operator_assign.md): the same for an assignment
- [clone](clone.md): a second key by name
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
