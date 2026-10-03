[sgcl](../README.md) › [io](README.md)

# sgcl::io::executable

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> executable() noexcept;
}
```

Returns the path of the running program's binary with its symbolic links resolved, Go's `os.Executable`: on macOS
the loader's path (`_NSGetExecutablePath`) through `realpath`, on Linux the link `/proc/self/exe`. A program finds
the files it ships beside itself from there.

## Parameters

None.

## Return value

The path, or an [error](error/README.md) whose operation is `executable`: `std::errc::filename_too_long` for a path past
4095 bytes, or the error of `realpath` or `readlink`.

## Complexity

Linear in the length of the path, and the resolution of its links.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto self = io::executable()) {
        println("{}", io::path::join(io::path::dir(*self), "data"));
    }
}
```

## See also

- [args](args.md): the command line, the name as the program was started under first
- [path::dir](path/dir.md): the directory of a path
