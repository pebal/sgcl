[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::seed

```cpp
secret<32> seed() const;
```

The seed ξ the key was made of (Go's `Bytes`): the one form of an ML-DSA private key, from which [from_seed](from_seed.md) makes the same key again.

## Parameters

None.

## Return value

The seed, as a [secret\<32\>](../secret/README.md): zeroed when it goes, never in managed memory.

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
    auto key = crypto::mldsa65::private_key::generate();
    auto again = crypto::mldsa65::private_key::from_seed(key.seed()).value();
    println("{}", again == key);
}
```

Output:

```text
true
```

## See also

- [from_seed](from_seed.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
