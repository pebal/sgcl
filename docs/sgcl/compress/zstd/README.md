[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::zstd

```cpp
#include "sgcl/compress/zstd.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zstd;
}
```

`sgcl::compress::zstd` is Zstandard (`.zst`, RFC 8878): the format of the `zstd` command and of HTTP's
`Content-Encoding: zstd`, which compresses about as well as `xz -6` at its higher levels and about as fast as LZ4 at
its lowest, and decodes at the same speed whatever the level. It is LZ77 with entropy coding: a block is a section of
literals, Huffman-coded, and one of sequences — each a literal length, a match length and an offset, the offset
possibly one of the three used last — coded by Finite State Entropy, a coder of arithmetic coding's precision at
Huffman's speed. A frame is a header (the window the decoder needs, the content's size, a dictionary's id), blocks of
up to 128 KB, and the XXH64 of the content. It is a class of static functions and the types of the format: a frame in
memory either way ([compress](compress.md), [decompress](decompress.md)), what a frame's header says
([content_size](content_size.md), [dictionary_id](dictionary_id.md)), and a stream each way, a
[writer](../zstd-writer/README.md) and a [reader](../zstd-reader/README.md). Go's standard library has no zstd.

**Levels** are zstd's own ([level](../zstd-level/README.md)): 1 to 19 as `zstd -1` to `-19` (3 the default), 20 to 22
as `--ultra`, a negative level as `--fast=N`. Every level makes the same format, decoded at the same speed.

## Rules

- **The frame's defaults are the `zstd` command's**: level 3, the content's checksum, the content's size when it is
  known ([options](../zstd-options.md)): [compress](compress.md) writes it, a [writer](../zstd-writer/README.md)
  cannot. [compress](compress.md) shrinks the window to the data; a writer announces the level's.
- **Frames one after another** are read as one, as `zstd -d` reads them; skippable frames (0x184D2A50 to 0x184D2A5F)
  are passed over. Anything else after a frame is `errc::invalid_header`.
- **Checks.** The content's checksum, when a frame carries it, is compared (`errc::checksum`); a content size in the
  header must be the size decoded. A table that is not valid, a block larger than 128 KB or than the window, a match
  before the data, a bitstream that does not end where it should are `errc::corrupt`; reserved bits set are
  `errc::invalid_header`.
- **A dictionary** ([dictionary](../zstd-dictionary/README.md)) is made once and given to any number of calls and
  streams: zstd's format (from `zstd --train`: an id, entropy tables, three offsets and content) or any other bytes
  as raw content. A frame made with one names its id in the header, and reading it without that dictionary is
  `errc::dictionary_required`.
- **Memory.** A frame's window is held against the [limits](../limits.md)' `max_memory` before anything is taken (1
  GiB by default; the `zstd` command's own bound is 128 MB); `decompress` has `max_size` as well, and a content size in
  the header past it fails before any work.
- Options out of range (a `window_log` outside 10 to 31) are the program's mistake: `compress` throws
  `std::invalid_argument`, and a writer's first write reports `errc::invalid_argument`. A level out of range is refused
  by [level](../zstd-level/README.md) itself.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [level](../zstd-level/README.md) | how hard the compressor works: 1 to 22, or a negative level for `--fast` |
| [dictionary](../zstd-dictionary/README.md) | a dictionary of zstd's format or raw content, parsed once and shared |
| [options](../zstd-options.md) | the level, the checksum, the content's size, the window, a dictionary |
| [writer](../zstd-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../zstd-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed into one frame (static) |
| [decompress](decompress.md) | every frame decompressed and checked, against the limits (static) |

#### Observers

| Function | Description |
|---|---|
| [content_size](content_size.md) | the content's size a frame's header gives, if it gives one (static) |
| [dictionary_id](dictionary_id.md) | the id of the dictionary a frame's header names (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words, the same words again. ").repeat(100);
    vector<byte> standard = compress::zstd::compress(text);
    vector<byte> small = compress::zstd::compress(text, {.level = 19});
    println("{} bytes into {} or {}", text.size(), standard.size(), small.size());

    auto back = compress::zstd::decompress(small);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
5400 bytes into 53 or 50
5400
```

## See also

- [zstd::writer](../zstd-writer/README.md), [zstd::reader](../zstd-reader/README.md): the streams
- [hash::xxh64](../../hash/xxh64/README.md): the checksum of the frame
- [benchmarks](../benchmarks.md): against libzstd
- `tests/compress/zstd.cpp`: libzstd as the oracle both ways, its trained dictionaries, frames written by hand from
  RFC 8878; `tests/compress/fuzz/zstd_fuzz.cpp`
- [sgcl::compress](../README.md)
