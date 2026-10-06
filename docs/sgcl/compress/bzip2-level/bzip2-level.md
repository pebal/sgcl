[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [level](README.md)

# sgcl::compress::bzip2::level::level

```cpp
constexpr level() noexcept;    // (1)
constexpr level(int n);        // (2)
```

1. The default level, 9 (`level::standard`).
2. The level `n`: 1 to 9. Not explicit, so an `int` converts to a level where one is taken:
   `{.level = 1}` in the options. In a constant expression a value outside those does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the level: 1 to 9 |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `n` is outside 1 to 9.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    compress::bzip2::level standard;
    compress::bzip2::level other = 1;
    println("{} {}", standard.value(), other.value());
    try {
        compress::bzip2::level wrong(10);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
9 1
compress::bzip2::level: 1..9
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::bzip2::level](README.md)
