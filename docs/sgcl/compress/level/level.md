[sgcl](../../README.md) › [compress](../README.md) › [level](../level.md)

# sgcl::compress::level::level

```cpp
constexpr level() noexcept;    // (1)
constexpr level(int n);        // (2)
```

1. The default level, 6 (`level::standard`).
2. The level `n`: 0 to 9, or `level::huffman_only`. Not explicit, so an `int` converts to a level where one is
   taken: `{.level = 9}` in the options of a format. In a constant expression a value outside those does not
   compile.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the level: 0 to 9, or `level::huffman_only` (-2) |

## Complexity

Constant.

## Exceptions

- (1) None.
- (2) `std::invalid_argument` when `n` is neither 0 to 9 nor `huffman_only`.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

int main() {
    compress::level standard;
    compress::level smallest = 9;
    println("{} {}", standard.value(), smallest.value());
    try {
        compress::level wrong(12);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
6 9
compress::level: 0..9 or level::huffman_only
```

## See also

- [value](value.md): the level as an `int`
- [sgcl::compress::level](../level.md)
