[sgcl](../README.md) › [io](README.md)

# sgcl::io::create, async_create

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<file, error> create(const string& path,                                      // (1)
                                 permissions p = permissions(0666)) noexcept;
    async::task<expected<file, error>> async_create(const string& path,                   // (2)
                                                    permissions p = permissions(0666))
        noexcept;
}
```

Opens the file at `path` for writing, created when it is not there and emptied when it is:
`open(path, open_flags::write | open_flags::create | open_flags::truncate, p)` ([open](open.md)). A file it creates
gets the permissions `p`, masked by the umask. Go's `os.Create`.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md): an open waits for the disk.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |
| `p` | the permissions of a file it creates, before the umask |

## Return value

The [file](file/README.md), opened for writing only, or the [error](error/README.md) of [open](open.md), its operation `open`
(`is_not_found()` for a directory on the way that is not there, `is_permission()`).

## Complexity

As [open](open.md); the bytes an existing file had are released by the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("out.txt");
    f.write("a long first version");
    f.close();

    f = io::create("out.txt");  // emptied
    f.write("second");
    f.close();
    println("{}", *io::read_text("out.txt"));

    println("{}", io::create("no/such/dir/out.txt").error().message());
}
```

Output:

```text
second
open no/such/dir/out.txt: No such file or directory
```

## See also

- [open](open.md): any flags
- [write_file](write_file.md): a file created and written in one call
- [temp_file](temp_file.md): a new file under a random name
- [sgcl::io::file](file/README.md)
