[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](../ed25519-private_key.md)

# sgcl::crypto::ed25519::operator== (sgcl::crypto::ed25519::private_key)

```cpp
friend bool operator==(const private_key& a, const private_key& b);
```

Compares the seeds of two keys in constant time, as [constant_time::equal](../constant_time/equal.md) does: two
keys of one seed are the same key, with the same scalar, prefix and public key. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys compared |

## Return value

Whether the keys have the same seed.

## Complexity

Constant.

## Exceptions

`logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::ed25519::private_key::generate();
    auto other = crypto::ed25519::private_key::generate();
    println("{} {}", key == key.clone(), key == other);
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md): a second key of the same seed
- [sgcl::crypto::ed25519::private_key](../ed25519-private_key.md)
