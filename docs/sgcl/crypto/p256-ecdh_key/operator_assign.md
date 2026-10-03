[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](README.md)

# sgcl::crypto::p256::ecdh_key::operator=

```cpp
ecdh_key& operator=(ecdh_key&& other) noexcept;    // (1)
ecdh_key& operator=(const ecdh_key&) = delete;     // (2)
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
    // a key for each session: the next one replaces the last
    auto key = crypto::p256::ecdh_key::generate();
    auto first_public = key.public_key();

    key = crypto::p256::ecdh_key::generate();
    println("{}", key.public_key() == first_public);
}
```

Output:

```text
false
```

## See also

- [(constructor)](p256-ecdh_key.md): the move
- [clone](clone.md): a second key of the same scalar
- [sgcl::crypto::p256::ecdh_key](README.md)
