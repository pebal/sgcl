[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md)

# sgcl::compress::bzip2::level

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2 {
    public:
        class level;
    };
}
```

`sgcl::compress::bzip2::level` is bzip2's level: the block size in 100 KB, 1 to 9 as the command's `-1` to `-9`, 9 the
default of the command and of libbz2. The work for a byte hardly changes with it; a larger block finds more of the
data's repeats, and a reader needs the block's size and a few times more in memory. An `int` converts to it, so the
options take `{.level = 1}`; a value outside 1 to 9 is `std::invalid_argument`, and in a constant expression an error
at compile time. It is not [compress::level](../level/README.md), whose 0 to 9 and default 6 are DEFLATE's and
LZMA's.

## Rules

- A block is up to 100 KB × level of the data after the first run-length pass (19 bytes less), the stream's header
  names the level, and a reader keeps a block of that size.
- From 1 to 9 the output does not grow on text; the time stays about the same.
- A level is one `int`, trivially copyable.

## Member objects

| Constant | Description |
|---|---|
| `fastest` | 1: blocks of 100 KB |
| `standard` | 9: the bzip2 command's default |
| `smallest` | 9: blocks of 900 KB |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bzip2-level.md) | the default level, 9 (`level::standard`), or the one given |

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
    for (int l : {1, 5, 9}) {
        println("level {}: {} bytes", l, compress::bzip2::compress(text, {.level = l}).size());
    }
}
```

Output:

```text
level 1: 104 bytes
level 5: 104 bytes
level 9: 104 bytes
```

## See also

- [bzip2::options](../bzip2-options.md): where it is given
- [compress::level](../level/README.md): the level of DEFLATE and LZMA
- [sgcl::compress::bzip2](../bzip2/README.md)
