[sgcl](../../README.md) › [compress](../README.md)

# sgcl::compress::lzma

```cpp
#include "sgcl/compress/lzma.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzma;
}
```

`sgcl::compress::lzma` is LZMA in its first file format, "LZMA alone": the `.lzma` files of `xz --format=lzma`,
`lzma` and the LZMA SDK. A header of 13 bytes — lc, lp and pb in one byte, the dictionary's size, the size
decompressed or all ones when it is not known — then the range coder's data, ended by a marker when the size is not
known. It is a class of static functions and the types of the format: the whole of the data in memory either way
([compress](compress.md), [decompress](decompress.md)), or a stream each way, a [writer](../lzma-writer/README.md)
and a [reader](../lzma-reader/README.md).

LZMA finds repeats as far back as its dictionary reaches (8 MiB at the default level, against DEFLATE's 32 KB) and codes
every bit against probabilities it learns as it goes: it makes a third less than [gzip](../gzip/README.md) at level 9, for several
times gzip's work both ways ([benchmarks](../benchmarks.md)). The levels are xz's `-0` to `-9` and `-e`, with its
dictionaries and its two parsers ([options](../lzma-options.md)). The `.xz` container ([xz](../xz/README.md)), LZMA2 and 7z
([sevenzip](../sevenzip.md)) are built on it.

## Rules

- [compress](compress.md) knows the size and writes it into the header with no end marker, as the LZMA SDK
  does; a [writer](../lzma-writer/README.md) does not, and writes all ones and the marker, as xz does.
  [decompress](decompress.md) and the [reader](../lzma-reader/README.md) take both, and the size known followed by the
  marker as well. Every decoder of the format reads either.
- The output is valid LZMA that any decoder reads, not byte for byte what xz makes: the format leaves the encoder its
  choices. On the text and the program measured its size is within 0.3 % of xz's at every level but 0, where it is
  smaller ([benchmarks](../benchmarks.md)).
- `.lzma` has no checksum. A decoder finds data the format does not allow — a distance before the start, a marker
  before the size in the header, a range coder that does not end at zero, data cut short (`errc::corrupt`,
  `errc::unexpected_end`) — but a flipped bit may also decode to other bytes without an error. Data that must
  arrive intact goes in a container with a check (`.xz`, 7z, or a hash of its own).
- **Memory.** The decoder's dictionary is as large as the header asks, or as the data when the header gives its
  size, and a stream's reader allocates it; the [limits](../limits.md)' `max_memory` (1 GiB unless told otherwise) is
  checked against it before anything is taken — more is `errc::too_large` at offset 0, in memory and as a stream
  alike. `decompress` has `max_size` as well, and a size in the header past it fails before any work. The encoder's
  tables take about 10 times its dictionary at levels 4 to 9 (the binary tree: 80 MiB at level 6, 576 MiB at level
  9) and 6 times at levels 0 to 3; a writer adds a window of about 1.25 times the dictionary. `compress` sizes the
  tables to the data when it is smaller than the dictionary, and codes the data where it lies.
- Options out of range (lc past 8, lp or pb past 4, a dictionary under 4 KiB or past 1.5 GiB,
  `level::huffman_only`) are the program's mistake: `compress` throws `std::invalid_argument`, and a writer's first
  write reports `errc::invalid_argument`.
- Nothing after the LZMA data is read by `decompress`; the reader reads its input 64 KB at a time, so it may take
  bytes past the end from its source.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](../error/README.md) |
| [options](../lzma-options.md) | the level, `-e`, the dictionary, the literal and position bits |
| [writer](../lzma-writer/README.md) | an io writer: what is written, compressed into another writer |
| [reader](../lzma-reader/README.md) | an io reader: what the data read from another reader decompresses to |

## Member objects

| Constant | Description |
|---|---|
| `HeaderSize` | 13: the bytes of the header, `static constexpr size_t` |

## Member functions

#### Operations

| Function | Description |
|---|---|
| [compress](compress.md) | the whole of the data compressed, its size in the header (static) |
| [decompress](decompress.md) | the whole of the data decompressed, checked against the limits (static) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    vector<byte> packed = compress::lzma::compress(text, {.level = 9});
    println("{} bytes into {}", text.size(), packed.size());

    auto back = compress::lzma::decompress(packed);  // data from outside: checked
    if (!back) {
        println("{}", back.error().message());
        return 1;
    }
    println("{}", back->size());
}
```

Output:

```text
52 bytes into 43
52
```

## See also

- [xz](../xz/README.md): LZMA2 in a container with a check
- [gzip](../gzip/README.md): for what must be fast or read by browsers
- [benchmarks](../benchmarks.md): against liblzma
- `tests/compress/lzma.cpp`: liblzma as the oracle both ways; `tests/compress/fuzz/lzma_fuzz.cpp`
- [sgcl::compress](../README.md)
