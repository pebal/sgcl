[sgcl](../README.md) › [io](README.md)

# sgcl::io::mkdir, async_mkdir

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<void, error> mkdir(const string& path,
                                        permissions p = permissions(0777)) noexcept;
    /*(2)*/ async::task<expected<void, error>> async_mkdir(const string& path,
                                                         permissions p = permissions(0777))
                noexcept;
}
```

Makes one directory, Go's `os.Mkdir`, the system's `mkdir`. The parent must exist, and nothing may be at the path
already. The permissions are those the program's umask leaves of `p`.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the directory to make |
| `p` | its permissions, before the umask; `0777` by default |

## Return value

Nothing, or the [error](error.md) of the call: `is_exists()` when something is at the path, `is_not_found()` when
the parent is missing; the operation is `mkdir` and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::mkdir("logs").has_value());
    println("{}", io::mkdir("logs").error().is_exists());
    println("{}", io::mkdir("a/b").error().message());
}
```

Output:

```text
true
true
mkdir a/b: No such file or directory
```

## See also

- [mkdir_all](mkdir_all.md): the whole chain, existing ones left alone
- [make_temp_dir](make_temp_dir.md): a new directory of a name of its own
- [remove](remove.md): removes an empty directory
- [permissions](permissions.md): the mode bits
