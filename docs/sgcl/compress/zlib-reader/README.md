[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib/README.md)

# sgcl::compress::zlib::reader

```cpp
#include "sgcl/compress/zlib.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zlib {
    public:
        class reader;
    };
}
```

`sgcl::compress::zlib::reader` is an [io reader](../../io/reader/README.md) of what the data read from another reader, `in`,
decompresses to: zlib (RFC 1950): DEFLATE between a two-byte header and the Adler-32 of the data. It is Go's
`zlib.NewReader`. The data is read into an input buffer and decoded into a window of 64 KB whose first half is the
history the back references reach, and the decoded bytes go from the window to the caller; any reader's forms work on it
(`read_all_text`, `copy_to`, an `io::buffered_reader` over it for lines).

It stops at the end of the zlib stream (the Adler-32 after the data) and gives nothing after it, as zlib and Go do; it
reads its input a block at a time (16 KB), so it may take bytes past that end from its source. The header is checked
(`errc::invalid_header`), the data as it is decoded (`errc::corrupt`), and the Adler-32 at the end (`errc::checksum`),
at the read that reaches each. A stream made with a preset dictionary is read with that dictionary in the
[options](../zlib-options.md); without it, or with another, the first read is `errc::dictionary_required`, and
[dictionary_id](dictionary_id.md) names the one it needs. A failure is the error of the read that reaches it
and of every read after; [last_error](last_error.md) holds the whole of it, offset included, and a read
gives it as an `io::error` of the [compress category](../compress_category.md).

## Rules

- The reader holds `in`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's frame,
  a managed object. It is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](last_error.md) is `errc::io` with that error), its [close](close.md) closes
  nothing, and a [reset](reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form lets the worker go every 64 KB handed out (a
  yield), so that a read of it all does not starve the other tasks; only the reads of `in` wait
  ([The rules](../README.md#the-rules)).
- A stream has no limit of its own: the program decides how much it reads, and
  [io::limit_reader](../../io/limit_reader/README.md) over the reader bounds it.
- [close](close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zlib-reader.md) | a reader of `in`, with a dictionary |
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
| [dictionary_id](dictionary_id.md) | the Adler-32 of the dictionary the stream asks for |
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
    auto packed = compress::zlib::compress("first line\nsecond line\n");
    compress::zlib::reader r{io::buffer(packed)};
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

- [zlib::writer](../zlib-writer/README.md): the other way
- [zlib::decompress](../zlib/decompress.md): the whole of the data at once
- [io::reader](../../io/reader/README.md): what it is
- [sgcl::compress::zlib](../zlib/README.md)
