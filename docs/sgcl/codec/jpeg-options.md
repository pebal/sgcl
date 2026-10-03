[sgcl](../README.md) › [codec](README.md) › [jpeg](jpeg/README.md)

# sgcl::codec::jpeg::options

```cpp
#include "sgcl/codec/jpeg.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class jpeg {
    public:
        struct options {
            int quality = 85;
            jpeg::subsampling subsampling = jpeg::subsampling::s420;
            bool optimize = false;
        };
    };
}
```

`sgcl::codec::jpeg::options` is what [encode](jpeg/encode.md) is told, a plain struct written in place:
`{.quality = 90}`, `{.subsampling = codec::jpeg::subsampling::s444, .optimize = true}`. The quality is the IJG's
scale, as `cjpeg -quality` takes it: the quantization tables of Annex K scaled by 5000 / quality percent below 50
and by 200 − 2 × quality percent from there, so that 50 is the tables as printed and 100 all ones; each entry is
kept within 1 to 255, as a baseline file's 8-bit tables are. A quality outside 1 to 100, or a subsampling outside
the list of [subsampling](jpeg-subsampling.md), is `invalid_argument` from `encode`.

## Member objects

| Member | Description |
|---|---|
| `quality` | 1 to 100 on the IJG's scale, higher the finer and the larger; 85 unless told |
| `subsampling` | the resolution of the chrominance ([subsampling](jpeg-subsampling.md)); `s420`, half both ways, unless told |
| `optimize` | Huffman tables made for the image's own counts in place of the typical ones of Annex K, as `cjpeg -optimize`: a smaller file for a second pass over the image; `false` unless told |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image photo(128, 128, codec::pixel_format::rgb8);
    for (int y : range(128)) {
        slice<byte> row = photo.row(y);
        for (int x : range(128)) {
            row[3 * x] = byte(x * 2);
            row[3 * x + 1] = byte((x ^ y) * 2);
            row[3 * x + 2] = byte(y * 2);
        }
    }
    println("the defaults: {} bytes", codec::jpeg::encode(photo)->size());
    for (int quality : {10, 50, 85, 100}) {
        vector<byte> file = codec::jpeg::encode(photo, {.quality = quality});
        vector<byte> optimized = codec::jpeg::encode(photo, {.quality = quality, .optimize = true});
        println("quality {}: {} bytes, optimized {}", quality, file.size(), optimized.size());
    }
}
```

Output:

```text
the defaults: 2824 bytes
quality 10: 970 bytes, optimized 503
quality 50: 1563 bytes, optimized 1008
quality 85: 2824 bytes, optimized 1812
quality 100: 11284 bytes, optimized 8305
```

## See also

- [encode](jpeg/encode.md): what takes the options
- [subsampling](jpeg-subsampling.md): the resolution of the chrominance
- [save_options](save_options.md): the quality and the subsampling `save` writes a JPEG at
- [sgcl::codec::jpeg](jpeg/README.md)
