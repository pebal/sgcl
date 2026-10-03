[sgcl](../README.md) › [io](README.md)

# sgcl::io::remove_all, async_remove_all

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> remove_all(const string& path) noexcept;                       // (1)
    async::task<expected<void, error>> async_remove_all(const string& path) noexcept;    // (2)
}
```

Removes the path and everything under it, `rm -rf`, Go's `os.RemoveAll`, through `std::filesystem::remove_all`. A
symbolic link is removed, not followed. Nothing at the path is no error, so the call is idempotent. A path whose last
element is `.` or `..` is refused before anything is removed, as Go refuses `.`: the system removes neither, and
would refuse it only after everything under it was gone.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | what to remove, with everything under it |

## Return value

Nothing, or the [error](error.md) of the first entry that could not be removed, or `std::errc::invalid_argument` for
a path whose last element is `.` or `..`; the operation is `remove_all` and the path `path`.

## Complexity

Linear in the number of entries under the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("build/obj/debug");
    (void)io::write_file("build/obj/debug/main.o", "x");
    println("{}", io::remove_all("build").has_value());
    println("{} {}", io::exists("build"), io::remove_all("build").has_value());
}
```

Output:

```text
true
false true
```

## See also

- [remove](remove.md): one file or an empty directory
- [mkdir_all](mkdir_all.md): a tree made
