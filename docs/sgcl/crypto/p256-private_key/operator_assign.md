[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](README.md)

# sgcl::crypto::p256::private_key::operator=

```cpp
private_key& operator=(private_key&& other) noexcept;    // (1)
private_key& operator=(const private_key&) = delete;     // (2)
```

1. Replaces the key with the key of `other` and zeroes `other`: the scalar this object held before is overwritten,
   and `other` holds no key after, so every operation on it throws `std::logic_error` until a key is assigned to it.
   An object moved from may be assigned to again. A key assigned to itself stays as it is.
2. There is no copy assignment: a second key is made by name, [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key to take over |

## Return value

`*this`.

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
    auto key = crypto::p256::private_key::generate();
    auto next = crypto::p256::private_key::generate();
    auto next_public = next.public_key();

    key = std::move(next);  // the old scalar is overwritten
    println("{}", key.public_key() == next_public);

    next = key.clone();  // a key moved from takes a key again
    println("{}", next.public_key() == next_public);
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](p256-private_key.md): the move
- [clone](clone.md): a second key of the same scalar
- [sgcl::crypto::p256::private_key](README.md)
