[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [level](README.md)

# sgcl::compress::brotli::level::value

```cpp
constexpr int value() const noexcept;
```

Returns the level as an `int`: 0 to 11.

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
    compress::brotli::options o{.level = 5};
    println("{}", o.level.value());
}
```

Output:

```text
5
```

## See also

- [(constructor)](' + n + '-level.md)
- [sgcl::compress::brotli::level](README.md)
