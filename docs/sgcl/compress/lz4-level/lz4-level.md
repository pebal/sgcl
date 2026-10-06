[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md) › [level](README.md)

# sgcl::compress::lz4::level::level

```cpp
constexpr level() noexcept;    // (1)
constexpr level(int n);        // (2)
```

1. The default level, 1 (`level::standard`).
2. The level `n`: 1 to 12, or -1 to -65537 for acceleration. Not explicit, so an `int` converts to a level where one is taken:
   `{.level = 9}` in the options. In a constant expression a value outside those does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the level: 1 to 12, or -1 to -65537 for acceleration |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `n` is outside 1 to 12, or -1 to -65537 for acceleration.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    compress::lz4::level standard;
    compress::lz4::level other = 9;
    println("{} {}", standard.value(), other.value());
    try {
        compress::lz4::level wrong(13);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
1 9
compress::lz4::level: 1..12, or -1..-65537 for acceleration
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::lz4::level](README.md)
