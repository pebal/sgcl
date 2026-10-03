[sgcl](../README.md) › [io](README.md)

# sgcl::io::symlink, async_symlink

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> symlink(const string& target, const string& link) noexcept;    // (1)
    async::task<expected<void, error>> async_symlink(const string& target,               // (2)
                                                   const string& link) noexcept;
}
```

Makes `link` a symbolic link to `target`, Go's `os.Symlink`, the system's `symlink`. The target is stored as it is
written, and a relative one is read from the directory of the link, not from the working directory; it need not
exist.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `target` | what the link points to |
| `link` | the path of the link |

## Return value

Nothing, or the [error](error.md) of the call (`is_exists()` when something is at `link`); the operation is
`symlink` and the path `link`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("releases");
    (void)io::write_file("releases/v2.txt", "the second");
    println("{}", io::symlink("releases/v2.txt", "current").has_value());
    println("{}", io::read_text("current").value_or(""));
    println("{}", io::symlink("x", "current").error().is_exists());
}
```

Output:

```text
true
the second
true
```

## See also

- [read_link](read_link.md): where a link points
- [lstat](lstat.md): the link itself
