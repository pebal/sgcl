[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::entry_info

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    struct entry_info {
        optional<time::datetime> modified;
        optional<time::datetime> created;
        optional<time::datetime> accessed;
        optional<io::permissions> mode;
        bool symlink = false;
        optional<uint32_t> attributes;
    };
}
```

`sgcl::compress::sevenzip::entry_info` is what an entry the [writer](sevenzip-writer.md) writes is besides its name
and data: its times, its mode, whether it is a link. [create](sevenzip-writer/create.md),
[add](sevenzip-writer/add.md) and [add_directory](sevenzip-writer/add_directory.md) take it. Entries are written as
7-Zip writes them on Unix: the modification time (now, unless given), the attributes with the POSIX mode in the high
16 bits (0x8000 set, the type — file, directory, link — with the mode), a link's target as its data.

## Member objects

| Member | Description |
|---|---|
| `modified` | the time of the last change; none, by default, is now |
| `created`, `accessed` | written when given |
| `mode` | the permission bits; none, by default, is 0644, a directory's 0755, a link's 0777 |
| `symlink` | the entry is a symbolic link: its data is the target; `false` by default |
| `attributes` | the archive's attributes as they are (Windows' and POSIX's), written in place of those of `mode` |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    compress::sevenzip::entry_info info{.mode = io::permissions(0600)};
    info.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    w.add("private.txt", "secret\n", info);
    (void)w.close();

    auto e = compress::sevenzip::archive::from(archive.data())->entries()[0];
    println("{} {} {:o}", e.name, *e.modified, (e.attributes >> 16) & 0777);
}
```

Output:

```text
private.txt 2026-09-21T14:13:20Z 600
```

## See also

- [writer](sevenzip-writer.md), [entry](sevenzip-entry.md)
- [sgcl::compress::sevenzip](sevenzip.md)
