[sgcl](../README.md) › [io](README.md)

# sgcl::io::lstat, async_lstat

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<file_info, error> lstat(const string& path) noexcept;
    /*(2)*/ async::task<expected<file_info, error>> async_lstat(const string& path) noexcept;
}
```

Returns what the file system says about the file at `path` without following a symbolic link: of a link, the
link itself, its type `file_type::symlink`. Go's `os.Lstat`, the system's `lstat`; for anything else the same as
[stat](stat.md).

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file or the link |

## Return value

What the file system says, or the [error](error.md) of the call; the operation is `lstat` and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("target.txt", "data");
    (void)io::symlink("target.txt", "link.txt");
    auto link = io::lstat("link.txt");
    auto target = io::stat("link.txt");
    println("{} {}", link->is_symlink(), target->is_regular());
}
```

Output:

```text
true true
```

## See also

- [stat](stat.md): follows the link
- [symlink](symlink.md), [read_link](read_link.md): make a link and read where it points
- [directory_entry::info](directory_entry/info.md): the lstat of an entry of a listing
