[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz/README.md)

# sgcl::compress::xz::writer

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        class writer;
    };
}
```

`sgcl::compress::xz::writer` is an [io writer](../../io/writer/README.md) that compresses what is written to it into another
writer, `out`: the `.xz` format of XZ Utils: LZMA2 in a container that checks what it holds. [close](close.md)
writes the rest of the data, the block's check, the index and the footer and leaves `out` open.

The writer makes one stream of one block without its sizes in its header, as single-threaded xz does; empty data is a
stream of no block (32 bytes). What is written goes through the filters of the [options](../xz-options.md) into LZMA2 and
out as the parser can see far enough ahead (about 4 KB), so the output comes some way behind the input; xz has no flush,
and nothing is decodable before the [close](close.md). The memory is [lzma](../lzma/README.md)'s at the same level.

Every error the writer gives is kept as its first: a failure of `out`, options out of range, a write after the close.
Every write and close after it gives that error at once and writes nothing, so a stream is written freely and checked
once, at the close; [last_error](last_error.md) holds it, and [reset](reset.md) clears it.

## Rules

- The writer holds `out`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's
  frame, a managed object. It is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its stream gone with the move: its [close](close.md) does nothing, a write
  gives `io::errc::closed` (kept as its first error), and a [reset](reset.md) gives it a new stream with the
  settings it was made with. A writer moved onto itself is unchanged.
- Compressing is work for the processor, not a wait: the task's forms work in portions of 64 KB of input and let
  the worker go between them (a yield), so that a large write does not starve the other tasks; only the writes of
  `out` wait ([The rules](../README.md#the-rules)).
- Options out of range (a Delta distance past 256, a check that is not one of the four, a dictionary under 4 KiB or past
  1.5 GiB, `level::huffman_only`) are the program's mistake, which the writer reports at its first write as
  `errc::invalid_argument`, kept as its first error.
- The destructor does not close: a stream not closed has no end, which a reader reports as
  `errc::unexpected_end`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xz-writer.md) | a writer into `out`, with the default options or the ones given |
| `(destructor)` | destroys the writer; the stream is not closed |

#### Operations

| Function | Description |
|---|---|
| [write, async_write](write.md) | compresses bytes into `out` |
| [close, async_close](close.md) | writes the rest of the data, the block's check, the index and the footer; `out` stays open |
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
    compress::xz::writer w(sink, {.level = 1});
    w.write(text);
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::xz::reader r(sink);
    println("{}", r.read_all_text()->size());
}
```

Output:

```text
104
```

## See also

- [xz::reader](../xz-reader/README.md): the other way
- [xz::compress](../xz/compress.md): the whole of the data at once
- [xz::options](../xz-options.md)
- [sgcl::compress::xz](../xz/README.md)
