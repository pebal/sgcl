[sgcl](../../README.md) › [compress](../README.md) › [xz](../xz/README.md)

# sgcl::compress::xz::reader

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        class reader;
    };
}
```

`sgcl::compress::xz::reader` is an [io reader](../../io/reader/README.md) of what the data read from another reader, `in`,
decompresses to: an `.xz` stream, every stream of it. Any reader's forms work on it (`read_all_text`, `copy_to`, an
`io::buffered_reader` over it for lines).

The reader decodes a block into its dictionary, a window the output goes round, and through the block's filters, and
hands the bytes out from there. Every block's check is compared after its last byte is handed out, so a damaged block's
bytes are handed out before the error (`errc::checksum`); the headers, the index and the footer carry CRC-32s of their
own, and the index must list exactly the blocks read (`errc::corrupt`). Streams one after another, with zero bytes
between them in fours, are read as one, as xz reads them. A block's dictionary is held against the limits' `max_memory`
(1 GiB unless told otherwise) before anything is taken: more is `errc::too_large` at the block's offset. A branch
converter keeps its last few bytes (fewer than an instruction) until more come, whatever the size of the reads, and its
state goes on across them. It reads its input 64 KB at a time, so it may take bytes past the end from its source. A
failure is the error of the read that reaches it and of every read after; [last_error](last_error.md) holds
the whole of it, offset included, and a read gives it as an `io::error` of the [compress
category](../compress_category.md).

## Rules

- The reader holds `in`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's frame,
  a managed object. It is move-only, and read by one thread or task at a time.
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
| [(constructor)](xz-reader.md) | a reader of `in`, with the default limits or the ones given |
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
    auto packed = compress::xz::compress("first line\nsecond line\n");
    compress::xz::reader r{io::buffer(packed)};
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

- [xz::writer](../xz-writer/README.md): the other way
- [xz::decompress](../xz/decompress.md): the whole of the data at once
- [io::reader](../../io/reader/README.md): what it is
- [sgcl::compress::xz](../xz/README.md)
