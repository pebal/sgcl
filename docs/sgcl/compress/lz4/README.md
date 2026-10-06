[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::lz4

```cpp
#include "sgcl/compress/lz4.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lz4;
}
```

`sgcl::compress::lz4` is LZ4 (`.lz4`): the fastest format of the module both ways, at a lower ratio than DEFLATE. It
is LZ77 with no entropy coding: a block is a run of sequences, each a token, literals copied as they are, and a match
of four bytes or more reaching back up to 64 KB, every field on a byte boundary, so the decoder copies and never
decodes bits. The frame format (LZ4 Frame Format Description 1.6) wraps the blocks: a header with the largest block
(64 KB to 4 MB), whether a block may refer to the one before it, the checksums asked for (XXH32 of every block, of the
whole content, of the header itself), the content's size and a dictionary's id, then the blocks, an end mark and the
content's checksum. It is a class of static functions and the types of the format: a frame in memory either way
([compress](compress.md), [decompress](decompress.md)), the block format alone without a frame
([compress_block](compress_block.md), [decompress_block](decompress_block.md)), and a stream each way, a
[writer](../lz4-writer/README.md) and a [reader](../lz4-reader/README.md). Go's standard library has no LZ4.

**Levels** are lz4's own ([level](../lz4-level/README.md)): 1, the default, probes one position a byte; 2 probes two
tables; 3 to 9 walk hash chains (lz4hc); 10 to 12 price every way to code a stretch and take the cheapest; a negative
level is the fast compressor with acceleration. Every level makes the same format, decoded at the same speed.

## Rules

- **The frame's defaults are the `lz4` command's**: blocks of 4 MB, independent, the content's checksum, no block
  checksums ([options](../lz4-options.md)). [compress](compress.md) writes the content's size in the header (it is known);
  a [writer](../lz4-writer/README.md) does not.
- **Frames one after another** are read as one, as `lz4 -d` reads them; skippable frames (0x184D2A50 to 0x184D2A5F)
  are passed over, and the legacy frame of `lz4 -l` (blocks of 8 MB, no checksums), which Linux kernel images still
  carry, is read. Anything else after a frame is `errc::invalid_header`.
- **Checks.** Every checksum the frame asks for is compared: the header's (`errc::checksum`), a block's before it is
  decoded, the content's at the end; a content size in the header must be the size decoded (`errc::corrupt`). A block
  larger than the frame's block size, a match before the start of the data or of offset 0, reserved bits set are
  `errc::corrupt` or `errc::invalid_header`.
- **A dictionary**: the last 64 KB of the bytes given come before the data, as if decoded before it; with independent
  blocks every block may refer to them. A frame made with one names its id in the header, and reading it without a
  dictionary, or with another id, is `errc::dictionary_required`. The id is the application's: LZ4 does not define it.
- **Memory.** A frame's block size is held against the [limits](../limits.md)' `max_memory` before anything is taken;
  `decompress` has `max_size` as well, and a content size in the header past it fails before any work.
  [decompress_block](decompress_block.md) takes the size the block decompresses to at most, which the block itself
  does not carry.
- Options out of range (a block size that is not one of the four) are the program's mistake: `compress` throws
  `std::invalid_argument`, and a writer's first write reports `errc::invalid_argument`. A level out of range is refused
  by [level](../lz4-level/README.md) itself.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [level](../lz4-level/README.md) | how hard the compressor works: 1 to 12, or a negative acceleration |
| [block_size](../lz4-block_size.md) | a frame's largest block: 64 KB, 256 KB, 1 MB, 4 MB |
| [options](../lz4-options.md) | the level, the block size and kind, the checksums, a dictionary |
| [writer](../lz4-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../lz4-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one frame (static) |
| [decompress](decompress.md) | every frame decompressed and checked, against the limits (static) |
| [compress_block](compress_block.md) | the block format alone: no frame, no checksum, no size (static) |
| [decompress_block](decompress_block.md) | a block of the block format, into at most the size given (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words, the same words again. ").repeat(100);
    vector<byte> fast = compress::lz4::compress(text);
    vector<byte> small = compress::lz4::compress(text, {.level = 12});
    println("{} bytes into {} or {}", text.size(), fast.size(), small.size());

    auto back = compress::lz4::decompress(small);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
5400 bytes into 92 or 86
5400
```

## See also

- [lz4::writer](../lz4-writer/README.md), [lz4::reader](../lz4-reader/README.md): the streams
- [hash::xxh32](../../hash/xxh32/README.md): the checksum of the frame
- [benchmarks](../benchmarks.md): against liblz4
- `tests/compress/lz4.cpp`: liblz4 as the oracle both ways, frames written by hand from the specification;
  `tests/compress/fuzz/lz4_fuzz.cpp`
- [sgcl::compress](../README.md)
