[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::flate

```cpp
#include "sgcl/compress/flate.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class flate;
}
```

`sgcl::compress::flate` is DEFLATE (RFC 1951) with nothing around it: the data of a zip entry, of PNG (inside
[zlib](../zlib/README.md)) and of [gzip](../gzip/README.md), and HTTP's `deflate` as browsers read it. It is Go's `compress/flate`, as a
class of static functions and the types of the format: the whole of the data in memory either way
([compress](compress.md), [decompress](decompress.md)), or a stream each way, a
[writer](../flate-writer/README.md) that compresses what is written to it into another writer and a [reader](../flate-reader/README.md)
of what the data read from another reader decompresses to.

`compress` makes the shortest of the format's three forms for every block — its own Huffman codes, the fixed ones,
or stored — from a search of 32 KB of history whose effort the [level](../level/README.md) sets; `decompress` takes any valid
DEFLATE, whoever made it. A **dictionary** is data both sides agree on in advance, which the first matches may refer
to: short messages of one kind (JSON of one schema, HTTP headers) compress much better against a sample of them
(Go's `NewWriterDict`). The reader must be given the same bytes ([options](../flate-options.md)).

## Rules

- The output is valid DEFLATE that any decoder reads (this decoder, zlib's and Go's read every level's), but not
  byte for byte what zlib or Go make for the same input: the format leaves the encoder its choices. At levels 7 to 9
  its size is within 1 % of zlib's at the same level; levels 1 to 6 are the table encoder, Go's kind, a few per cent
  larger (the default, 6, some 3 % on text) ([level](../level/README.md)).
- [decompress](decompress.md) and the [reader](../flate-reader/README.md) stop at the end of the DEFLATE data and give
  nothing after it, as zlib and Go do. The reader reads its input a block at a time, so it may take bytes past that
  end from its source: a format with more after it (a zip entry, a gzip trailer) gives the reader only its part
  ([io::limit_reader](../../io/limit_reader/README.md)), or uses [gzip](../gzip/README.md) and [zlib](../zlib/README.md), which handle their trailers
  themselves.
- A decoded stream is checked as it is read: a code that is not one, a distance before the start or a stored block
  whose length does not match its complement is `errc::corrupt` at the read that reaches it, and the bytes before it
  were handed out.
- DEFLATE has no checksum: a flipped bit may decode to other bytes without an error. Data that must arrive intact
  goes in zlib or gzip, which check it.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [options](../flate-options.md) | the level and a preset dictionary |
| [writer](../flate-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../flate-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed (static) |
| [decompress](decompress.md) | the whole of the data decompressed, checked against the limits (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(100);
    vector<byte> packed = compress::flate::compress(text);
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::flate::decompress(packed);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", string(slice<const byte>(*back)) == text);
}
```

Output:

```text
4800 bytes into 65
true
```

## See also

- [zlib](../zlib/README.md), [gzip](../gzip/README.md): DEFLATE with a header and a checksum
- [level](../level/README.md): the encoders behind the levels
- [benchmarks](../benchmarks.md): against zlib and Go
- `tests/compress`: the format against zlib and Go, both ways
- [sgcl::compress](../README.md)
