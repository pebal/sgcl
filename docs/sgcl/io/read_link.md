[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_link

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> read_link(const string& link) noexcept;
}
```

Returns the target of the symbolic link `link`, as it was written when the link was made, Go's `os.Readlink`,
through `std::filesystem::read_symlink`. The target is not resolved: a relative one stays relative to the link's
directory.

## Parameters

| Parameter | Description |
|---|---|
| `link` | the path of the link |

## Return value

The target, or the [error](error/README.md) (`std::errc::invalid_argument` when `link` is not a symbolic link,
`is_not_found()` when nothing is there); the operation is `read_link` and the path `link`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("v2.txt", "x");
    (void)io::symlink("v2.txt", "current");
    println("{}", io::read_link("current").value_or("?"));
    println("{}", io::read_link("v2.txt").error().message());
}
```

Output:

```text
v2.txt
read_link v2.txt: Invalid argument
```

## See also

- [symlink](symlink.md): makes a link
- [lstat](lstat.md): the link itself
