[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::is_nil

```cpp
constexpr bool is_nil() const noexcept;
```

Whether every byte is zero: the nil UUID, what [uuid()](uuid.md) is — a key not yet given.

## Parameters

None.

## Return value

`true` for the nil UUID.

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
    println("{} {}", encoding::uuid().is_nil(), encoding::uuid::v4().is_nil());
}
```

Output:

```text
true false
```

## See also

- [nil](nil.md)
- [sgcl::encoding::uuid](README.md)
