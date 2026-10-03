[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [ecdh_key](../p256-ecdh_key.md)

# sgcl::crypto::p256::ecdh_key::clone

```cpp
ecdh_key clone() const;
```

A second key of the same scalar: the copy the class does not have, made by name so that a secret is never copied by
accident. The two keys are independent objects; each zeroes its scalar when it goes. A key moved from has no scalar
to give and is refused, as every other operation refuses it.

## Parameters

None.

## Return value

A new key of the same scalar.

## Complexity

Constant.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::ecdh_key::generate();
    auto copy = key.clone();
    auto peer = crypto::p256::ecdh_key::generate().public_key();

    println("{}", copy.shared_secret(peer) == key.shared_secret(peer));
}
```

Output:

```text
true
```

## See also

- [(constructor)](p256-ecdh_key.md): the move
- [sgcl::crypto::p256::ecdh_key](../p256-ecdh_key.md)
