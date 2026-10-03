[sgcl](../README.md) › [compress](README.md) › [bzip2](bzip2.md)

# sgcl::compress::bzip2::reader

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2 {
    public:
        class reader;
    };
}
```

`sgcl::compress::bzip2::reader` is an [io reader](../io/reader.md) of what the data read from another reader, `in`,
decompresses to: bzip2 1.0, every stream of it, as Go's `bzip2.NewReader`. Any reader's forms work on it
(`read_all_text`, `copy_to`, an `io::buffered_reader` over it for lines).

Several streams one after another (what `pbzip2` makes, and `cat a.bz2 b.bz2`) are one stream, as `bunzip2` reads
them. A block comes out only when the whole of it has been read (the transform needs all of it): a stream's first
bytes wait for up to 900 KB of the data. Every block's CRC and the stream's CRC are checked (`errc::checksum`); a
block larger than its header allows, selectors or code lengths the format does not allow are `errc::corrupt`; blocks
of the old "randomised" form are `errc::unsupported`. A read decodes into the caller's buffer directly. A failure is
the error of the read that reaches it and of every read after; [last_error](bzip2-reader/last_error.md) holds the
whole of it, offset included, and a read gives it as an `io::error` of the [compress category](compress_category.md).

## Rules

- The reader holds `in`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's frame,
  a managed object. It is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: a read gives `io::errc::closed` (its
  [last_error](bzip2-reader/last_error.md) is `errc::io` with that error), its [close](bzip2-reader/close.md) closes
  nothing, and a [reset](bzip2-reader/reset.md) gives it a new stream. A reader moved onto itself is unchanged.
- Decoding is work for the processor, not a wait: the task's form works in portions of 64 KB of output with a yield
  between them, so that a read of it all does not starve the other tasks; only the reads of `in` wait
  ([The rules](README.md#the-rules)).
- It reads its input 64 KB at a time (32 KB in the managed block of a task's reads), so it may take bytes past the
  end of the bzip2 data from its source.
- A stream has no limit of its own on the output: the program decides how much it reads, and
  [io::limit_reader](../io/limit_reader.md) over the reader bounds it.
- [close](bzip2-reader/close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bzip2-reader/bzip2-reader.md) | a reader of `in` |
| `(destructor)` | destroys the reader; `in` is not closed |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](bzip2-reader/read.md) | decompresses into a buffer |
| [close, async_close](bzip2-reader/close.md) | closes `in` |
| [reset](bzip2-reader/reset.md) | a new stream from another reader, the decoder's memory kept |

#### Observers

| Function | Description |
|---|---|
| [last_error](bzip2-reader/last_error.md) | the error of the data, kept |

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
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // "first line\nsecond line\n" as bzip2 -9 writes it
    auto packed = encoding::hex::decode("425a68393141592653598b13e184000004d180001040000f259c"
                                        "00200021a1323194201a00912a319568cb0482fd57f17724538509"
                                        "08b13e1840");
    compress::bzip2::reader r{io::buffer(*packed)};
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

- [bzip2::decompress](bzip2/decompress.md): the whole of the data at once
- [io::reader](../io/reader.md): what it is
- [sgcl::compress::bzip2](bzip2.md)
