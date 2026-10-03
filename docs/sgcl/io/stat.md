[sgcl](../README.md) › [io](README.md)

# sgcl::io::stat, async_stat

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<file_info, error> stat(const string& path) noexcept;                       // (1)
    async::task<expected<file_info, error>> async_stat(const string& path) noexcept;    // (2)
}
```

Returns what the file system says about the file at `path`, following a symbolic link to its target: Go's
`os.Stat`, the system's `stat`. The [file_info](file_info/README.md) holds the name (the last element of the path), the
size, the type, the permissions and the time of the last modification. [lstat](lstat.md) describes a link itself.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works. A stat of a path on a slow or a network disk waits as a read does.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

What the file system says, or the [error](error/README.md) of the call (`is_not_found()` when nothing is at the path,
`is_permission()`); the operation is `stat` and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("notes.txt", "twelve bytes");
    if (auto info = io::stat("notes.txt")) {
        println("{} {} {} {}", info->name, info->size, info->is_regular(), info->is_directory());
    }
    if (auto info = io::stat("missing.txt"); !info) {
        println("{}, not found: {}", info.error().message(), info.error().is_not_found());
    }
}
```

Output:

```text
notes.txt 12 true false
stat missing.txt: No such file or directory, not found: true
```

The same from a task.

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<uint64_t> size_of(string path) {
    auto info = co_await io::async_stat(path);
    co_return info ? info->size : 0;
}

int main() {
    (void)io::write_file("data.bin", "0123456789");
    println("{}", async::run(size_of("data.bin")));
}
```

Output:

```text
10
```

## See also

- [lstat](lstat.md): the link itself, not its target
- [exists](exists.md), [is_directory](is_directory.md), [is_regular](is_regular.md): the questions without the error
- [file::stat](file/stat.md): the stat of an open file
- [file_info](file_info/README.md): what the result holds
