[sgcl](../README.md) › [codec](README.md) › [tiff](tiff/README.md)

# sgcl::codec::tiff::compression

```cpp
#include "sgcl/codec/tiff.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class tiff {
    public:
        enum class compression : uint8_t { none, lzw, deflate };
    };
}
```

`sgcl::codec::tiff::compression` names the compression [encode](tiff/encode.md) writes a TIFF's strips with, in
its [options](tiff-options.md). [decode](tiff/decode.md) reads these and PackBits and JPEG besides.

| Value | Description |
|---|---|
| `none` | the samples as they are (compression 1) |
| `lzw` | TIFF's LZW with the horizontal predictor (5); the default, read by every TIFF reader |
| `deflate` | zlib's DEFLATE with the horizontal predictor (8), smaller and slower |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 64, codec::pixel_format::gray8);
    for (codec::tiff::compression c : {codec::tiff::compression::none,
            codec::tiff::compression::lzw,
                                       codec::tiff::compression::deflate}) {
        vector<byte> file = codec::tiff::encode(picture, {.compression = c});
        println("{} bytes", file.size());
    }
}
```

Output:

```text
4242 bytes
264 bytes
184 bytes
```

## See also

- [options](tiff-options.md): what holds the compression
- [encode](tiff/encode.md): what writes it
- [sgcl::codec::tiff](tiff/README.md)
