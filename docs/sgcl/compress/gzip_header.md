[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::gzip_header

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    struct gzip_header {
        string name;
        string comment;
        optional<time::datetime> modified;
        vector<byte> extra;
        uint8_t os = 255;
    };

    class gzip {
    public:
        using header = gzip_header;
    };
}
```

`sgcl::compress::gzip_header` is the header of a gzip member (RFC 1952, 2.3): every field optional. The
[writer](gzip-writer.md) and [compress](gzip/compress.md) write the one of their [options](gzip-options.md) before the
data; the [reader](gzip-reader.md) reads each member's ([header](gzip-reader/header.md)).
[compress_file](gzip/compress_file.md) writes the file's name and time into it, as gzip(1) does. It is Go's
`gzip.Header`, and goes by the name `gzip::header` too.

## Rules

- The name and the comment are ISO 8859-1 in the format (2.3.1); a program's are UTF-8. Written, a string ISO 8859-1
  holds is converted to it; one past U+00FF is written as its UTF-8 bytes, as gzip(1) writes a file's name (Go
  refuses it). Read, bytes that are UTF-8 are taken as they are — a name gzip(1) wrote on a system of UTF-8 names —
  and other bytes are converted from ISO 8859-1. So `name` and `comment` read back as they were written: a string
  ISO 8859-1 holds whose bytes there would read as UTF-8 (`Ã©` is `C3 A9`, the UTF-8 of `é`) is written as UTF-8
  for that reason. A stream of another writer whose ISO 8859-1 happens to be UTF-8 reads as that UTF-8.
- A NUL in the name or the comment cannot be written, nor an extra field past 65 535 bytes: `compress` throws
  `std::invalid_argument`, and a writer's first write (or its close, when nothing was written) gives
  `errc::invalid_argument`, as Go does; the message of either says which field and what is wrong.
- `modified` is written as Unix seconds: one before 1970 or past 2106 is written as 0, which the format reads as "not
  known", and a header read with 0 there has `nullopt`. Read, it is a time in UTC.
- A header of a stream read is at most 1 MiB with its name, comment and extra field: more is `errc::too_large`. A
  header CRC-16, when the stream has one, is checked (`errc::checksum`); the writer writes none.
- The byte of the extra flags is the level's hint, written from the level of the options (2 for 9, 4 for 1), not a
  field of the header.

## Member objects

| Member | Description |
|---|---|
| `name` | the name of the file the data came from, without a directory; empty: none. [compress_file](gzip/compress_file.md) writes the file's name as it is, which [decompress_file](gzip/decompress_file.md) gives back |
| `comment` | a comment; empty: none |
| `modified` | when the data was last modified; `nullopt`: not known |
| `extra` | the extra field, subfields of the program's own; empty: none |
| `os` | the system that made the stream, as RFC 1952 numbers them (0 FAT, 3 Unix, 11 NTFS…); 255, unknown, by default, as Go writes |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    compress::gzip_header h;
    h.name = "café.txt";
    h.comment = "a menu";
    h.modified = time::datetime::from_unix(1790000000, time::zone::utc());

    auto packed = compress::gzip::compress("croissant, 4.50", {.header = h});
    compress::gzip::reader r{io::buffer(packed)};
    auto read = r.header();
    println("{} | {} | {} | {}", read->name, read->comment, *read->modified, read->os);
}
```

Output:

```text
café.txt | a menu | 2026-09-21T14:13:20Z | 255
```

## See also

- [gzip::options](gzip-options.md), [gzip::reader::header](gzip-reader/header.md)
- [sgcl::compress::gzip](gzip.md)
