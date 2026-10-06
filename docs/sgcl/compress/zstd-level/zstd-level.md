[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [level](README.md)

# sgcl::compress::zstd::level::level

```cpp
constexpr level() noexcept;    // (1)
constexpr level(int n);        // (2)
```

1. The default level, 3 (`level::standard`).
2. The level `n`: 1 to 22, or -1 to -131072 for --fast. Not explicit, so an `int` converts to a level where one is taken:
   `{.level = 19}` in the options. In a constant expression a value outside those does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the level: 1 to 22, or -1 to -131072 for --fast |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `n` is outside 1 to 22, or -1 to -131072 for --fast.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    compress::zstd::level standard;
    compress::zstd::level other = 19;
    println("{} {}", standard.value(), other.value());
    try {
        compress::zstd::level wrong(23);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
3 19
compress::zstd::level: 1..22, or -1..-131072 for --fast
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::zstd::level](README.md)
