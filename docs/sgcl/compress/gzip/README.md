[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::gzip

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class gzip;
}
```

`sgcl::compress::gzip` is gzip (RFC 1952): DEFLATE between a [header](../gzip_header.md) — a name, a comment, the time
the data was modified, an extra field, the system that made it — and the CRC-32 and the length of the data, which
the reader checks at the end. `.gz` files, `.tar.gz`, and HTTP's `Content-Encoding: gzip`. It is Go's
`compress/gzip`, as a class of static functions and the types of the format: the whole of the data in memory either
way ([compress](compress.md), [decompress](decompress.md)), a stream each way (a [writer](../gzip-writer/README.md)
that writes the header before the first bytes, a [reader](../gzip-reader/README.md) of every member one after another), and
what gzip(1) and gunzip do to a file ([compress_file](compress_file.md),
[decompress_file](decompress_file.md)).

## Rules

- **Several members are one stream.** `cat a.gz b.gz > c.gz` is valid gzip, and gunzip gives both parts one after
  another: so do [decompress](decompress.md) and the [reader](../gzip-reader/README.md). What follows a member is taken as
  the next member, as Go does: bytes after the last member that are not another one are `errc::invalid_header`
  (fewer than a header's ten bytes, `errc::unexpected_end`). A reader made with `gzip::single_member` stops after
  the first member.
- **The header's strings are ISO 8859-1** in the format (2.3.1): the writer converts the program's UTF-8 where ISO
  8859-1 holds it, and writes a name or a comment past U+00FF as its UTF-8 bytes, as gzip(1) writes a file's name;
  the reader takes bytes that are UTF-8 as they are and converts the others, so a string reads back as it was
  written ([gzip_header](../gzip_header.md)). A NUL in either, or an extra field past 65 535 bytes, is refused with
  `errc::invalid_argument` at the writer's first write (as Go does), and by `compress` with `std::invalid_argument`
  (it returns no error).
- The output is valid gzip that any decoder reads, at the sizes of [flate](../flate/README.md)'s levels plus a header and a
  trailer of eight bytes.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| `header` | [gzip_header](../gzip_header.md) |
| [options](../gzip-options.md) | the level and the header |
| [file_options](../gzip-file_options.md) | the level of `compress_file`, and whether the original stays |
| `single_member_t` | the type of the tag `single_member`: an empty struct with an explicit default constructor |
| [writer](../gzip-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../gzip-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member objects

| Constant | Description |
|---|---|
| `single_member` | the tag of the [reader](../gzip-reader/gzip-reader.md)'s constructor that stops after the first member |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one member (static) |
| [decompress](decompress.md) | every member decompressed, checked against the limits (static) |
| [compress_file, async_compress_file](compress_file.md) | a file compressed beside itself, as gzip(1) (static) |
| [decompress_file, async_decompress_file](decompress_file.md) | a `.gz` file decompressed beside itself, as gunzip (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("log.txt", "one line\nanother line\n");
    (void)compress::gzip::compress_file("log.txt", {.level = 9});  // log.txt.gz beside it
    (void)io::remove("log.txt");

    (void)compress::gzip::decompress_file("log.txt.gz");
    print("{}", io::read_text("log.txt").value_or(string("?")));
    (void)io::remove("log.txt");
    (void)io::remove("log.txt.gz");
}
```

Output:

```text
one line
another line
```

## See also

- [flate](../flate/README.md): DEFLATE alone; [zlib](../zlib/README.md): DEFLATE with an Adler-32
- [tar](../tar.md): `.tar.gz`
- [benchmarks](../benchmarks.md): a gzip stream read against zlib and Go
- `tests/compress/files.cpp`: the files tested both ways against gzip(1)
- [sgcl::compress](../README.md)
