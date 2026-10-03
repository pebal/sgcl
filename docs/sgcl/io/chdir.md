[sgcl](../README.md) › [io](README.md)

# sgcl::io::chdir

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> chdir(const string& path) noexcept;
}
```

Makes `path` the working directory of the process: Go's `os.Chdir`, the C library's `chdir`. The working directory
is the process's, shared by every thread: the relative paths of all of them are read from the new one at once. The
name is qualified in a program, `io::chdir`: under `using namespace sgcl;` a bare `chdir("x")` is the C library's.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the new working directory, absolute or relative to the current one |

## Return value

Nothing, or the [error](error/README.md) of the call: not found, not a directory, permission denied. The operation is
`chdir` and the path `path`.

## Complexity

Linear in the length of the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("site/static");
    if (auto r = io::chdir("site/static"); r) {
        println("{}", io::path::base(*io::working_dir()));
    }
    if (auto r = io::chdir("nowhere"); !r) {
        println("{}, not found: {}", r.error().message(), r.error().is_not_found());
    }
}
```

Output:

```text
static
chdir nowhere: No such file or directory, not found: true
```

## See also

- [working_dir](working_dir.md): the working directory
- [command](command/README.md): `dir`, the working directory of a child, which leaves the program's as it is
