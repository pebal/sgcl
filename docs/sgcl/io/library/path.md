[sgcl](../../README.md) › [io](../README.md) › [library](README.md)

# sgcl::io::library::path

```cpp
const string& path() const noexcept;
```

The path or the name the library was opened with, as given to [open_library](../open_library.md).

## Parameters

None.

## Return value

The path.

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
    println("{}", z.path());
}
```

Output:

```text
libz.dylib
```

## See also

- [library_file_name](../library_file_name.md)
- [sgcl::io::library](README.md)
