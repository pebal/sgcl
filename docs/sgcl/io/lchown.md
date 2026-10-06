[sgcl](../README.md) › [io](README.md)

# sgcl::io::lchown

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> lchown(const string& path, int uid, int gid) noexcept;
}
```

Changes the owner and the group of a symbolic link itself, not of the file it names (`lchown(2)`, Go's `os.Lchown`);
of any other file, as [chown](chown.md). A `uid` or a `gid` of `-1` keeps that one.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the link |
| `uid`, `gid` | the new owner and group; `-1` keeps one |

## Return value

Nothing, or the [error](error/README.md), its operation `lchown`.

## Complexity

One system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("target.txt", "x");
    io::symlink("target.txt", "link.txt");
    println("{}", io::lchown("link.txt", -1, -1).has_value());
}
```

Output:

```text
true
```

## See also

- [chown](chown.md), [symlink](symlink.md)
