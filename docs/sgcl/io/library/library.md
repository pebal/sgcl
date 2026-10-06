[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::library

```cpp
library() noexcept = default;              // (1)
library(const library& other) noexcept;    // (2), implicitly declared
```

1. A handle that holds no library: `!lib`.
2. A handle of the same library as `other`.

A library is loaded by [open_library](../open_library.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose library this one shares |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library none;
    io::library z = io::open_library(io::library_file_name("z")).value();
    io::library same = z;
    println("{} {}", static_cast<bool>(none), same == z);
}
```

Output:

```text
false true
```

## See also

- [open_library](../open_library.md)
- [sgcl::io::library](README.md)
