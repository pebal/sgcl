[sgcl](../../README.md) › [compress](../README.md) › [level](../level.md)

# sgcl::compress::level::value

```cpp
constexpr int value() const noexcept;
```

Returns the level as an `int`: 0 to 9, or `level::huffman_only` (-2).

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
    compress::gzip::options o{.level = compress::level::smallest};
    println("{}", o.level.value());
    println("{}", compress::level(compress::level::huffman_only).value());
}
```

Output:

```text
9
-2
```

## See also

- [(constructor)](level.md)
- [sgcl::compress::level](../level.md)
