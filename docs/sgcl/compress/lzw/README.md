[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::lzw

```cpp
#include "sgcl/compress/lzw.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzw;
}
```

`sgcl::compress::lzw` is LZW as Go's `compress/lzw` has it: codes of up to 12 bits after a clear code and an end
code, packed least significant bit first for GIF and most significant first for TIFF and PDF
([order](../lzw-order.md)); literals of 2 to 8 bits (GIF's image data uses 2 to 8, TIFF and PDF 8). The codes below
2^w are the literals of w bits (the literal width), 2^w is the clear code (the table starts over), 2^w + 1 the end
of the data, and each code after the first since a clear makes a new one: the string of the code before it and the
first byte of its own. A code takes w + 1 bits at the start and one more each time the next new code reaches the
next power of two, up to 12 bits (4096 codes). It is a class of static functions and the types of the format: the
whole of the data in memory either way ([compress](compress.md), [decompress](decompress.md)), or a stream
each way, a [writer](../lzw-writer/README.md) and a [reader](../lzw-reader/README.md), each taking the order and the literal width.

## Rules

- A `literal_width` outside 2 to 8 is the program's mistake: `std::invalid_argument`, from every function and
  constructor that takes one. So is a byte the width cannot hold (5 with a width of 2) given to
  [compress](compress.md); the writer's [write](../lzw-writer/write.md) reports it as `errc::invalid_argument`.
- The compressor sends a clear code when the table fills, in place of its last code, as Go does: its output is Go's,
  byte for byte.
- The decompressor stops at the end code and reads nothing after it. A table that fills with no clear stays full
  and gains nothing until a clear comes (the "deferred clear" of GIF encoders, as Go reads it).
- TIFF's "early change" variant (the width grows one code early), which libtiff writes, is not read, as Go does not
  read it either.
- LZW has no checksum and no point to flush at short of the end.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [order](../lzw-order.md) | the order of the bits in the bytes: `lsb` (GIF), `msb` (TIFF, PDF) |
| [writer](../lzw-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../lzw-reader/README.md) | an io reader: what the data read from another reader decompresses to |

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
    string text = string("TOBEORNOTTOBEORTOBEORNOT").repeat(4);
    auto packed = compress::lzw::compress(slice<const byte>(text), compress::lzw::order::msb, 8);
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::lzw::decompress(packed, compress::lzw::order::msb, 8);
    println("{}", string(slice<const byte>(*back)) == text);
}
```

Output:

```text
96 bytes into 43
true
```

## See also

- [codec](../../codec/README.md): GIF, whose image data this is
- `tests/compress`: Go's output as the oracle, byte for byte
- [sgcl::compress](../README.md)
