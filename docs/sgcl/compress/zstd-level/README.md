[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md)

# sgcl::compress::zstd::level

```cpp
#include "sgcl/compress/zstd.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class zstd {
    public:
        class level;
    };
}
```

`sgcl::compress::zstd::level` is how hard zstd's compressor works, on zstd's own scale: 1 to 19 as the command's
`-1` to `-19` (3 the default), 20 to 22 as `--ultra -20` to `-22` (a window of up to 128 MB, which a reader must hold),
and a negative level -N as `--fast=N`: the fastest finder stepping over more of the data after misses, the literals
left raw. An `int` converts to it, so the options take `{.level = 19}`; a value outside those is
`std::invalid_argument`, and in a constant expression an error at compile time. 0, which libzstd reads as 3, is not a
level here: the type has a default instead. It is not [compress::level](../level/README.md), whose 0 to 9 and default
6 are DEFLATE's and LZMA's: zstd's levels mean what zstd's mean.

## Rules

- The levels as the finders they are, with zstd's parameters for each: 1 and 2 one table probed two positions at a
  time; 3 and 4 a long table and a short one; 5 to 12 rows of the last positions under a hash, a tag of the hash
  beside each, tried 8 to 64 deep with none, one or two bytes of lookahead (greedy, lazy, lazy2; the greedy 5 takes
  the last offset again a byte on before it searches, as zstd does), the middle of a match past 384 bytes left out of
  the rows; 13 to 15 the same lookahead
  over a binary tree of the window; 16 to 22 an optimal parser pricing every way through a stretch from the
  statistics of the blocks before (19 to 22 parse the first block twice to start from real statistics).
- From 1 to 19 the output does not grow on text, and the time grows; the decoder's speed is about the same for every
  level, a little faster at the higher ones.
- A level is one `int`, trivially copyable.

## Member objects

| Constant | Description |
|---|---|
| `fastest` | -131072: `--fast=131072`, the least work |
| `standard` | 3: zstd's default |
| `smallest` | 19: the most without `--ultra` |
| `ultra` | 22: `--ultra -22`, a window of 128 MB |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zstd-level.md) | the default level, 3 (`level::standard`), or the one given |

#### Observers

| Function | Description |
|---|---|
| [value](value.md) | the level as an `int` |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same level |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(200);
    for (int l : {-10, 1, 9}) {
        println("level {}: {} bytes", l, compress::zstd::compress(text, {.level = l}).size());
    }
}
```

Output:

```text
level -10: 108 bytes
level 1: 56 bytes
level 9: 55 bytes
```

## See also

- [zstd::options](../zstd-options.md): where it is given
- [compress::level](../level/README.md): the level of DEFLATE and LZMA
- [sgcl::compress::zstd](../zstd/README.md)
