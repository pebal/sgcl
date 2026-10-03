[sgcl](../README.md) › [io](README.md)

# sgcl::io::working_dir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> working_dir() noexcept;
}
```

Returns the absolute path of the process's working directory: Go's `os.Getwd`, the C library's `getcwd`. A relative
path given to any function of the module is read from there.

## Parameters

None.

## Return value

The path, or the [error](error/README.md) of `getcwd` (the directory removed meanwhile, a path longer than 4095 bytes); the
operation is `getcwd`.

## Complexity

Linear in the length of the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("work");
    (void)io::chdir("work");
    if (auto dir = io::working_dir()) {
        println("{}", io::path::base(*dir));
    }
}
```

Output:

```text
work
```

## See also

- [chdir](chdir.md): changes the working directory
- [path::abs](path/abs.md): a path made absolute against the working directory
