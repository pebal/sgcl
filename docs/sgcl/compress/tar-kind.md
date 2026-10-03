[sgcl](../README.md) › [compress](README.md) › [tar](tar.md)

# sgcl::compress::tar::kind

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    enum class kind : uint8_t {
        file,
        directory,
        symlink,
        hardlink,
        char_device,
        block_device,
        fifo
    };
}
```

What an entry of a tar archive is: the type flag of its header, by name. A type flag the list has no name for is a
file, as POSIX says of ustar (`'7'`, contiguous, among them); GNU's `'D'` (a directory with the listing of an
incremental dump as its data) is a directory, the listing stepped over. Only a `file` has data: the
[entry](tar-entry.md)'s size is 0 for every other kind.

| Value | Description |
|---|---|
| `file` | a regular file, its data after the header |
| `directory` | a directory; its name ends in `/` as archivers write it |
| `symlink` | a symbolic link to `link_name` |
| `hardlink` | a second name of an earlier entry, `link_name`, from the archive's root |
| `char_device` | a character device, `dev_major` and `dev_minor` |
| `block_device` | a block device, `dev_major` and `dev_minor` |
| `fifo` | a named pipe |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    compress::tar::entry link{.name = "latest", .link_name = "v2/",
                              .type = compress::tar::kind::symlink};
    (void)w.write_header(link);
    (void)w.close();

    compress::tar::reader r(archive);
    auto e = r.next();
    println("{} -> {} {}", (*e)->name, (*e)->link_name, (*e)->type == compress::tar::kind::symlink);
}
```

Output:

```text
latest -> v2/ true
```

## See also

- [entry](tar-entry.md)
- [sgcl::compress::tar](tar.md)
