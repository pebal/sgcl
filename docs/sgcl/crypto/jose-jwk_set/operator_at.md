[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk_set](README.md)

# sgcl::crypto::jose::jwk_set::operator[]

```cpp
const jwk& operator[](size_t i) const noexcept;
```

The key at index `i`, in the order of the set.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the index, below [size](size.md); past it the behaviour is undefined (an assertion in a debug build) |

## Return value

The key.

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
    crypto::jose::jwk_set set{crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa, {.kid = "x"})};
    println("{}", set[0].kid());
}
```

Output:

```text
x
```

## See also

- [keys](keys.md)
- [sgcl::crypto::jose::jwk_set](README.md)
