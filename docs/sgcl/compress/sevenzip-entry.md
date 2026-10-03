[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::entry

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    struct entry {
        string name;
        uint64_t size = 0;
        bool is_directory = false;
        bool is_anti = false;
        optional<uint32_t> crc;
        optional<time::datetime> modified;
        optional<time::datetime> created;
        optional<time::datetime> accessed;
        uint32_t attributes = 0;
        bool encrypted = false;
        uint32_t folder = UINT32_MAX;
        uint64_t offset = 0;

        bool is_symlink() const noexcept;
        bool is_local() const noexcept;
    };
}
```

`sgcl::compress::sevenzip::entry` is one entry of a 7z archive: what the archive's header says of it. The
[archive](sevenzip-archive.md) lists them ([entries](sevenzip-archive/entries.md)) and gives one with a reader of
its data in a [walk](sevenzip-archive/walk.md).

## Rules

- Names are UTF-16LE in the archive and UTF-8 here, `/` between the parts; a surrogate without its pair is
  `errc::corrupt` when the archive is opened.
- The times are the header's FILETIMEs, in UTC.
- An entry without data is a directory unless the header marks it an empty file; an anti-item (what `7zz u` with
  `-u…q3` writes into an archive of differences) is an entry with `is_anti`, directory or file, and no data.
- A value, copied.

## Member objects

| Member | Description |
|---|---|
| `name` | the path in the archive, `/` between the parts |
| `size` | the bytes of the data, decompressed |
| `is_directory` | the entry is a directory |
| `is_anti` | an anti-item: an update's mark that the name was deleted |
| `crc` | the CRC-32 of the data; none for directories and empty files |
| `modified`, `created`, `accessed` | the times the header gives, in UTC |
| `attributes` | Windows' attributes in the low 16 bits; with 0x8000, POSIX `st_mode` in the high 16 |
| `encrypted` | its folder is encrypted (7zAES): its data needs the password |
| `folder` | where its data lies: its folder; `UINT32_MAX` for an entry without data |
| `offset` | the byte of the folder's output where its data starts |

## Member functions

| Function | Description |
|---|---|
| [is_symlink](sevenzip-entry/is_symlink.md) | checks whether the POSIX attributes name a symbolic link |
| [is_local](sevenzip-entry/is_local.md) | checks whether the name stays inside the directory it is joined to |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    w.add("notes.txt", "remember the milk\n");
    w.add_directory("logs");
    w.add("logs/app.log", "started\n");
    (void)w.close();

    for (auto& e : compress::sevenzip::archive::from(archive.data())->entries()) {
        println("{}: {} bytes, directory {}, folder {}, offset {}", e.name, e.size, e.is_directory,
                e.is_directory ? -1 : int(e.folder), e.offset);
    }
}
```

Output:

```text
notes.txt: 18 bytes, directory false, folder 0, offset 0
logs: 0 bytes, directory true, folder -1, offset 0
logs/app.log: 8 bytes, directory false, folder 0, offset 18
```

## See also

- [archive](sevenzip-archive.md), [entry_info](sevenzip-entry_info.md)
- [sgcl::compress::sevenzip](sevenzip.md)
