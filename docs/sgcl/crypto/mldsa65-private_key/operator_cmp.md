[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::operator==(private_key)

```cpp
friend bool operator==(const private_key& a, const private_key& b);
```

Whether two keys are the same: their seeds and their public keys compared in constant time, every byte of both. A key moved
from is refused, as the module's other keys refuse one.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same key.

## Complexity

Linear in the size of the public key.

## Exceptions

`std::logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    println("{} {}", key.clone() == key, crypto::mldsa65::private_key::generate() == key);
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
