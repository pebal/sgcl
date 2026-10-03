[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::level

```cpp
#include "sgcl/compress/level.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class level;
}
```

`sgcl::compress::level` is how hard a compressor works, as zlib and Go count it: 0 stores the data as it is, 1 is the
fastest, 9 the smallest, 6 the default of every format of the module (and of zlib and Go); `huffman_only` codes the
bytes with no search for repeats (Go's `HuffmanOnly`), for data that has none worth the search, such as the residuals
of an image filter. An `int` converts to it, so the options of a format take `{.level = 9}`; a value outside those is
`std::invalid_argument`, and in a constant expression an error at compile time.

The level is read by the format it is given to. DEFLATE ([flate](flate.md), [zlib](zlib.md), [gzip](gzip.md), a
zip entry, 7z's Deflate) has two encoders, described below. [lzma](lzma.md) and [xz](xz.md) take 0 to 9 as xz's
`-0` to `-9`, with their dictionaries and parsers, and refuse `huffman_only`, which is DEFLATE's.

## Rules

- **DEFLATE's levels are two encoders: levels 1–6 as Go's fast encoder, 7–9 zlib's chains; Go's 6 is this 6.**
  Levels 1 to 6 look each position up in tables of the latest position under a hash of four bytes (and of seven
  from level 3), with no chains; the default, 6, is as fast as Go's default and a little smaller. Levels 7 to 9
  walk chains of earlier positions under a hash of three bytes with lazy matching, zlib's own 7 to 9: zlib's size,
  at zlib's pace. The default is a few per cent larger than zlib's 6, more on binary data (object files) than on
  text, at several times its speed ([benchmarks](benchmarks.md)), so an archive of binaries that should be small
  asks for 7 to 9.

| Level | Encoder | Hashes | A position looks at | Lazy | Positions inside a match |
|---|---|---|---|---|---|
| 0 | stored blocks, straight from the input | — | — | — | — |
| 1 | table | 4 bytes, 2^14 | 1 candidate; a run of misses skipped faster | no | every 4th |
| 2 | table | 4 bytes, 2^15 | 1 candidate; misses skipped faster | no | every 2nd |
| 3 | table | 4 bytes 2^15, 7 bytes 2^15 | 2 candidates | no | every 2nd |
| 4 | table | 4 bytes 2^16, 7 bytes 2^15 | 2 candidates | the next position | all |
| 5 | table | 4 bytes 2^16, 7 bytes 2^16 | 2 candidates | the next position | all |
| 6 (default) | table | 4 bytes 2^16, 7 bytes 2^16 (2 a bucket) | 3 candidates | the next position | all |
| 7 | chains | 3 bytes, 2^16 | 256 of the chain (64 past a match of 8) | up to 32 | all |
| 8 | chains | 3 bytes, 2^17 | 1024 (256 past 32) | up to 128 | all |
| 9 | chains | 3 bytes, 2^17 | 4096 (1024 past 32) | up to 258 | all |

- From 1 to 9 the output does not grow and the time does not shrink (a test holds the order on text, PNG's
  filtered rows and binary data).
- Data an image filter left is compressed at 7 and up with zlib's filtered strategy, which the table levels have no
  need of: PNG's encoder writes at 7 unless asked.
- Level 0 of DEFLATE stores the data in blocks as it is; level 0 of LZMA is not "stored", it is the fastest LZMA.
- A level is one `int`, trivially copyable.

## Member objects

| Constant | Description |
|---|---|
| `store` | 0: the data stored as it is |
| `fastest` | 1 |
| `standard` | 6: the default |
| `smallest` | 9 |
| `huffman_only` | -2: the bytes coded with no search for repeats (DEFLATE only) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](level/level.md) | the default level, 6, or the one given |

#### Observers

| Function | Description |
|---|---|
| [value](level/value.md) | the level as an `int` |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](level/operator_cmp.md) | the same level |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(2000);
    println("{} bytes", text.size());
    for (int n : {0, 1, 6, 9, compress::level::huffman_only}) {
        println("level {}: {}", n, compress::flate::compress(text, {.level = n}).size());
    }
}
```

Output:

```text
96000 bytes
level 0: 96010
level 1: 987
level 6: 331
level 9: 331
level -2: 45648
```

## See also

- [flate](flate.md), [zlib](zlib.md), [gzip](gzip.md): DEFLATE
- [lzma](lzma.md), [xz](xz.md): xz's levels
- [benchmarks](benchmarks.md): the levels against zlib and Go
- [sgcl::compress](README.md)
