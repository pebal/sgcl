[sgcl](../README.md) › [compress](README.md) › [flate](flate.md)

# sgcl::compress::flate::reader

```cpp
#include "sgcl/compress/flate.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class flate {
    public:
        class reader;
    };
}
```

`sgcl::compress::flate::reader` is an [io reader](../io/reader.md) of what the data read from another reader, `in`,
decompresses to: DEFLATE (RFC 1951) with nothing around it. It is Go's `flate.NewReader`. The data is read into an input
buffer and decoded into a window of 64 KB whose first half is the history the back references reach, and the decoded
bytes go from the window to the caller; any reader's forms work on it (`read_all_text`, `copy_to`, an
`io::buffered_reader` over it for lines).

It stops at the end of the DEFLATE data and gives nothing after it, as zlib and Go do. It reads its input a block at a
time (16 KB), so it may take bytes past that end from its source: a format with more after the data (a zip entry, a gzip
trailer) gives the reader only its part ([io::limit_reader](../io/limit_reader.md)), or uses [gzip](gzip.md) and
[zlib](zlib.md), which handle their trailers themselves. A code that is not one, a distance before the start or a stored
block whose length does not match its complement is `errc::corrupt` at the read that reaches it, and the bytes before it
were handed out. A failure is the error of the read that reaches it and of every read after;
[last_error](flate-reader/last_error.md) holds the whole of it, offset included, and a read gives it as an `io::error`
of the [compress category](compress_category.md).

## Rules

- The reader holds `in`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's frame,
  a managed object. It is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](flate-reader/last_error.md) is `errc::io` with that error), its [close](flate-reader/close.md) closes
  nothing, and a [reset](flate-reader/reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form lets the worker go every 64 KB handed out (a
  yield), so that a read of it all does not starve the other tasks; only the reads of `in` wait
  ([The rules](README.md#the-rules)).
- A stream has no limit of its own: the program decides how much it reads, and
  [io::limit_reader](../io/limit_reader.md) over the reader bounds it.
- [close](flate-reader/close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](flate-reader/flate-reader.md) | a reader of `in`, with a dictionary |
| `(destructor)` | destroys the reader; `in` is not closed |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](flate-reader/read.md) | decompresses into a buffer |
| [close, async_close](flate-reader/close.md) | closes `in` |
| [reset](flate-reader/reset.md) | a new stream from another reader, the decoder's memory kept |

#### Observers

| Function | Description |
|---|---|
| [last_error](flate-reader/last_error.md) | the error of the data, kept |

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
    auto packed = compress::flate::compress("first line\nsecond line\n");
    compress::flate::reader r{io::buffer(packed)};
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

- [flate::writer](flate-writer.md): the other way
- [flate::decompress](flate/decompress.md): the whole of the data at once
- [io::reader](../io/reader.md): what it is
- [sgcl::compress::flate](flate.md)
