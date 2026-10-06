[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md) › [level](README.md)

# sgcl::compress::brotli::level::level

```cpp
constexpr level() noexcept;    // (1)
constexpr level(int n);        // (2)
```

1. The default level, 11 (`level::standard`).
2. The level `n`: 0 to 11. Not explicit, so an `int` converts to a level where one is taken:
   `{.level = 5}` in the options. In a constant expression a value outside those does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the level: 0 to 11 |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `n` is outside 0 to 11.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    compress::brotli::level standard;
    compress::brotli::level other = 5;
    println("{} {}", standard.value(), other.value());
    try {
        compress::brotli::level wrong(12);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
11 5
compress::brotli::level: 0..11
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::brotli::level](README.md)
