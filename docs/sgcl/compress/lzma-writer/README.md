[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma/README.md)

# sgcl::compress::lzma::writer

```cpp
#include "sgcl/compress/lzma.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzma {
    public:
        class writer;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::lzma::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: LZMA in its first file format, "LZMA alone". [close](close.md) writes the end marker and the
range coder's last bytes and leaves `out` open.

The header says the size is not known (all ones), and the data ends with the end marker, as xz writes it. The writer
copies what is written into a window of its own and codes it as the parser can see far enough ahead of each position
(about 4 KB), so the output comes some way behind the input and all of it only at the [close](close.md).
LZMA has no flush: nothing written is decodable before the close. The tables and the window are taken at the first
write: about 10 times the dictionary at levels 4 to 9 (80 MiB at level 6) and 6 times at levels 0 to 3, and a window of
about 1.25 times the dictionary ([options](../lzma-options.md)).

Every error the writer gives is kept as its first: a failure of `out`, options out of range, a write after the close.
Every write and close after it gives that error at once and writes nothing, so a stream is written freely and checked
once, at the close; [last_error](last_error.md) holds it, and [reset](reset.md) clears it.

## Rules

- The writer is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its stream gone with the move: its [close](close.md) does nothing, a
  write gives `io::errc::closed` (kept as its first error), and a [reset](reset.md) gives it a new stream
  with the settings it was made with. A writer moved onto itself is unchanged.
- Compressing is work for the processor, not a wait: the task's forms work in portions of 64 KB of input and let
  the worker go between them (a yield), so that a large write does not starve the other tasks; only the writes of
  `out` wait ([The rules](../README.md#the-rules)).
- Options out of range (lc past 8, lp or pb past 4, a dictionary under 4 KiB or past 1.5 GiB, `level::huffman_only`) are
  the program's mistake, which the writer reports at its first write as `errc::invalid_argument`, kept as its first
  error.
- The destructor does not close: a stream not closed has no end, which a reader reports as
  `errc::unexpected_end`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](lzma-writer.md) | a writer into `out`, with the default options or the ones given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [close, async_close](close.md) | writes the end marker and the range coder's last bytes; `out` stays open |
| [reset](reset.md) | a new stream into another writer, the window and the tables kept |

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
    compress::lzma::writer w(sink, {.level = 1});
    w.write(text);
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::lzma::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
104
```

## See also

- [lzma::reader](../lzma-reader/README.md): the other way
- [lzma::compress](../lzma/compress.md): the whole of the data at once
- [lzma::options](../lzma-options.md)
- [sgcl::compress::lzma](../lzma/README.md)
