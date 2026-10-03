[sgcl](../README.md) › [io](README.md)

# sgcl::io::set_modified

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    using file_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    expected<void, error> set_modified(const string& path, file_time t) noexcept;
}
```

Sets the time of the last modification of the file at `path` to `t`, to the nanosecond the file system keeps,
leaving its access time as it is: the second half of Go's `os.Chtimes`, the system's `utimensat`. `file_time` is the
time of the system clock in nanoseconds, the type of [file_info](file_info/README.md)'s `modified`. A time before 1970 is set
as it is, its fraction of a second included.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `t` | the new time of its last modification |

## Return value

Nothing, or the [error](error/README.md) of the call; the operation is `set_modified` and the path `path`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("stamp", "");
    io::file_time t(std::chrono::seconds(1700000000));
    (void)io::set_modified("stamp", t);
    println("{}", io::stat("stamp")->modified == t);
    println("{}", io::set_modified("none", t).error().message());
}
```

Output:

```text
true
set_modified none: No such file or directory
```

## See also

- [stat](stat.md): the time read
- [file_info](file_info/README.md): `modified`
