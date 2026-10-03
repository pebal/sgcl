[sgcl](../README.md) › [compress](README.md) › [zip](zip.md)

# sgcl::compress::zip::entry

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    struct entry {
        string name;
        string comment;
        time::datetime modified;
        uint64_t size = 0;
        uint64_t compressed_size = 0;
        uint32_t crc32 = 0;
        zip::method method = zip::method::deflate;
        io::permissions mode = io::permissions(0644);
        bool symlink = false;
        vector<byte> extra;
        uint64_t offset = 0;

        bool is_directory() const noexcept;
        bool is_symlink() const noexcept;
        bool is_local() const noexcept;
    };
}
```

`sgcl::compress::zip::entry` is one entry of a zip archive, Go's `zip.FileHeader`: what its central directory record
says. The [archive](zip-archive.md) lists them ([entries](zip-archive/entries.md)); the [writer](zip-writer.md)'s
[create](zip-writer/create.md) takes one for the method, the time, the mode and the comment of a new entry.

## Rules

- The time is the one of the extra fields that carry one (the extended timestamp, Info-ZIP's Unix field, NTFS's),
  else the DOS date and time of the record, which name no zone and are read as UTC, as Go reads them; it is written
  in its own zone.
- A name is UTF-8 when the record says so or when it is valid UTF-8 anyway (most tools write it without the flag);
  anything else is taken as code page 437, which is what the flag's absence means. A directory's name ends in `/`.
- The sizes, the CRC-32 and the offset are read from the archive; the writer fills them itself and does not read
  them.
- An aggregate, a value: `{.name = "a.txt", .method = zip::method::store}`, the other fields at their defaults.

## Member objects

| Member | Description |
|---|---|
| `name` | `"dir/file.txt"`, `/` between the parts; a directory's ends in `/` |
| `comment` | the entry's comment |
| `modified` | the time of the last change |
| `size` | the bytes of the data, decompressed |
| `compressed_size` | the bytes of the data in the archive |
| `crc32` | the CRC-32 of the data, decompressed |
| `method` | how the data is kept ([method](zip-method.md)); `method::deflate` by default |
| `mode` | the permission bits; 0644 by default |
| `symlink` | the entry is a symbolic link (Unix attributes): its data is the target |
| `extra` | the central record's extra field, as it is |
| `offset` | where its local header starts in the archive (read) |

## Member functions

| Function | Description |
|---|---|
| [is_directory](zip-entry/is_directory.md) | checks whether the name ends in `/` |
| [is_symlink](zip-entry/is_symlink.md) | checks whether the entry is a symbolic link |
| [is_local](zip-entry/is_local.md) | checks whether the name stays inside the directory it is joined to |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    compress::zip::entry e{.name = "notes.txt", .comment = "kept", .mode = io::permissions(0600)};
    e.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    (void)(*w.create(e)).write("remember the milk\n");
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    const compress::zip::entry& read = a->entries()[0];
    println("{} ({}) {} {} bytes into {}", read.name, read.comment, read.modified, read.size,
            read.compressed_size);
}
```

Output:

```text
notes.txt (kept) 2026-09-21T14:13:20Z 18 bytes into 20
```

## See also

- [archive](zip-archive.md), [writer](zip-writer.md)
- [sgcl::compress::zip](zip.md)
