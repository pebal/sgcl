[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [level](README.md)

# sgcl::compress::bzip2::level::value

```cpp
constexpr int value() const noexcept;
```

Returns the level as an `int`: 1 to 9.

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
    compress::bzip2::options o{.level = 1};
    println("{}", o.level.value());
}
```

Output:

```text
1
```

## See also

- [(constructor)](' + n + '-level.md)
- [sgcl::compress::bzip2::level](README.md)
