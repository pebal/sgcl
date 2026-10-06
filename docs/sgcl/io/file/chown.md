[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::chown, async_chown

```cpp
expected<void, error> chown(int uid, int gid) const noexcept;                       // (1)
async::task<expected<void, error>> async_chown(int uid, int gid) const noexcept;    // (2)
```

Changes the owner and the group of the open file (`fchown`): a `uid` or a `gid` of `-1` keeps that one.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `uid`, `gid` | the new owner and group; `-1` keeps one |

## Return value

Nothing, or the [error](../error/README.md), its operation `chown`: `errc::closed` for a closed file,
`is_permission()` for a process that may not give it away.

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
    io::file f = io::create("mine.txt").value();
    println("{}", f.chown(-1, -1).has_value());
    f.close();
    println("{}", f.chown(-1, -1).error().code() == io::errc::closed);
}
```

Output:

```text
true
true
```

## See also

- [chmod](chmod.md), [io::chown](../chown.md)
- [sgcl::io::file](README.md)
