[sgcl](../README.md) › [io](README.md)

# sgcl::io::chmod, async_chmod

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> chmod(const string& path, permissions p) noexcept;    // (1)
    async::task<expected<void, error>> async_chmod(const string& path,          // (2)
                                                 permissions p) noexcept;
}
```

Sets the permissions of the file at `path` to `p`, following a symbolic link: Go's `os.Chmod`, the system's
`chmod`. The umask does not apply.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `p` | its new permissions, the set-id and sticky bits included |

## Return value

Nothing, or the [error](error.md) of the call; the operation is `chmod` and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("deploy.sh", "#!/bin/sh\n");
    (void)io::chmod("deploy.sh", io::permissions(0755));
    println("{:o}", unsigned(io::stat("deploy.sh")->mode));
    println("{}", io::chmod("none.sh", io::permissions::all).error().message());
}
```

Output:

```text
755
chmod none.sh: No such file or directory
```

## See also

- [permissions](permissions.md): the mode bits
- [file::chmod](file/chmod.md): the same through an open file
- [stat](stat.md): the permissions read
