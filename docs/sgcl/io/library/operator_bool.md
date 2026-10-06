[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a library; a closed one is still held ([is_closed](is_closed.md) tells it apart).

## Parameters

None.

## Return value

`true` when the handle holds a library.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::library lib;
    println("{}", static_cast<bool>(lib));
    lib = io::open_library(io::library_file_name("z")).value();
    println("{}", static_cast<bool>(lib));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](library.md)
- [sgcl::io::library](README.md)
