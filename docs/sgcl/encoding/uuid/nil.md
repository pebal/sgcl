[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::nil

```cpp
static constexpr uuid nil() noexcept;
```

The nil UUID, all zeros (RFC 9562 §5.9): what [uuid()](uuid.md) is.

## Parameters

None.

## Return value

The nil UUID.

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
    println("{} {}", encoding::uuid::nil(), encoding::uuid::nil().is_nil());
}
```

Output:

```text
00000000-0000-0000-0000-000000000000 true
```

## See also

- [max](max.md)
- [is_nil](is_nil.md)
- [sgcl::encoding::uuid](README.md)
