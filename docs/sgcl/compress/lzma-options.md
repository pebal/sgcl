[sgcl](../README.md) › [compress](README.md) › [lzma](lzma/README.md)

# sgcl::compress::lzma::options

```cpp
#include "sgcl/compress/lzma.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzma {
    public:
        struct options {
            compress::level level;
            bool extreme = false;
            uint32_t dictionary = 0;
            uint8_t lc = 3;
            uint8_t lp = 0;
            uint8_t pb = 2;
        };
    };
}
```

`sgcl::compress::lzma::options` is how LZMA data is made: the level and its mode, as xz's `-0` to `-9` and `-e`, a
dictionary of another size, and the literal and position bits of the header. [compress](lzma/compress.md) and the
[writer](lzma-writer/README.md) take it; the decoders read all of it from the header.

## Rules

- An aggregate: `{.level = 9}`, `{.extreme = true}`, the other fields at their defaults.
- The levels are xz's, with its dictionaries and its two modes:

| Level | Dictionary | Parser | Match finder |
|---|---|---|---|
| 0 | 256 KiB | fast | hash chain, 4 candidates |
| 1 | 1 MiB | fast | hash chain, 8 |
| 2 | 2 MiB | fast | hash chain, 24 |
| 3 | 4 MiB | fast | hash chain, 48 |
| 4 | 4 MiB | optimal | binary tree, nice length 16 |
| 5 | 8 MiB | optimal | binary tree, 32 |
| 6 (default) | 8 MiB | optimal | binary tree, 64 |
| 7, 8, 9 | 16, 32, 64 MiB | optimal | binary tree, 64 |

- The fast parser takes the longest match found, a repeat of a recent distance when it is nearly as long, and a
  literal instead when the next position has a better match. The optimal parser prices every way of coding the next
  few thousand bytes (literals, matches, repeats of the four last distances, and a match followed by a literal and
  the same distance again) against the coder's current probabilities, and takes the cheapest. `extreme` searches
  deeper (nice length 273, 512 candidates; 192 at levels 3 and 5), as xz's `-e` does. Level 0 is not "stored" here:
  it is the fastest LZMA.
- Out of range — lc past 8, lp or pb past 4, a dictionary under 4 KiB or past 1.5 GiB, `level::huffman_only` (which
  is DEFLATE's) — is the program's mistake: `compress` throws `std::invalid_argument`, and a writer's first write
  reports `errc::invalid_argument`.

## Member objects

| Member | Description |
|---|---|
| `level` | 0 to 9 as xz `-0` to `-9`; 6 by default ([level](level/README.md)) |
| `extreme` | the deeper search of xz's `-e`; `false` by default |
| `dictionary` | the dictionary in bytes, 4 KiB to 1.5 GiB; 0, by default, is the level's |
| `lc` | the literal context bits, 0 to 8; 3 by default |
| `lp` | the literal position bits, 0 to 4; 0 by default |
| `pb` | the position bits, 0 to 4; 2 by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // 100 000 words drawn from a list of 16, a fixed sequence
    const char* words[] = {"alpha ", "beta ", "gamma ", "delta ", "epsilon ", "zeta ", "eta ",
                           "theta ", "iota ", "kappa ", "lambda ", "mu ", "nu ", "xi ", "omicron ",
                           "pi "};
    vector<byte> text;
    uint32_t x = 1;
    for (int i : range(100000)) {
        x = x * 1103515245 + 12345;
        for (const char* c = words[(x >> 16) % 16]; *c; ++c) {
            text.push_back(byte(*c));
        }
    }
    println("{} bytes", text.size());
    for (int level : {0, 3, 6}) {
        println("level {}: {}", level, compress::lzma::compress(text, {.level = level}).size());
    }
}
```

Output:

```text
525677 bytes
level 0: 109387
level 3: 90937
level 6: 71284
```

## See also

- [xz::options](xz-options.md): the same levels
- [level](level/README.md)
- [sgcl::compress::lzma](lzma/README.md)
