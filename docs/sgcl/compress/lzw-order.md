[sgcl](../README.md) › [compress](README.md) › [lzw](lzw.md)

# sgcl::compress::lzw::order

```cpp
#include "sgcl/compress/lzw.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class lzw {
    public:
        enum class order : uint8_t {
            lsb,
            msb
        };
    };
}
```

The order of the bits of the codes in the bytes: a value of the data's format, which every function and stream of
[lzw](lzw.md) takes with the literal width, as Go's `lzw.Order`. The two orders make different bytes of the same
codes, and data read in the other order decodes to nothing valid.

| Value | Description |
|---|---|
| `lsb` | least significant bit first: GIF |
| `msb` | most significant bit first: TIFF and PDF |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> pixels(64, byte(3));
    auto gif = compress::lzw::compress(pixels, compress::lzw::order::lsb, 2);
    auto tiff = compress::lzw::compress(pixels, compress::lzw::order::msb, 2);
    println("{} {} {}", gif.size(), tiff.size(), gif == tiff);
    println("{}", compress::lzw::decompress(gif, compress::lzw::order::lsb, 2)->size());
}
```

Output:

```text
7 7 false
64
```

## See also

- [sgcl::compress::lzw](lzw.md)
