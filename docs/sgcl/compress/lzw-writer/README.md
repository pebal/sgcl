[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw/README.md)

# sgcl::compress::lzw::writer

```cpp
#include "sgcl/compress/lzw.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzw {
    public:
        class writer;
    };
}
```

`sgcl::compress::lzw::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: LZW codes of the order and the literal width given, as Go's `lzw.NewWriter`. What is written is coded
64 KB of input at a time and the codes are written to `out` as they come; [close](close.md) writes the end
code and leaves `out` open, as Go's does. LZW has no point to flush at short of the end.

A failure of `out`, a byte the literal width cannot hold, or a write after the close is kept for good: every write and
close after it gives that error at once and writes nothing, so a stream is written freely and checked once, at the close;
[last_error](last_error.md) holds it, and [reset](reset.md) clears it.

## Rules

- The writer holds `out`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's
  frame, a managed object. It is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its stream gone with the move: its [close](close.md) does nothing, a write
  gives `io::errc::closed` (kept as its first error), and a [reset](reset.md) gives it a new stream with
  the settings it was made with. A writer moved onto itself is unchanged.
- Compressing is work for the processor, not a wait: the task's form works 64 KB of input at a time and lets the
  worker go between the pieces (a yield); only the writes of `out` wait ([The rules](../README.md#the-rules)).
- The destructor does not close: a stream not closed has no end code, which a reader reports as
  `errc::unexpected_end`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](lzw-writer.md) | a writer into `out`, of the order and the literal width given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [close, async_close](close.md) | writes the end code; `out` stays open |
| [reset](reset.md) | a new stream into another writer, the encoder's table kept |

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
    compress::lzw::writer w(sink, compress::lzw::order::msb, 8);
    w.write("TOBEORNOTTOBEORTOBEORNOT");
    if (auto done = w.close(); !done) {
        println("{}", done.error().message());
        return 1;
    }
    compress::lzw::reader r(sink, compress::lzw::order::msb, 8);
    println("{}", r.read_all_text().value_or(string()));
}
```

Output:

```text
TOBEORNOTTOBEORTOBEORNOT
```

## See also

- [lzw::reader](../lzw-reader/README.md): the other way
- [lzw::compress](../lzw/compress.md): the whole of the data at once
- [sgcl::compress::lzw](../lzw/README.md)
