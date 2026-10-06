[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the library was closed ([close](close.md)), through this handle or a copy of it.

## Parameters

None.

## Return value

`true` after [close](close.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library z = io::open_library(io::library_file_name("z")).value();
    println("{}", z.is_closed());
    z.close();
    println("{}", z.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md)
- [sgcl::io::library](README.md)
