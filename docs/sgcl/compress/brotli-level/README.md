[sgcl](../../README.md) › [compress](../README.md) › [brotli](../brotli/README.md)

# sgcl::compress::brotli::level

```cpp
#include "sgcl/compress/brotli.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class brotli {
    public:
        class level;
    };
}
```

`sgcl::compress::brotli::level` is how hard brotli's compressor works, on brotli's own scale of qualities: 0 to 11 as
the command's `-q 0` to `-q 11`, 11 the default of the command, of libbrotli and of Python's module (HTTP servers that
compress as they answer use 4 to 6). An `int` converts to it, so the options take `{.level = 5}`; a value outside
0 to 11 is `std::invalid_argument`, and in a constant expression an error at compile time. It is not
[compress::level](../level/README.md), whose 0 to 9 and default 6 are DEFLATE's and LZMA's.

## Rules

- The qualities as the finders they are: 0 and 1 one table probed two positions at a time; 2 and 3 a long table and a
  short one; 4 to 9 rows of the last positions under a hash, 4 with a greedy parse that takes the last distance again
  a byte on before it searches, 5 to 9 with a lazy parse; from 5 the literals of a meta-block of 8 KB and more coded
  by the context of the two bytes before them (their 64 contexts clustered into 16 trees, 64 from 7), a smaller one
  (a flush of a live response) without, which there costs bytes and time; 10 and 11 the optimal parser over a
  binary tree.
- From 0 to 11 the output does not grow on text, and the time grows; the decoder's speed is about the same for every
  quality.
- A level is one `int`, trivially copyable.

## Member objects

| Constant | Description |
|---|---|
| `fastest` | 0: `-q 0`, the least work |
| `standard` | 11: the brotli command's default |
| `smallest` | 11: `-q 11`, the most work |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](brotli-level.md) | the default level, 11 (`level::standard`), or the one given |

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
    for (int l : {0, 5, 11}) {
        println("level {}: {} bytes", l, compress::brotli::compress(text, {.level = l}).size());
    }
}
```

Output:

```text
level 0: 49 bytes
level 5: 44 bytes
level 11: 44 bytes
```

## See also

- [brotli::options](../brotli-options.md): where it is given
- [compress::level](../level/README.md): the level of DEFLATE and LZMA
- [sgcl::compress::brotli](../brotli/README.md)
