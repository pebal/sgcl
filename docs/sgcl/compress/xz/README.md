[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::xz

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz;
}
```

`sgcl::compress::xz` is the `.xz` format of XZ Utils (`.xz`, `.txz`, `.tar.xz`), xz-file-format 1.2:
[LZMA](../lzma/README.md) in its second form, LZMA2, inside a container that checks what it holds. A stream is a header naming
the check, blocks — each a header listing its filters, the compressed data, and the check of the data decompressed —
an index of the blocks' sizes, and a footer. It is a class of static functions and the types of the format: the
whole of the data in memory either way ([compress](compress.md), [decompress](decompress.md)), or a stream
each way, a [writer](../xz-writer/README.md) and a [reader](../xz-reader/README.md). The levels are [lzma](../lzma/README.md)'s (xz's `-0` to `-9`
and `-e`), and so is the memory.

**LZMA2** codes the data in chunks of up to 2 MiB, each of LZMA with a range coder of its own or stored as it is
when LZMA would not make it smaller (random data grows by 0.01 % instead of LZMA's 1.4 %). **Filters** go before
LZMA2 and keep the length: a branch converter ([filter](../xz-filter.md)) turns the relative targets of calls and jumps
in machine code into absolute ones, so that calls to one function from many places look alike; Delta codes each byte
as its difference from the byte `delta` before it, for uncompressed sound or images whose samples are that many
bytes apart. Both at once: the converter first ([options](../xz-options.md)).

## Rules

- **Checks.** Every block's check ([check](../xz-check.md)) is compared at the block's end (`errc::checksum`); the
  headers, the index and the footer carry CRC-32s of their own, and the index must list exactly the blocks read
  (`errc::corrupt`). `check::none` is read and written, as xz does: the data is then only as safe as LZMA2's
  structure makes it. Check types the format reserves but does not define are `errc::unsupported` (xz warns and
  goes on).
- **Streams.** Streams one after another, with zero bytes between them in fours (stream padding), are read as one,
  as xz reads them; padding not a multiple of four is `errc::corrupt`, anything else after a stream
  `errc::invalid_header`. Several blocks in a stream (what `xz -T` writes) are read in order. On reading, every
  chain xz writes is taken: up to three filters before LZMA2, start offsets of the converters included.
- **Writing.** [compress](compress.md) writes one block with its sizes in its header; a [writer](../xz-writer/README.md)
  one block without them, as single-threaded xz does. Empty data is a stream of no block (32 bytes), as xz makes
  it. Several blocks and several threads are not written in this version.
- **Memory.** A block's dictionary is as large as its header asks, or as the block when the header gives the block's
  size, and is held against the [limits](../limits.md)' `max_memory` before anything is taken — more is
  `errc::too_large` at the block's offset, in memory and in a stream alike. `decompress` has `max_size` as well; a
  block's size in its header past it fails before any work.
- Options out of range (a Delta distance past 256, a check that is not one of the four, a dictionary under 4 KiB or
  past 1.5 GiB, `level::huffman_only`) are the program's mistake: `compress` throws `std::invalid_argument`, and a
  writer's first write reports `errc::invalid_argument`.
- The branch converters are the formats' own conversions, bit for bit those of xz.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [check](../xz-check.md) | the check of each block: none, CRC-32, CRC-64, SHA-256 |
| [filter](../xz-filter.md) | the branch converters: the processor the data's code is for |
| [options](../xz-options.md) | the level, `-e`, the check, the filters, the dictionary |
| [writer](../xz-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../xz-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one stream of one block (static) |
| [decompress](decompress.md) | every stream decompressed and checked, against the limits (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    vector<byte> packed = compress::xz::compress(text, {.check = compress::xz::check::sha256});
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::xz::decompress(packed);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
52 bytes into 116
52
```

## See also

- [lzma](../lzma/README.md): the levels and the parser
- [tar](../tar.md): `.tar.xz`
- [benchmarks](../benchmarks.md): against liblzma, and the converters' speed
- `tests/compress/xz.cpp`: liblzma as the oracle both ways, every converter against liblzma's byte for byte;
  `tests/compress/fuzz/xz_fuzz.cpp`
- [sgcl::compress](../README.md)
