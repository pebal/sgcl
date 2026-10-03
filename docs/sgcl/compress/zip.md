[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::zip

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    using error = compress::error;
    enum class method : uint16_t;
    struct entry;
    class archive;
    class writer;
    struct options;

    expected<void, error> extract(const string& archive_path, const string& directory,
                                  const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory,
                                                     options o = {}) noexcept;
    expected<void, error> create(const string& directory, const string& archive_path,
                                 const options& o = {});
    async::task<expected<void, error>> async_create(string directory, string archive_path,
                                                    options o = {}) noexcept;
}
```

`sgcl::compress::zip` is zip as PKWARE's APPNOTE 6.3.10 has it and as every tool writes it: entries stored or
deflated, ZIP64 for 65 535 entries or more and past 4 GiB, names in UTF-8; entries of Deflate64 (method 9, as 7-Zip
and Windows write large archives) are read. `.zip`, `.jar`, `.docx`, `.apk`. It is Go's `archive/zip`: an
[archive](zip-archive.md) to read, its entries in any order, a [writer](zip-writer.md) that writes entry after
entry, the [entry](zip-entry.md) both of them describe, and what `unzip` and `zip -r` do to a directory
([extract](zip-extract.md), [create](zip-create.md)).

Unlike tar, a zip archive is read from its end: the central directory there lists every entry with its sizes, its
CRC-32 and where its data lies, so an archive is opened by reading that directory alone, and an entry's data is read
when it is asked for. The writer, the other way, writes the local headers without sizes and a data descriptor after
each entry's data, so it never seeks back: a socket or an HTTP response is written straight through.

## Rules

- **The names are the archive's.** An archive from outside names its entries as it likes (`../../etc/passwd`, the
  *zip slip*): a program that writes entries to disk checks each name with [is_local](zip-entry/is_local.md), and
  [extract](zip-extract.md) checks every name before anything is written.
- **Checked against the directory.** An entry's sizes and CRC-32 are the central record's: its reader gives exactly
  that size, and fewer or more is `errc::corrupt`, a wrong CRC-32 `errc::checksum`. A whole read refuses an entry
  larger than the [limits](limits.md) before reading a byte, so that neither a zip bomb nor entries overlapping the
  same data (the "overlap" bomb) take more than the limit.
- **Tolerated as Go tolerates it:** bytes in front of the archive (a self-extractor), a directory offset or size the
  end record gets wrong, junk after the archive, data descriptors with or without their signature.
- **Refused:** a comment that runs past the end of the file (a truncated archive), a count of entries the directory
  does not hold, encryption and methods other than store, deflate and Deflate64 (`errc::unsupported`, when the entry
  is read).
- The writer keeps its first error, as every stream of the module does: an archive is written freely and checked
  once, at the close.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](error.md) |
| [method](zip-method.md) | how an entry's data is kept: store, deflate, Deflate64 (read) |
| [entry](zip-entry.md) | one entry: its name, time, sizes, CRC-32, method, mode |
| [archive](zip-archive.md) | an archive to read: its entries in any order |
| [writer](zip-writer.md) | an archive written entry after entry |
| [options](zip-options.md) | what `extract` and `create` take besides the paths |

## Member functions

| Function | Description |
|---|---|
| [extract, async_extract](zip-extract.md) | an archive unpacked into a directory, every name checked first |
| [create, async_create](zip-create.md) | a directory packed into an archive |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("photos/2026");
    (void)io::write_file("photos/2026/list.txt", "beach.jpg\nhills.jpg\n");
    (void)compress::zip::create("photos", "photos.zip");
    (void)compress::zip::extract("photos.zip", "restored");
    print("{}", io::read_text("restored/2026/list.txt").value_or(string("?")));
    for (auto dir : {"photos", "restored"}) {
        (void)io::remove_all(dir);
    }
    (void)io::remove("photos.zip");
}
```

Output:

```text
beach.jpg
hills.jpg
```

## See also

- [tar](tar.md), [sevenzip](sevenzip.md): the other archives
- [flate](flate.md): the data of a deflated entry
- [benchmarks](benchmarks.md): an archive of 10 000 entries opened, against Go
- `tests/compress/zip.cpp`: every archive of Go's test data read as Go reads it; `tests/compress/files.cpp`:
  `extract` and `create` both ways against Info-ZIP's `zip` and `unzip`
- [sgcl::compress](README.md)
