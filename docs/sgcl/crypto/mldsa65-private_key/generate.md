[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::generate

```cpp
static private_key generate() noexcept;
```

A new key: a seed ξ of 32 bytes of [crypto::random](../random/README.md) and the key generation of FIPS 204 (Algorithm 1) over it, the expanded key made once.

## Parameters

None.

## Return value

The key.

## Complexity

Constant: one key generation (the matrix Â sampled, s1, s2 and t transformed).

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    auto sig = key.sign("hello");
    println("{} {}", sig.size(), key.public_key().verify("hello", sig));
}
```

Output:

```text
3309 true
```

## See also

- [from_seed](from_seed.md): the key of a seed kept
- [sgcl::crypto::mldsa65::private_key](README.md)
