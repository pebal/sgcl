[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::keys

```cpp
slice<const jwk> keys() const noexcept;
```

The keys, in the order of the set.

## Parameters

None.

## Return value

A slice of the keys, valid while the set is not changed.

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
    crypto::jose::jwk_set set{crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa, {.kid = "a"}),
                              crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "b"})};
    for (const auto& k : set.keys()) {
        println("{} {}", k.kid(), k.crv());
    }
}
```

Output:

```text
a Ed25519
b P-256
```

## See also

- [operator[]](operator_at.md)
- [sgcl::crypto::jose::jwk_set](README.md)
