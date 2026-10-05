[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md)

# sgcl::compress::zip::archive

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    class archive;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zip::archive` is a zip archive to read, Go's `zip.Reader`: its central directory read when it is
opened, the entries then open in any order and any number of times at once. [open](open.md) reads the
central directory, where the archive lists its entries, in pieces of 256 KB, and nothing else;
[from](from.md) does the same for an archive in memory. An entry's data is read when it is asked for,
through [reader](reader.md) as a stream or whole with [read](read.md). Readers of any number
of entries may be open at once, from any threads (the file is read at offsets).

## Rules

- A value, copied cheaply: the entries and the source are shared by the copies.
- **Checked against the directory.** An entry's sizes and CRC-32 are the central record's (a failure read through
  the io handle names the entry as its path): its reader gives exactly `e.size` bytes, and fewer or more is
  `errc::corrupt`, a wrong CRC-32 `errc::checksum`, at the read that reaches the end. A whole
  [read](read.md) refuses an entry larger than the [limits](../limits.md) before reading a byte, so that
  neither a zip bomb nor entries overlapping the same data (the "overlap" bomb) take more than the limit.
- The local header is read only to find where the data starts (its own name and extra field lengths, which may
  differ from the central record's); its name must be the central one.
- **Tolerated as Go tolerates it:** bytes in front of the archive (a self-extractor), a directory offset or size the
  end record gets wrong, junk after the archive, data descriptors with or without their signature.
- **Deflate64** entries are read as deflated ones are, the reader's window twice 64 KB.
- **Refused:** a comment that runs past the end of the file (a truncated archive), a count of entries the directory
  does not hold, encryption and methods other than store, deflate and Deflate64 (`errc::unsupported`, when the entry
  is read).
- [close](close.md) closes the file the archive opened itself from a path; a file given to `open` is the
  caller's.
- A task's reads of a file (`async_open`, `async_read`, an entry reader's `async_read`) run on the
  [blocking pool](../../async/spawn_blocking.md), so they go into managed memory a slice given to the file holds: one
  buffer for the open, a block of 512 bytes an entry reader keeps for the local header and the name, the inflater's
  input, and for a stored entry the caller's own slice, its owner with it. A thread's reads keep plain memory, and so
  does a task's of an archive in memory, which waits for nothing and is read as a thread reads it (letting the worker
  go every 64 KB).

## Member functions

| Function | Description |
|---|---|
| [open, async_open](open.md) | an archive of a file, its central directory read (static) |
| [from](from.md) | an archive in memory (static) |
| [close](close.md) | closes the file the archive opened itself |

#### Lookup

| Function | Description |
|---|---|
| [entries](entries.md) | every entry, in the order of the central directory |
| [find](find.md) | the first entry of a name |
| [comment](comment.md) | the archive's comment |

#### Operations

| Function | Description |
|---|---|
| [reader](reader.md) | an io reader of an entry's data, decompressed and checked |
| [read, async_read](read.md) | an entry's data whole, checked against the limits |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("site/css");
    (void)io::write_file("site/index.html", "<h1>hello</h1>\n");
    (void)io::write_file("site/css/site.css", "h1 { color: teal }\n");
    (void)compress::zip::create("site", "site.zip");

    auto a = compress::zip::archive::open("site.zip");
    if (!a) {
        println("{}", a.error().message());
        return 1;
    }
    for (auto& e : a->entries()) {
        if (e.is_directory() || !e.is_local()) {
            continue;
        }
        auto data = a->read(e);  // under 1 GiB
        println("{}: {} bytes", e.name, data->size());
    }
    (void)a->close();
    (void)io::remove_all("site");
    (void)io::remove("site.zip");
}
```

Output:

```text
css/site.css: 19 bytes
index.html: 15 bytes
```

## See also

- [writer](../zip-writer/README.md): the other way
- [extract](../zip-extract.md): the whole archive into a directory
- [sgcl::compress::zip](../zip.md)
