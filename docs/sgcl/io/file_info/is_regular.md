[sgcl](../../README.md) › [io](../README.md) › [file_info](../file_info.md)

# sgcl::io::file_info::is_regular

```cpp
bool is_regular() const noexcept;
```

Checks whether the file is a regular file: whether `type` is `file_type::regular`.

## Parameters

None.

## Return value

`true` when `type` is `file_type::regular`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir("dir");
    (void)io::write_file("file", "x");
    (void)io::symlink("file", "link");
    for (const char* p : {"dir", "file", "link"}) {
        if (auto info = io::lstat(p)) {
            println("{}: {}", p, info->is_regular());
        }
    }
}
```

Output:

```text
dir: false
file: true
link: false
```

## See also

- [is_directory](is_directory.md), [is_symlink](is_symlink.md): the other two types asked for
- [file_type](../file_type.md): every type
- [sgcl::io::file_info](../file_info.md)
