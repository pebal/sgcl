[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::public_key

```cpp
mldsa65::public_key public_key() const;
```

The key's [public_key](../mldsa65-public_key/README.md), the one to publish, made of the public key's bytes kept with the private key (its matrix Â made again).

## Parameters

None.

## Return value

The public key, a handle of one word.

## Complexity

Constant: one sampling of Â.

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    println("{} bytes", key.public_key().bytes().size());
}
```

Output:

```text
1952 bytes
```

## See also

- [mldsa65::public_key](../mldsa65-public_key/README.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
