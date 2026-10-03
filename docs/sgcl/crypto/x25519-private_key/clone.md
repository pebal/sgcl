[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](README.md)

# sgcl::crypto::x25519::private_key::clone

```cpp
private_key clone() const;
```

Returns a second key of the same bytes. A key has no copy constructor, so that every copy of it is one the program
asked for by name: one more place the secret lies, zeroed when that key goes in turn. A key moved from has no bytes to
give and is refused.

## Parameters

None.

## Return value

A key of the same bytes and the same public key.

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::x25519::private_key::generate();
    auto copy = key.clone();
    println("{} {}", copy == key, copy.public_key() == key.public_key());
}
```

Output:

```text
true true
```

## See also

- [(constructor)](x25519-private_key.md): the move
- [operator==](operator_cmp.md): the comparison in constant time
- [sgcl::crypto::x25519::private_key](README.md)
