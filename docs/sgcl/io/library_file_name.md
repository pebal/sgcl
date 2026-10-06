[sgcl](../README.md) › [io](README.md)

# sgcl::io::library_file_name

```cpp
#include "sgcl/io/library.h"   // or "sgcl/io.h"

namespace sgcl::io {
    string library_file_name(const string& name) noexcept;
}
```

The platform's file name of a library: `libz.dylib` for `z` on macOS, `libz.so` on Linux, `z.dll` on Windows. The
loader's search then finds it ([open_library](open_library.md)).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the library's name, without prefix or suffix |

## Return value

The file name.

## Complexity

Linear in the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::library_file_name("sqlite3"));
}
```

Sample output:

```text
libsqlite3.dylib
```

## See also

- [open_library](open_library.md)
