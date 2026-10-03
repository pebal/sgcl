[sgcl](../README.md) › [io](README.md)

# sgcl::io::copy_file, async_copy_file

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<void, error> copy_file(const string& from, const string& to) noexcept;    // (1)
    async::task<expected<void, error>> async_copy_file(const string& from,             // (2)
                                                     const string& to) noexcept;
}
```

Copies the bytes and the permissions of the regular file at `from` to `to`, replacing what is at `to`, through
`std::filesystem::copy_file` with `overwrite_existing`. Go has no counterpart in `os`.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the file to copy |
| `to` | the path of the copy |

## Return value

Nothing, or the [error](error.md) of the copy; the operation is `copy_file` and the path `from`.

## Complexity

Linear in the size of the file.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("settings.json", "{}");
    (void)io::write_file("backup.json", "old");
    println("{}", io::copy_file("settings.json", "backup.json").has_value());
    println("{}", io::read_text("backup.json").value_or(""));
    println("{}", io::copy_file("none.json", "x.json").error().message());
}
```

Output:

```text
true
{}
copy_file none.json: No such file or directory
```

## See also

- [rename](rename.md): a move
- [copy](copy.md): a stream copied into another
