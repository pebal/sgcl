[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [private_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::operator==(private_key)

```cpp
friend bool operator==(const private_key& a, const private_key& b);
```

Whether two keys are the same: their 4n bytes compared in constant time. A key moved from is refused, as the
module's other keys refuse one.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same key.

## Complexity

Constant.

## Exceptions

`std::logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    println("{} {}", key.clone() == key, crypto::slhdsa_sha2_128s::private_key::generate() == key);
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md)
- [sgcl::crypto::slhdsa_sha2_128s::private_key](README.md)
