[sgcl](../../README.md) › [io](../README.md) › [file_info](../file_info.md)

# sgcl::io::file_info::is_directory

```cpp
bool is_directory() const noexcept;
```

Checks whether the file is a directory: whether `type` is `file_type::directory`.

## Parameters

None.

## Return value

`true` when `type` is `file_type::directory`.

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
            println("{}: {}", p, info->is_directory());
        }
    }
}
```

Output:

```text
dir: true
file: false
link: false
```

## See also

- [is_regular](is_regular.md), [is_symlink](is_symlink.md): the other two types asked for
- [file_type](../file_type.md): every type
- [sgcl::io::file_info](../file_info.md)
