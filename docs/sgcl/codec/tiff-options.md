[sgcl](../README.md) › [codec](README.md) › [tiff](tiff/README.md)

# sgcl::codec::tiff::options

```cpp
#include "sgcl/codec/tiff.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class tiff {
    public:
        struct options {
            tiff::compression compression = tiff::compression::lzw;
        };
    };
}
```

`sgcl::codec::tiff::options` is what [encode](tiff/encode.md) is told: the [compression](tiff-compression.md) of
the strips, a plain struct written in place as `{.compression = codec::tiff::compression::deflate}`. LZW is the
default for what reads it: every TIFF reader since 1988, where Deflate came with TIFF's technical notes of 1995 and
some older programs lack it. [save](save.md) writes `.tif` and `.tiff` with LZW.

## Member objects

| Member | Description |
|---|---|
| `compression` | the compression of the strips; `compression::lzw` unless told |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 64, codec::pixel_format::rgb16);
    vector<byte> file = codec::tiff::encode(picture,
            {.compression = codec::tiff::compression::deflate});
    println("{} bytes; a page: {}", file.size(), codec::tiff::decode_all(file)->size());
}
```

Output:

```text
210 bytes; a page: 1
```

## See also

- [compression](tiff-compression.md): the compressions
- [encode](tiff/encode.md): what takes the options
- [sgcl::codec::tiff](tiff/README.md)
