[sgcl](../README.md) › [compress](README.md) › [bzip2](bzip2/README.md)

# sgcl::compress::bzip2::options

```cpp
#include "sgcl/compress/bzip2.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class bzip2 {
    public:
        struct options {
            bzip2::level level;
        };
    };
}
```

`sgcl::compress::bzip2::options` is how a bzip2 stream is made: its [level](bzip2-level/README.md), the block size.
[compress](bzip2/compress.md) and the [writer](bzip2-writer/README.md) take it; the readers take none, the block
size coming from the stream's header.

## Rules

- An aggregate: `{.level = 1}`; the default is the `bzip2` command's, 9.
- There is no work factor, which libbz2 has for its sorting's fallback: the sorting here has no worst case to fall
  back from.

## Member objects

| Member | Description |
|---|---|
| `level` | the [level](bzip2-level/README.md): the block size in 100 KB; 9 by default |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("a line of a log, and another line of a log; ").repeat(20000);
    for (int level : {1, 5, 9}) {
        println("level {}: {} bytes", level, compress::bzip2::compress(text, {.level = level}).size());
    }
}
```

Output:

```text
level 1: 959 bytes
level 5: 258 bytes
level 9: 128 bytes
```

## See also

- [level](bzip2-level/README.md)
- [sgcl::compress::bzip2](bzip2/README.md)
