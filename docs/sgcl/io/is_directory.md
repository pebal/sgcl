[sgcl](../README.md) › [io](README.md)

# sgcl::io::is_directory

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    bool is_directory(const string& path) noexcept;
}
```

Checks whether the path names a directory, following a symbolic link. The question is the shortcut: `false` when nothing is there and on
any error alike (a permission denied, a path too long); [stat](stat.md) is the answer with the error. A task asks
`co_await io::async_stat(p)` instead, which waits on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path to ask about |

## Return value

`true` when `stat` succeeds and the file is a directory.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("dir");
    (void)io::write_file("file.txt", "x");
    for (const char* p : {"dir", "file.txt", "nothing"}) {
        println("{}: {} {} {}", p, io::exists(p), io::is_directory(p), io::is_regular(p));
    }
}
```

Output:

```text
dir: true true false
file.txt: true false true
nothing: false false false
```

## See also

- [stat](stat.md): the answer with the error
- [exists](exists.md)
- [is_regular](is_regular.md)
