[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::is_abs

```cpp
bool is_abs(const string& p) noexcept;
```

Checks whether the path is absolute, Go's `filepath.IsAbs`: on POSIX, whether it begins with the separator.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the path |

## Return value

`true` when `p` begins with `/`; `false` for a relative path and for the empty one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", io::path::is_abs("/etc/hosts"), io::path::is_abs("docs/README.md"),
            io::path::is_abs(""));
}
```

Output:

```text
true false false
```

## See also

- [abs](abs.md): the absolute form of a relative path
- [is_local](is_local.md): whether a name stays inside a directory
- [sgcl::io::path](README.md)
