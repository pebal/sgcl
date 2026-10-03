[sgcl](../README.md) › [io](README.md)

# sgcl::io::file_type

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class file_type { unknown, regular, directory, symlink, block, character, fifo, socket };
}
```

The type of a file, as [stat](stat.md) reports it in [file_info](file_info/README.md) and [read_dir](read_dir.md) in a
[directory_entry](directory_entry/README.md): the type bits of Go's `fs.FileMode`.

| Value | Description |
|---|---|
| `unknown` | a type the system did not say, or one of none of the others |
| `regular` | a regular file |
| `directory` | a directory |
| `symlink` | a symbolic link, from [lstat](lstat.md) or a listing, which do not follow it |
| `block` | a block device |
| `character` | a character device: a terminal, `/dev/null` |
| `fifo` | a named pipe |
| `socket` | a Unix domain socket |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("kinds/dir");
    (void)io::write_file("kinds/file", "x");
    (void)io::symlink("file", "kinds/link");
    auto entries = io::read_dir("kinds");
    if (!entries) {
        return 1;
    }
    for (const io::directory_entry& e : *entries) {
        switch (e.type) {
            case io::file_type::directory: println("{}: a directory", e.name); break;
            case io::file_type::regular: println("{}: a file", e.name); break;
            case io::file_type::symlink: println("{}: a link", e.name); break;
            default: println("{}: something else", e.name); break;
        }
    }
    println("{}", io::stat("/dev/null")->type == io::file_type::character);
}
```

Output:

```text
dir: a directory
file: a file
link: a link
true
```

## See also

- [file_info](file_info/README.md): `type`, `is_regular`, `is_directory`, `is_symlink`
- [directory_entry](directory_entry/README.md): the type of an entry
