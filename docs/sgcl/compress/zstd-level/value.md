[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [level](README.md)

# sgcl::compress::zstd::level::value

```cpp
constexpr int value() const noexcept;
```

Returns the level as an `int`: 1 to 22, or -1 to -131072 for --fast.

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
    compress::zstd::options o{.level = 19};
    println("{}", o.level.value());
}
```

Output:

```text
19
```

## See also

- [(constructor)](' + n + '-level.md)
- [sgcl::compress::zstd::level](README.md)
