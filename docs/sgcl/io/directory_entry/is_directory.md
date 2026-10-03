[sgcl](../../README.md) › [io](../README.md) › [directory_entry](README.md)

# sgcl::io::directory_entry::is_directory

```cpp
bool is_directory() const noexcept;
```

Checks whether the entry is a directory: whether `type` is `file_type::directory`, as the listing said. A symbolic
link to a directory is not one: its type is `file_type::symlink`.

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
    (void)io::mkdir_all("site/assets");
    (void)io::symlink("assets", "site/static");
    auto entries = io::read_dir("site");
    if (!entries) {
        return 1;
    }
    for (const io::directory_entry& e : *entries) {
        println("{} {}", e.name, e.is_directory());
    }
}
```

Output:

```text
assets true
static false
```

## See also

- [info](info.md): what a stat of the entry says
- [sgcl::io::directory_entry](README.md)
