[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](README.md)

# sgcl::compress::zip::archive::open, async_open

```cpp
static expected<archive, error> open(const string& path) noexcept;                  // (1)
static async::task<expected<archive, error>> async_open(string path) noexcept;      // (2)
static expected<archive, error> open(const io::file& file) noexcept;                // (3)
static async::task<expected<archive, error>> async_open(io::file file) noexcept;    // (4)
```

Opens an archive: finds the end record at the end of the file, and reads the central directory, where the archive
lists its entries, in pieces of 256 KB, and nothing else. The entries' data is read when it is asked for, at offsets
of the file.

- (1–2) The file at `path`, opened by the archive and closed by its [close](close.md); a failure closes it.
- (3–4) A file the program opened, which stays the program's to close.
- (2), (4) Return a task whose reads of the file run on the [blocking pool](../../async/spawn_blocking.md), into one
  managed buffer for the open.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the archive's path |
| `file` | an open file of the archive |

## Return value

The archive, or the [error](../error/README.md): no end of central directory (`errc::invalid_header`, "not a zip file"), a
directory that cannot be read or a count of entries it does not hold (`errc::corrupt`, `errc::invalid_header`), a
comment that runs past the end (`errc::unexpected_end`), a failure of the file (`errc::io`).

## Complexity

Linear in the size of the central directory.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    {
        io::writer file = *io::create("notes.zip");
        compress::zip::writer w(file);
        (void)w.add("notes.txt", "remember the milk\n");
        (void)w.close();
        (void)file.close();
    }
    auto a = compress::zip::archive::open("notes.zip");
    println("{} entry", a->entries().size());
    (void)a->close();

    println("{}", compress::zip::archive::open("missing.zip").error().message());
    (void)io::remove("notes.zip");
}
```

Output:

```text
1 entry
input/output error: open missing.zip: No such file or directory
```

## See also

- [from](from.md): an archive in memory
- [close](close.md)
- [sgcl::compress::zip::archive](README.md)
