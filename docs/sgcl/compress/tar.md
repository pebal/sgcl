[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::tar

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    using error = compress::error;
    enum class kind : uint8_t;
    struct entry;
    class reader;
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

`sgcl::compress::tar` is tar as POSIX and GNU have it: ustar, pax (a record per field, for names, sizes and times
ustar cannot hold) and GNU's long names and links, read from any of them — v7, ustar, pax, GNU, star — and written as
ustar, or as pax where ustar cannot say what an entry holds. `.tar`, and with [gzip](gzip/README.md) `.tar.gz`, with
[xz](xz/README.md) `.tar.xz`, with [bzip2](bzip2/README.md) `.tar.bz2` (read). It is Go's `archive/tar`: a [reader](tar-reader/README.md)
that goes from entry to entry and reads each one's data, a [writer](tar-writer/README.md) that writes a header and then the
entry's data, the [entry](tar-entry/README.md) both of them take, and what `tar -x` and `tar -c` do to a directory
([extract](tar-extract.md), [create](tar-create.md)).

A tar archive is a stream, read and written from its start to its end: the reader and the writer take any
[io stream](../io/README.md), so an archive inside gzip is a `tar::reader` over a `gzip::reader`, and an archive
written into a network connection never touches the disk.

## Rules

- **The names are the archive's.** Nothing is checked or cleaned when an archive is read: an archive from outside
  names its entries as it likes, and a program writing them to disk checks each with
  [is_local](tar-entry/is_local.md). [extract](tar-extract.md) checks every name before anything is written.
- **What is read and not given:** a sparse file (GNU's and pax's forms) or a multi-volume part is
  `errc::unsupported` naming the entry, and the reader goes on to the entry after it.
- **Limits:** a pax header or a GNU long name larger than 1 MiB is `errc::too_large`, and the global records
  altogether no more; a negative or impossible size is `errc::invalid_header`. Nothing an archive says makes the
  reader hold more than that.
- **Unlike Go:** the global pax records apply to the entries after them, as POSIX says (Go hands the `g` header out
  as an entry and applies nothing); a directory, a link or a device has size 0, whatever its header carries.
- **Written as Go writes it:** the golden archives of Go's tests are matched byte for byte; the one known
  difference is the name of a pax header written for a directory (Go keeps the directory's trailing part in it).
- The reader and the writer keep their first error, as every stream of the module does: an archive is written
  freely and checked once, at the close.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](error/README.md) |
| [kind](tar-kind.md) | what an entry is: a file, a directory, a link, a device, a fifo |
| [entry](tar-entry/README.md) | one entry: its name, type, size, mode, times, owners, pax records |
| [reader](tar-reader/README.md) | an archive read entry by entry, each entry's data an io stream |
| [writer](tar-writer/README.md) | an archive written entry by entry |
| [options](tar-options.md) | what `extract` and `create` take besides the paths |

## Member functions

| Function | Description |
|---|---|
| [extract, async_extract](tar-extract.md) | an archive unpacked into a directory, every name checked first |
| [create, async_create](tar-create.md) | a directory packed into an archive, gzip or xz around it by its name |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("site/css");
    (void)io::write_file("site/index.html", "<h1>hello</h1>\n");
    (void)io::write_file("site/css/site.css", "h1 { color: teal }\n");
    (void)compress::tar::create("site", "site.tar.gz");   // gzip, by the name
    (void)compress::tar::extract("site.tar.gz", "copy");  // gzip, xz or bzip2, by the first bytes
    print("{}", io::read_text("copy/index.html").value_or(string("?")));
    for (auto dir : {"site", "copy"}) {
        (void)io::remove_all(dir);
    }
    (void)io::remove("site.tar.gz");
}
```

Output:

```text
<h1>hello</h1>
```

## See also

- [zip](zip.md), [sevenzip](sevenzip.md): the other archives
- [io::path::is_local](../io/path/README.md): the rule of a name that stays inside
- `tests/compress/tar.cpp`: Go's test archives read as Go reads them, its golden archives written byte for byte;
  `tests/compress/files.cpp`: `extract` and `create` both ways against bsdtar
- [sgcl::compress](README.md)
