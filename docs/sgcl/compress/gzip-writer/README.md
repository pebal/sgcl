[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip/README.md)

# sgcl::compress::gzip::writer

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class gzip {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::gzip::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: gzip (RFC 1952): a header, DEFLATE, then the CRC-32 and the length of the data. It is Go's
`gzip.NewWriter`. What is written is compressed in pieces of 64 KB of input, and what the pieces make is written to
`out` as it comes, so the output comes some way behind the input: [flush](flush.md) makes everything written
so far decodable at once, and [close](close.md) writes the last block, the CRC-32 and the length of the data
(and the header, when nothing was written) and leaves `out` open, for a format or a protocol that goes on after it (a
zip entry, an HTTP body).

Every error the writer gives is kept as its first: a failure of `out`, a write or a flush after the close, a header the
format cannot write. Every write, flush and close after it gives that error at once and writes nothing, so a stream is
written freely and checked once, at the close; [last_error](last_error.md) holds it, and
[reset](reset.md) clears it with the rest of the stream.

## Rules

- The writer is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its stream gone with the move: its [close](close.md) does nothing, a
  write gives `io::errc::closed` (kept as its first error), and a [reset](reset.md) gives it a new stream
  with the settings it was made with. A writer moved onto itself is unchanged.
- Compressing is work for the processor, not a wait: the task's forms do it on the worker and let it go between
  the pieces of 64 KB (a yield), so that a large write does not starve the other tasks; only the writes of `out`
  wait ([The rules](../README.md#the-rules)).
- The destructor does not close: a stream not closed ends without its last block and its trailer, which a reader reports
  as `errc::unexpected_end`.
- The header of the options ([gzip_header](../gzip_header.md)) is written before the first bytes. A name or a comment
  past ISO 8859-1 is written as its UTF-8 bytes, as gzip(1) writes a file's name. A header the format cannot write — a
  name or a comment with a NUL, an extra field past 65 535 bytes — fails the first write (or the close, when nothing
  was written) with `errc::invalid_argument`, as Go does, and is kept as the writer's first error; its message says
  what is wrong.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](gzip-writer.md) | a writer into `out`, with the default options or the ones given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [flush, async_flush](flush.md) | makes everything written so far decodable (a sync flush) |
| [close, async_close](close.md) | writes the last block, the CRC-32 and the length of the data; `out` stays open |
| [reset](reset.md) | a new stream into another writer, the encoder's memory kept |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | checks whether the stream was closed |
| [last_error](last_error.md) | the first error the writer gave, kept |

#### From mixin::writer

| Function | Description |
|---|---|
| [write, async_write](../../io/mixin/writer/write.md) | writes a string, a text slice, a literal, a C string, a `std::string_view` or one byte |
| [copy_from, async_copy_from](../../io/mixin/writer/copy_from.md) | writes everything a reader gives, to its end |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::gzip::writer w(sink, {.level = 9});
    w.write("one line\n");
    w.write("another line\n");
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::gzip::reader r(sink);
    print("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
one line
another line
```

## See also

- [gzip::reader](../gzip-reader/README.md): the other way
- [gzip::compress](../gzip/compress.md): the whole of the data at once
- [io::writer](../../io/writer/README.md): what it is
- [sgcl::compress::gzip](../gzip/README.md)
