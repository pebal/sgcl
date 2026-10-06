[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md) › [level](README.md)

# sgcl::compress::lz4::level::value

```cpp
constexpr int value() const noexcept;
```

Returns the level as an `int`: 1 to 12, or -1 to -65537 for acceleration.

## Parameters

None.

## Return value

The level.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::lz4::options o{.level = 9};
    println("{}", o.level.value());
}
```

Output:

```text
9
```

## See also

- [(constructor)](lz4-level.md)
- [sgcl::compress::lz4::level](README.md)
