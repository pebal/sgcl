[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::max

```cpp
static constexpr uuid max() noexcept;
```

The max UUID, all ones (RFC 9562 §5.10): greater than every other, an end of a range.

## Parameters

None.

## Return value

The max UUID.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::uuid::max());
    println(encoding::uuid::v7() < encoding::uuid::max());
}
```

Output:

```text
ffffffff-ffff-ffff-ffff-ffffffffffff
true
```

## See also

- [nil](nil.md)
- [sgcl::encoding::uuid](README.md)
