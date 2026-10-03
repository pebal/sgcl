[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_dir, async_read_dir

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<vector<directory_entry>, error> read_dir(const string& path) noexcept;             // (1)
    async::task<expected<vector<directory_entry>, error>> async_read_dir(const string& path)    // (2)
        noexcept;
}
```

Returns the entries of the directory at `path`, sorted by name, `.` and `..` left out: Go's `os.ReadDir`. Each
[directory_entry](directory_entry/README.md) has the name, the path (`path` joined with the name) and the type, which come
from the listing itself, a symbolic link being of type `file_type::symlink`, with no stat per entry; the rest of what
a stat says is [info()](directory_entry/info.md), when asked.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the directory to list |

## Return value

The entries, or the [error](error/README.md) (`is_not_found()` when nothing is at the path, `std::errc::not_a_directory`
for a file); the operation is `read_dir` and the path `path`.

## Complexity

Linear in the number of entries, plus their sort.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("project/src");
    (void)io::write_file("project/README.md", "# project");
    (void)io::write_file("project/build.sh", "make");
    auto entries = io::read_dir("project");
    if (!entries) {
        return 1;
    }
    for (const io::directory_entry& e : *entries) {
        println("{} {}", e.path, e.is_directory() ? "dir" : "file");
    }
}
```

Output:

```text
project/README.md file
project/build.sh file
project/src dir
```

## See also

- [walk_dir](walk_dir.md): every entry under a directory
- [directory_entry](directory_entry/README.md): an entry
- [path::glob](path/glob.md): the paths that match a pattern
