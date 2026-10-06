[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::empty

```cpp
bool empty() const noexcept;
```

Whether the set has no key.

## Parameters

None.

## Return value

`true` for a set without keys.

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
    crypto::jose::jwk_set set;
    println("{}", set.empty());
}
```

Output:

```text
true
```

## See also

- [size](size.md)
- [sgcl::crypto::jose::jwk_set](README.md)
