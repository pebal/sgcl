[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md)

# sgcl::compress::lz4::level

```cpp
#include "sgcl/compress/lz4.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lz4 {
    public:
        class level;
    };
}
```

`sgcl::compress::lz4::level` is how hard LZ4's compressor works, on lz4's own scale: 1 is the fast compressor (one
probe a position, the default of `lz4` and liblz4), 2 probes two tables a position, 3 to 9 walk hash chains (lz4hc),
10 to 12 price every way to code a stretch of 4 KB and take the cheapest (the optimal parser), and a negative level -N
is the fast compressor stepping
over more of the data after misses (`lz4 --fast=N`): faster, larger. An `int` converts to it, so the options take
`{.level = 9}`; a value outside those is `std::invalid_argument`, and in a constant expression an error at compile
time. It is not [compress::level](../level/README.md), whose 0 to 9 and default 6 are DEFLATE's and LZMA's: LZ4's
levels mean what lz4's mean.

## Rules

- The levels as the compressors they are: 1 one probe a position in a table of 4096; 2 two tables, of the last
  position under a hash of eight bytes and of four; 3 to 9 chains of the last 64 KB walked 2^(level-1) deep, a match
  looked for again near the end of each so that the next may begin inside it; 10, 11 and 12 the optimal parser with
  chains walked 96, 512 and 16384 deep.
- From 1 to 12 the output does not grow on text, and the time grows; the decoder's speed is the same for every level.
- A level is one `int`, trivially copyable.

## Member objects

| Constant | Description |
|---|---|
| `fastest` | -65537: the fast compressor with the most acceleration |
| `standard` | 1: the fast compressor, the default |
| `high` | 9: lz4hc's default |
| `smallest` | 12: the optimal parser at its fullest |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](lz4-level.md) | the default level, 1 (`level::standard`), or the one given |

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
        println("level {}: {} bytes", l, compress::lz4::compress(text, {.level = l}).size());
    }
}
```

Output:

```text
level -10: 173 bytes
level 1: 111 bytes
level 9: 109 bytes
```

## See also

- [lz4::options](../lz4-options.md): where it is given
- [compress::level](../level/README.md): the level of DEFLATE and LZMA
- [sgcl::compress::lz4](../lz4/README.md)
