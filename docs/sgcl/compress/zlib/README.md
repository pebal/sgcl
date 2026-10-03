[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::zlib

```cpp
#include "sgcl/compress/zlib.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zlib;
}
```

`sgcl::compress::zlib` is zlib (RFC 1950): DEFLATE between a two-byte header (the method, the window, a hint of the
level) and the Adler-32 of the data, which the reader checks at the end (`errc::checksum`). PNG's image data, PDF's
streams and many protocols are zlib. It is Go's `compress/zlib`, as a class of static functions and the types of the
format: the whole of the data in memory either way ([compress](compress.md),
[decompress](decompress.md)), or a stream each way, a [writer](../zlib-writer/README.md) and a [reader](../zlib-reader/README.md),
[flate's](../flate/README.md) with the header and the trailer around the data.

A stream made with a **preset dictionary** names it in its header by the dictionary's Adler-32. Reading it without
that dictionary, or with another, is `errc::dictionary_required`; [dictionary_id](dictionary_id.md) reads the
name from the header of data in memory, and the reader's [dictionary_id](../zlib-reader/dictionary_id.md) once it has
read the header, so that a program with several dictionaries picks the right one.

## Rules

- The output is valid zlib that any decoder reads, at the sizes of [flate](../flate/README.md)'s levels plus six bytes (eight
  more with a dictionary): the header with the level's hint (0 for levels 0, 1 and `huffman_only`, 1 below 6, 2
  for 6, 3 above), the data, the Adler-32.
- The header must be DEFLATE with a window of 32 KB or less, and its check bits must hold
  (`errc::invalid_header`).
- [decompress](decompress.md) and the [reader](../zlib-reader/README.md) stop at the end of the stream and take nothing
  after it, as zlib and Go do.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [options](../zlib-options.md) | the level and a preset dictionary |
| [writer](../zlib-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../zlib-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed (static) |
| [decompress](decompress.md) | the whole of the data decompressed, checked against the limits (static) |
| [dictionary_id](dictionary_id.md) | the Adler-32 of the dictionary a stream in memory was made with (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string sample = "{\"name\": \"\", \"email\": \"\", \"active\": true}";
    string record = "{\"name\": \"Ann\", \"email\": \"ann@example.com\", \"active\": true}";
    compress::zlib::options o{.dictionary = slice<const byte>(sample)};
    auto packed = compress::zlib::compress(record, o);

    // the receiver picks its dictionary by the name in the header
    auto id = compress::zlib::dictionary_id(packed);
    println("{}", id == hash::adler32::of(slice<const byte>(sample)));
    auto back = compress::zlib::decompress(packed, o);
    println("{}", string(slice<const byte>(*back)));
}
```

Output:

```text
true
{"name": "Ann", "email": "ann@example.com", "active": true}
```

## See also

- [flate](../flate/README.md): DEFLATE alone; [gzip](../gzip/README.md): DEFLATE with a header and a CRC-32
- [hash::adler32](../../hash/README.md): the checksum
- [sgcl::compress](../README.md)
