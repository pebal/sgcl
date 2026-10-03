[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::private_key::clone

```cpp
private_key clone() const;
```

Returns a second key of the same seed, with its scalar, its prefix and its public key copied rather than computed
again. A key has no copy constructor, so that every copy of it is one the program asked for by name: one more place
the secret lies, zeroed when that key goes in turn. A key moved from has no seed to give and is refused.

## Parameters

None.

## Return value

A key of the same seed.

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
    auto key = crypto::ed25519::private_key::generate();
    auto copy = key.clone();
    println("{} {}", copy == key, copy.sign("a message") == key.sign("a message"));
}
```

Output:

```text
true true
```

## See also

- [(constructor)](ed25519-private_key.md): the move
- [operator==](operator_cmp.md): the comparison in constant time
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
