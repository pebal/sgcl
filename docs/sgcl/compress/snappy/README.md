[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::snappy

```cpp
#include "sgcl/compress/snappy.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class snappy;
}
```

`sgcl::compress::snappy` is Snappy, Google's format made for speed: LZ77 with no entropy coding and no levels, its
elements on byte boundaries. Two formats share the name. The block format is the data's length as a varint and the
elements, literals and copies of up to 64 bytes from up to 4 GB back, what LevelDB, Parquet, Cassandra,
Prometheus' remote write and gRPC's snappy codec carry; the framing format (`.sz`, HTTP's `x-snappy-framed`) wraps
blocks in chunks of up to 64 KB of data, each with the masked CRC-32C of its data, after a stream identifier. It is a
class of static functions: the framing format in memory either way ([compress](compress.md),
[decompress](decompress.md)), the block format ([compress_block](compress_block.md),
[decompress_block](decompress_block.md), [decompressed_size](decompressed_size.md)), and a stream each way, a
[writer](../snappy-writer/README.md) and a [reader](../snappy-reader/README.md). Go's standard library has no Snappy.

## Rules

- **No levels and no options.** Snappy has one compressor by design: the data cut into fragments of 64 KB, each
  compressed on its own against a table of four-byte prefixes, so no copy reaches past its fragment.
- **The framing format** begins with its stream identifier (`errc::invalid_header` without it); a chunk that would not
  shrink by an eighth is stored as it is; every chunk's masked CRC-32C is compared (`errc::checksum`); padding and the
  skippable chunks (0x80 to 0xFD) are passed over, and a reserved chunk that may not be skipped (0x02 to 0x7F) is
  `errc::corrupt`. The identifier may come again, so streams one after another are read as one.
- **The block format** carries its length: [decompress_block](decompress_block.md) holds it against the
  [limits](../limits.md)' `max_size` before anything is made, and the data must be exactly that long (`errc::corrupt`);
  [decompressed_size](decompressed_size.md) reads it alone.
- A copy before the start of the data or of offset 0 is `errc::corrupt`; data cut short `errc::unexpected_end`.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [writer](../snappy-writer/README.md) | an io writer: what is written, compressed into another writer in the framing format |
| [reader](../snappy-reader/README.md) | an io reader: what the framed data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data in the framing format (static) |
| [decompress](decompress.md) | framed data decompressed and checked, against the limits (static) |
| [compress_block](compress_block.md) | the block format: the length and the elements (static) |
| [decompress_block](decompress_block.md) | a block of the block format, against the limits (static) |
| [decompressed_size](decompressed_size.md) | the length a block decompresses to, from its head (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words, the same words again. ").repeat(100);
    vector<byte> framed = compress::snappy::compress(text);
    vector<byte> block = compress::snappy::compress_block(text);
    println("{} bytes framed into {}, as a block {}", text.size(), framed.size(), block.size());

    auto back = compress::snappy::decompress(framed);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
5400 bytes framed into 303, as a block 285
5400
```

## See also

- [snappy::writer](../snappy-writer/README.md), [snappy::reader](../snappy-reader/README.md): the streams
- [hash::crc32c](../../hash/crc32c/README.md): the checksum of the chunks
- [lz4](../lz4/README.md): the other fast format, with levels
- `tests/compress/snappy.cpp`: blocks and streams written by hand from the specification (no reference library is on
  the machine the tests were written on); `tests/compress/fuzz/snappy_fuzz.cpp`
- [sgcl::compress](../README.md)
