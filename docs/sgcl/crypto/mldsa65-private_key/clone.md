[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::clone

```cpp
private_key clone() const;
```

A second key of the same seed, its own copy of the expanded key: the private key is move-only, so a second one is made by name.

## Parameters

None.

## Return value

The key.

## Complexity

Linear in the size of the expanded key (50 KB for ML-DSA-65).

## Exceptions

`std::logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    auto second = key.clone();
    println("{}", second == key);
}
```

Output:

```text
true
```

## See also

- [operator==](operator_cmp.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
