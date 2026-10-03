[sgcl](../README.md) › [compress](README.md) › [lzw](lzw.md)

# sgcl::compress::lzw::reader

```cpp
#include "sgcl/compress/lzw.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzw {
    public:
        class reader;
    };
}
```

`sgcl::compress::lzw::reader` is an [io reader](../io/reader.md) of what the data read from another reader, `in`,
decompresses to: LZW codes of the order and the literal width given, up to the end code. Any reader's forms work on it
(`read_all_text`, `copy_to`, an `io::buffered_reader` over it for lines).

It stops at the end code and reads nothing after it, but it reads its input 16 KB at a time, so it may take bytes past
the end code from its source. A table that fills with no clear stays full and gains nothing until a clear comes (the
"deferred clear" of GIF encoders, as Go reads it). A code that is not one yet is `errc::corrupt`; data that ends before
the end code is `errc::unexpected_end`. A read decodes up to 64 KB into the buffer it hands out from. A failure is the
error of the read that reaches it and of every read after; [last_error](lzw-reader/last_error.md) holds the whole of it,
offset included, and a read gives it as an `io::error` of the [compress category](compress_category.md).

## Rules

- The reader holds `in`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's frame,
  a managed object. It is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](lzw-reader/last_error.md) is `errc::io` with that error), its [close](lzw-reader/close.md) closes
  nothing, and a [reset](lzw-reader/reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form lets the worker go every 64 KB handed out (a yield),
  so that a read of it all does not starve the other tasks; only the reads of `in` wait ([The
  rules](README.md#the-rules)).
- A stream has no limit of its own on the output: the program decides how much it reads, and
  [io::limit_reader](../io/limit_reader.md) over the reader bounds it.
- [close](lzw-reader/close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](lzw-reader/lzw-reader.md) | a reader of `in`, of the order and the literal width given |
| `(destructor)` | destroys the reader; `in` is not closed |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](lzw-reader/read.md) | decompresses into a buffer |
| [close, async_close](lzw-reader/close.md) | closes `in` |
| [reset](lzw-reader/reset.md) | a new stream from another reader, the decoder's memory kept |

#### Observers

| Function | Description |
|---|---|
| [last_error](lzw-reader/last_error.md) | the error of the data, kept |

#### From mixin::reader

| Function | Description |
|---|---|
| [read_full, async_read_full](../io/mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](../io/mixin/reader/read_all.md) | everything to the end, as bytes |
| [read_all_text, async_read_all_text](../io/mixin/reader/read_all_text.md) | everything to the end, as text |
| [copy_to, async_copy_to](../io/mixin/reader/copy_to.md) | everything to the end, written to a writer |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "first line\nsecond line\n";
    auto packed = compress::lzw::compress(slice<const byte>(text), compress::lzw::order::lsb, 8);
    compress::lzw::reader r{io::buffer(packed), compress::lzw::order::lsb, 8};
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

- [lzw::writer](lzw-writer.md): the other way
- [lzw::decompress](lzw/decompress.md): the whole of the data at once
- [io::reader](../io/reader.md): what it is
- [sgcl::compress::lzw](lzw.md)
