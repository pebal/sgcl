[sgcl](../README.md) › [io](README.md)

# sgcl::io::rename, async_rename

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> rename(const string& from, const string& to) noexcept;    // (1)
    async::task<expected<void, error>> async_rename(const string& from,             // (2)
                                                  const string& to) noexcept;
}
```

Moves the file at `from` to `to`, replacing what is at `to`, Go's `os.Rename`, the system's `rename`. Within one
file system the replacement is atomic: a reader of `to` sees the old file or the new one, never a part, which is how
a file is written safely (written beside, then renamed over). The name is qualified in a program, `io::rename`:
under `using namespace sgcl;` a bare `rename` with literals is the C library's.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the path of the file to move |
| `to` | its new path |

## Return value

Nothing, or the [error](error/README.md) of the call (`is_not_found()` when nothing is at `from`; a move across file systems
is the system's `EXDEV`); the operation is `rename` and the path `from`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("index", "version 1");
    (void)io::write_file("index.tmp", "version 2");
    println("{}", io::rename("index.tmp", "index").has_value());  // an atomic replace
    println("{} {}", io::read_text("index").value_or(""), io::exists("index.tmp"));
    println("{}", io::rename("nothing", "x").error().message());
}
```

Output:

```text
true
version 2 false
rename nothing: No such file or directory
```

## See also

- [copy_file](copy_file.md): a copy, the original kept
- [remove](remove.md): removes a file
