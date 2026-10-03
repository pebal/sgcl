[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [private_key](../p256-private_key.md)

# sgcl::crypto::p256::private_key::clone

```cpp
private_key clone() const;
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
    auto key = crypto::p256::private_key::generate();
    auto copy = key.clone();
    auto digest = crypto::sha256::of("the message");

    println("{}", copy.public_key() == key.public_key());
    println("{}", copy.sign_digest(digest, crypto::deterministic)
                      == key.sign_digest(digest, crypto::deterministic));
}
```

Output:

```text
true
true
```

## See also

- [(constructor)](p256-private_key.md): the move
- [to_pkcs8_der](to_pkcs8_der.md): the key kept to be read again later
- [sgcl::crypto::p256::private_key](../p256-private_key.md)
