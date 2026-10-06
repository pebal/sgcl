[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::size

```cpp
size_t size() const noexcept;
```

The number of keys.

## Parameters

None.

## Return value

The number.

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
    crypto::jose::jwk_set set{crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa)};
    println("{}", set.size());
}
```

Output:

```text
1
```

## See also

- [empty](empty.md)
- [sgcl::crypto::jose::jwk_set](README.md)
