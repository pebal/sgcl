[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib/README.md)

# sgcl::compress::zlib::writer

```cpp
#include "sgcl/compress/zlib.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zlib {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zlib::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: zlib (RFC 1950): DEFLATE between a two-byte header and the Adler-32 of the data. It is Go's
`zlib.NewWriter`. What is written is compressed in pieces of 64 KB of input, and what the pieces make is written to
`out` as it comes, so the output comes some way behind the input: [flush](flush.md) makes everything written
so far decodable at once, and [close](close.md) writes the last block and the Adler-32 of the data and
leaves `out` open, for a format or a protocol that goes on after it (a zip entry, an HTTP body).

Every error the writer gives is kept as its first: a failure of `out`, a write or a flush after the close. Every write,
flush and close after it gives that error at once and writes nothing, so a stream is written freely and checked once, at
the close; [last_error](last_error.md) holds it, and [reset](reset.md) clears it with the rest
of the stream.

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
- A dictionary given in the [options](../zlib-options.md) is copied: the bytes need live only through the constructor. Of a
  dictionary longer than the window, its last 32 KB are the history; at level 0 nothing is matched and the dictionary is
  not used.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zlib-writer.md) | a writer into `out`, with the default options or the ones given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [flush, async_flush](flush.md) | makes everything written so far decodable (a sync flush) |
| [close, async_close](close.md) | writes the last block and the Adler-32 of the data; `out` stays open |
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
    compress::zlib::writer w(sink, {.level = 9});
    w.write("one line\n");
    w.write("another line\n");
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::zlib::reader r(sink);
    print("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
one line
another line
```

## See also

- [zlib::reader](../zlib-reader/README.md): the other way
- [zlib::compress](../zlib/compress.md): the whole of the data at once
- [io::writer](../../io/writer/README.md): what it is
- [sgcl::compress::zlib](../zlib/README.md)
