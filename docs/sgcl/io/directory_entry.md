[sgcl](../README.md) › [io](README.md)

# sgcl::io::directory_entry

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct directory_entry;
}
```

`io::directory_entry` is an entry of a directory listing, Go's `fs.DirEntry`: what [read_dir](read_dir.md) returns
and [walk_dir](walk_dir.md) gives its function. The name and the type come from the listing itself, with no stat
per entry; the rest of what a stat says is [info()](directory_entry/info.md), asked when needed.

## Rules

- The struct holds [string](../core/string.md)s, so it lives where one may: on a stack or inside a managed object
  ([The rules](../core/README.md#the-rules), 1).

## Member objects

| Member | Description |
|---|---|
| `string name` | the name of the entry in its directory |
| `string path` | the directory's path joined with the name |
| `file_type type` | the [file_type](file_type.md) from the listing: a symbolic link is `file_type::symlink`, not followed; `file_type::unknown` by default and when the listing did not say |

## Member functions

| Function | Description |
|---|---|
| [is_directory](directory_entry/is_directory.md) | checks whether the entry is a directory |
| [info](directory_entry/info.md) | what a stat of the entry says |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("docs/img");
    (void)io::write_file("docs/index.md", "# docs");
    auto entries = io::read_dir("docs");
    if (!entries) {
        return 1;
    }
    for (const io::directory_entry& e : *entries) {
        println("{} {} {}", e.name, e.path, e.is_directory());
    }
}
```

Output:

```text
img docs/img true
index.md docs/index.md false
```

## See also

- [read_dir](read_dir.md), [walk_dir](walk_dir.md): what gives the entries
- [file_info](file_info.md): what `info()` returns
