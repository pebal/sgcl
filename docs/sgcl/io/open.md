[sgcl](../README.md) › [io](README.md)

# sgcl::io::open, async_open

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<file, error> open(const string& path, open_flags flags = open_flags::read,
                                       permissions p = permissions(0666)) noexcept;
    /*(2)*/ async::task<expected<file, error>> async_open(const string& path,
                                                          open_flags flags = open_flags::read,
                                                          permissions p = permissions(0666))
                noexcept;
}
```

Opens the file at `path` as `flags` say ([open_flags](open_flags.md)): the `open(2)` of the system, made again when a
signal interrupts it, the descriptor `O_CLOEXEC`. A file it creates gets the permissions `p`, masked by the umask as
`open(2)` does. A regular file, a directory and a terminal are left blocking, and their async operations run on the
[blocking pool](../async/spawn_blocking.md); a FIFO or a device is made non-blocking and served by the
[reactor](../async/readable.md) ([is_nonblocking](file/is_nonblocking.md)).

1. On the calling thread.
2. The same for a task, on the blocking pool: an open waits for the disk, and one of a FIFO for the other end.

What Go's `os.Open` (the defaults) and `os.OpenFile` (the flags and the permissions) do.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file, relative to the working directory or absolute |
| `flags` | how to open it: `read`, `write`, `create`, `truncate`, `append`, `exclusive`, `sync`, combined with `\|` |
| `p` | the permissions of a file `create` makes, before the umask; not used otherwise |

## Return value

The [file](file.md), or the [error](error.md), its operation `open` and its path `path`, with the `errno` of
`open(2)`: it answers `is_not_found()` for a missing file or directory on the way, `is_permission()` for a file the
process may not open so, `is_exists()` for one that exists under `create | exclusive`.

## Complexity

One `open(2)`, then an `fstat(2)` to see what was opened, and for a FIFO or a device the calls that make it
non-blocking.

## Exceptions

None.

## Notes

`io::file f = io::open(p);` takes the file out of the `expected`, and throws its error when there is none
([expected](../core/expected.md)); `auto opened = io::open(p); if (!opened) ...` is the form that looks at the
failure.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto log_flags = io::open_flags::write | io::open_flags::create | io::open_flags::append;
    io::file log = io::open("app.log", log_flags, io::permissions(0644));
    log.write("started\n");

    auto lock_flags = io::open_flags::write | io::open_flags::create | io::open_flags::exclusive;
    auto lock = io::open("app.lock", lock_flags);
    println("{}", static_cast<bool>(lock));
    auto second = io::open("app.lock", lock_flags);
    if (!second && second.error().is_exists()) {
        println("another instance runs");
    }

    auto missing = io::open("missing.txt");
    println("{} {}", missing.error().is_not_found(), missing.error().message());
}
```

Output:

```text
true
another instance runs
true open missing.txt: No such file or directory
```

A task opens a file without holding a worker:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<expected<string, io::error>> load(string path) {
    auto opened = co_await io::async_open(path);
    if (!opened) co_return unexpected(opened.error());
    co_return co_await opened->async_read_all_text();
}

int main() {
    io::write_file("poem.txt", "a short poem");
    println("{}", *async::run(load("poem.txt")));
    println("{}", async::run(load("none.txt")).error().message());
}
```

Output:

```text
a short poem
open none.txt: No such file or directory
```

## See also

- [open_flags](open_flags.md): the flags
- [create](create.md): `open(path, write | create | truncate)`
- [from_fd](from_fd.md): a file over a descriptor opened elsewhere
- [read_file](read_file.md), [read_text](read_text.md): a whole file in one call
- [sgcl::io::file](file.md)
