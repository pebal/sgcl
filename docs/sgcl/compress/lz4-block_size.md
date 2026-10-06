[sgcl](../README.md) › [compress](README.md) › [lz4](lz4/README.md)

# sgcl::compress::lz4::block_size

```cpp
#include "sgcl/compress/lz4.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lz4 {
    public:
        enum class block_size : uint8_t {
            kb64 = 4,
            kb256 = 5,
            mb1 = 6,
            mb4 = 7
        };
    };
}
```

`sgcl::compress::lz4::block_size` is the largest block of an LZ4 frame, the value of its header's BD byte: what a
writer gathers before it compresses, and what a reader must hold at once (a reader refuses a frame whose block size
is past the limits' `max_memory`). Larger blocks compress a little better with independent blocks and cost the reader
more memory; a stream that must be decodable soon after each write flushes rather than shrinking its blocks.

| Value | Description |
|---|---|
| `kb64` | 64 KB, liblz4's default |
| `kb256` | 256 KB |
| `mb1` | 1 MB |
| `mb4` | 4 MB, the `lz4` command's default, and the default here |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = string("the same words, the same words again and again. ").repeat(5000);
    for (auto bs : {compress::lz4::block_size::kb64, compress::lz4::block_size::mb4}) {
        println("{}", compress::lz4::compress(text, {.block_size = bs}).size());
    }
}
```

Output:

```text
1168
1014
```

## See also

- [options](lz4-options.md)
- [sgcl::compress::lz4](lz4/README.md)
