[sgcl](../README.md) › [io](README.md)

# sgcl::io::link, async_link

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> link(const string& target, const string& link) noexcept;    // (1)
    async::task<expected<void, error>> async_link(const string& target,               // (2)
                                                  const string& link) noexcept;
}
```

Makes a hard link: `link` a second name of the file `target` names (`link(2)`, Go's `os.Link`). Both names are the
same file — its bytes, its permissions — and [file_info](file_info/README.md)`::links` counts its names; the file
goes when the last of them is removed. [symlink](symlink.md) makes a symbolic link instead, a file that names
another.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `target` | the file |
| `link` | its new name |

## Return value

Nothing, or the [error](error/README.md), its operation `link` and its path `link`: `is_exists()` for a name taken,
`is_not_found()` for a target missing, `EPERM` for a directory, `EXDEV` across file systems.

## Complexity

One system call.

## Exceptions

- (1) None.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("data.bin", "bytes");
    io::link("data.bin", "data.again").value();
    println("{}", io::stat("data.bin").value().links);
    io::append_file("data.again", "+");
    println("{}", io::read_text("data.bin").value());
}
```

Output:

```text
2
bytes+
```

## See also

- [symlink](symlink.md), [file_info](file_info/README.md)
