[sgcl](../../README.md) › [io](../README.md) › [file_info](../file_info.md)

# sgcl::io::file_info::is_symlink

```cpp
bool is_symlink() const noexcept;
```

Checks whether the file is a symbolic link: whether `type` is `file_type::symlink`.
Only [lstat](../lstat.md) reports one: [stat](../stat.md) follows the link.

## Parameters

None.

## Return value

`true` when `type` is `file_type::symlink`.

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
            println("{}: {}", p, info->is_symlink());
        }
    }
}
```

Output:

```text
dir: false
file: false
link: true
```

## See also

- [is_regular](is_regular.md), [is_directory](is_directory.md): the other two types asked for
- [file_type](../file_type.md): every type
- [sgcl::io::file_info](../file_info.md)
