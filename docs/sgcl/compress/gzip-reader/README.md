[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip/README.md)

# sgcl::compress::gzip::reader

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class gzip {
    public:
        class reader;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::gzip::reader` is an [io reader](../../io/reader/README.md) of what the data read from another reader, `in`,
decompresses to: gzip (RFC 1952): a header, DEFLATE, then the CRC-32 and the length of the data. It is Go's
`gzip.NewReader`. The data is read into an input buffer and decoded into a window of 64 KB whose first half is the
history the back references reach, and the decoded bytes go from the window to the caller; any reader's forms work on it
(`read_all_text`, `copy_to`, an `io::buffered_reader` over it for lines).

Several members one after another are one stream: `cat a.gz b.gz > c.gz` is valid gzip, and the reader gives both parts
one after another, as gunzip and Go do. Bytes after the last member that are not another one are `errc::invalid_header`
(fewer than a header's ten bytes, `errc::unexpected_end`), as in Go. Constructed with `gzip::single_member`, it stops
after the first member; it reads its input a block at a time (16 KB), so a format that keeps other data after the member
gives the reader only the member's part ([io::limit_reader](../../io/limit_reader/README.md)). Each member's header is checked
(`errc::invalid_header`; a header CRC-16, when there is one, `errc::checksum`; a method other than deflate
`errc::unsupported`; a header past 1 MiB `errc::too_large`), the data as it is decoded (`errc::corrupt`), and the CRC-32
and the length of the trailer (`errc::checksum`, `errc::corrupt`), at the read that reaches each. The header's strings
are taken as they are when they are UTF-8 (gzip(1) writes a file's name so) and converted from ISO 8859-1, the format's
own, when they are not ([header](header.md)). A failure is the error of the read
that reaches it and of every read after; [last_error](last_error.md) holds the whole of it, offset included,
and a read gives it as an `io::error` of the [compress category](../compress_category.md).

## Rules

- The reader is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](last_error.md) is `errc::io` with that error), its [close](close.md) closes
  nothing, and a [reset](reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form lets the worker go every 64 KB handed out (a
  yield), so that a read of it all does not starve the other tasks; only the reads of `in` wait
  ([The rules](../README.md#the-rules)).
- A stream has no limit of its own: the program decides how much it reads, and
  [io::limit_reader](../../io/limit_reader/README.md) over the reader bounds it.
- [close](close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](gzip-reader.md) | a reader of `in`, of every member or of the first |
| `(destructor)` | destroys the reader; `in` is not closed |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](read.md) | decompresses into a buffer |
| [close, async_close](close.md) | closes `in` |
| [reset](reset.md) | a new stream from another reader, the decoder's memory kept |

#### Observers

| Function | Description |
|---|---|
| [header, async_header](header.md) | the header of the member being read |
| [last_error](last_error.md) | the error of the data, kept |

#### From mixin::reader

| Function | Description |
|---|---|
| [read_full, async_read_full](../../io/mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](../../io/mixin/reader/read_all.md) | everything to the end, as bytes |
| [read_all_text, async_read_all_text](../../io/mixin/reader/read_all_text.md) | everything to the end, as text |
| [copy_to, async_copy_to](../../io/mixin/reader/copy_to.md) | everything to the end, written to a writer |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::gzip::compress("first line\nsecond line\n");
    compress::gzip::reader r{io::buffer(packed)};
    io::buffered_reader lines(r);
    for (auto line : lines.lines()) {
        println("[{}]", line);
    }
}
```

Output:

```text
[first line]
[second line]
```

## See also

- [gzip::writer](../gzip-writer/README.md): the other way
- [gzip::decompress](../gzip/decompress.md): the whole of the data at once
- [io::reader](../../io/reader/README.md): what it is
- [sgcl::compress::gzip](../gzip/README.md)
