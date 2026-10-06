[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md)

# sgcl::compress::brotli::reader

```cpp
#include "sgcl/compress/brotli.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class brotli {
    public:
        class reader;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::brotli::reader` is an [io reader](../../io/reader/README.md) of what the data read from another reader, `in`,
decompresses to: brotli data, every stream of it. Any reader's forms work on it (`read_all_text`, `copy_to`, an
`io::buffered_reader` over it for lines).

The reader decodes into a window of the stream's size and 256 KB more, and hands the bytes out from there; it
stops between any two commands where its input runs out, so a stream may arrive a byte at a time. A stream has no
checksum: the RFC's checks (complete prefix codes, distances within the data, the meta-blocks' lengths, padding of
zeros) are what tells damage. The reader stops at the stream's last meta-block and leaves what follows it. A
stream's window is held against the limits' `max_memory` before anything is taken. It reads its input 64 KB at a time, so it may take bytes past the end from its source. A failure is
the error of the read that reaches it and of every read after; [last_error](last_error.md) holds the whole of it,
offset included, and a read gives it as an `io::error` of the [compress category](../compress_category.md).

## Rules

- The reader is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](last_error.md) is `errc::io` with that error), its [close](close.md) closes
  nothing, and a [reset](reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form lets the worker go every 64 KB handed out (a yield),
  so that a read of it all does not starve the other tasks; only the reads of `in` wait ([The
  rules](../README.md#the-rules)).
- A stream has no limit of its own on the output: the program decides how much it reads, and
  [io::limit_reader](../../io/limit_reader/README.md) over the reader bounds it.
- [close](close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](brotli-reader.md) | a reader of `in` |
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
    auto packed = compress::brotli::compress("first line\nsecond line\n");
    compress::brotli::reader r{io::buffer(packed)};
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

- [brotli::writer](../brotli-writer/README.md): the other way
- [brotli::decompress](../brotli/decompress.md): the whole of the data at once
- [io::reader](../../io/reader/README.md): what it is
- [sgcl::compress::brotli](../brotli/README.md)
