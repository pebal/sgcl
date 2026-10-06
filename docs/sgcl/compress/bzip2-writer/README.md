[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md)

# sgcl::compress::bzip2::writer

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2 {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::bzip2::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: a bzip2 stream: the level's blocks, each sorted by the Burrows-Wheeler transform and coded with Huffman codes, with its CRC. [close](close.md) writes the rest and the end of the stream and leaves `out` open.

The writer gathers what is written, through the first run-length pass, into a block of the level's size (900 KB
unless told) and compresses it when the block is full, at a [flush](flush.md) and at the [close](close.md). The
memory is two blocks and the arrays of the sorting, about 12 bytes for each byte of a block.

Every error the writer gives is kept as its first: a failure of `out`, a write after the close.
Every write, flush and close after it gives that error at once and writes nothing, so a stream is written freely and
checked once, at the close; [last_error](last_error.md) holds it, and [reset](reset.md) clears it.

## Rules

- The writer is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its stream gone with the move: its [close](close.md) does nothing, a write
  gives `io::errc::closed` (kept as its first error), and a [reset](reset.md) gives it a new stream with the
  settings it was made with. A writer moved onto itself is unchanged.
- Compressing is work for the processor, not a wait: the task's forms work in portions of 64 KB of input and let
  the worker go between them (a yield), so that a large write does not starve the other tasks; only the writes of
  `out` wait ([The rules](../README.md#the-rules)).
- The destructor does not close: a stream not closed has no end, which a reader reports as
  `errc::unexpected_end`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bzip2-writer.md) | a writer into `out`, with the default options or the ones given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [flush, async_flush](flush.md) | writes everything so far, decodable at once |
| [close, async_close](close.md) | writes the rest and the end of the stream; `out` stays open |
| [reset](reset.md) | a new stream into another writer, the memory kept |

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
    string text = "the same words, the same words, the same words again";
    io::buffer sink;
    compress::bzip2::writer w(sink);
    w.write(text);
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::bzip2::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
104
```

## See also

- [bzip2::reader](../bzip2-reader/README.md): the other way
- [bzip2::compress](../bzip2/compress.md): the whole of the data at once
- [bzip2::options](../bzip2-options.md)
- [sgcl::compress::bzip2](../bzip2/README.md)
