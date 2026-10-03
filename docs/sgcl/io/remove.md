[sgcl](../README.md) › [io](README.md)

# sgcl::io::remove, async_remove

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> remove(const string& path) noexcept;                       // (1)
    async::task<expected<void, error>> async_remove(const string& path) noexcept;    // (2)
}
```

Removes a file, a symbolic link (not its target) or an empty directory, Go's `os.Remove`. The name is qualified in
a program, `io::remove`: under `using namespace sgcl;` a bare `remove("x")` is the C library's `::remove(const
char*)`, an exact match for a literal, which compiles to something else.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | what to remove |

## Return value

Nothing, or the [error](error/README.md): `is_not_found()` when nothing is at the path (`std::errc::no_such_file_or_directory`),
`std::errc::directory_not_empty` for a directory with entries, or the error of the call; the operation is `remove`
and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("old.log", "x");
    (void)io::mkdir_all("cache/data");
    println("{}", io::remove("old.log").has_value());
    println("{}", io::remove("old.log").error().is_not_found());
    println("{}", io::remove("cache").error().message());
}
```

Output:

```text
true
true
remove cache: Directory not empty
```

## See also

- [remove_all](remove_all.md): a directory with everything under it
- [rename](rename.md): moves a file
