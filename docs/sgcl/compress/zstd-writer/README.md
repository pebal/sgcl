[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md)

# sgcl::compress::zstd::writer

```cpp
#include "sgcl/compress/zstd.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zstd {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zstd::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: one Zstandard frame (RFC 8878): blocks of up to 128 KB, each compressed against the window before it, and the XXH64 of the content at the end. [close](close.md) writes the rest and the end of the stream and leaves `out` open.

The writer gathers what is written into a block of 128 KB and compresses it when the block is full, at a
[flush](flush.md) and at the [close](close.md); a block that would not shrink goes as it is, and one of a single byte
repeated as that byte. Matches reach back the whole window, which the level sets (2^19 bytes at level 1 to 2^23 at
19, 2^27 at 22) unless the options give one, and the frame's header announces it, so that a reader knows the memory it
needs. The memory is twice the window, two blocks and the tables of the level.

Every error the writer gives is kept as its first: a failure of `out`, options out of range, a write after the close.
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
- Options out of range (a window_log outside 10..31) are the program's mistake, which the writer reports at its first write as
  `errc::invalid_argument`, kept as its first error.
- The destructor does not close: a stream not closed has no end, which a reader reports as
  `errc::unexpected_end`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zstd-writer.md) | a writer into `out`, with the default options or the ones given |
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
    compress::zstd::writer w(sink);
    w.write(text);
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::zstd::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
104
```

## See also

- [zstd::reader](../zstd-reader/README.md): the other way
- [zstd::compress](../zstd/compress.md): the whole of the data at once
- [zstd::options](../zstd-options.md)
- [sgcl::compress::zstd](../zstd/README.md)
