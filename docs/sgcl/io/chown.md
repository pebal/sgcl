[sgcl](../README.md) › [io](README.md)

# sgcl::io::chown, async_chown

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> chown(const string& path, int uid, int gid) noexcept;                       // (1)
    async::task<expected<void, error>> async_chown(const string& path, int uid, int gid) noexcept;    // (2)
}
```

Changes the owner and the group of a file (`chown(2)`, Go's `os.Chown`): a `uid` or a `gid` of `-1` keeps that one.
A symbolic link is followed; [lchown](lchown.md) changes the link itself, [file::chown](file/chown.md) an open file.
[file_info](file_info/README.md)`::uid` and `::gid` read them.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `uid`, `gid` | the new owner and group; `-1` keeps one |

## Return value

Nothing, or the [error](error/README.md), its operation `chown`: `is_permission()` for a process that may not give a
file away, `is_not_found()` for a missing file.

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
    io::write_file("owned.txt", "x");
    io::file_info info = io::stat("owned.txt").value();
    println("{}", io::chown("owned.txt", -1, int(info.gid)).has_value());
    println("{}", io::chown("missing.txt", -1, -1).error().is_not_found());
}
```

Output:

```text
true
true
```

## See also

- [lchown](lchown.md), [file::chown](file/chown.md), [chmod](chmod.md)
