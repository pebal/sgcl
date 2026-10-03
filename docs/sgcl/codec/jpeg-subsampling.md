[sgcl](../README.md) › [codec](README.md) › [jpeg](jpeg/README.md)

# sgcl::codec::jpeg::subsampling

```cpp
#include "sgcl/codec/jpeg.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class jpeg {
    public:
        enum class subsampling : uint8_t {
            s444,
            s422,
            s420
        };
    };
}
```

The resolution at which [encode](jpeg/encode.md) keeps the chrominance of a color image, the field
[options](jpeg-options.md)`::subsampling`, as `cjpeg -sample` sets it: the eye sees changes of color less sharply
than changes of brightness, so the two color components of YCbCr may be kept at half the resolution, averaged over
two or four pixels, for a smaller file. The luminance is always whole, and a gray image has no chrominance to
subsample. The default is `s420`; a value outside the three below is `invalid_argument` from `encode`.

| Value | Description |
|---|---|
| `s444` | 4:4:4, the chrominance whole |
| `s422` | 4:2:2, the chrominance halved across |
| `s420` | 4:2:0, the chrominance halved across and down |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // red that changes from column to column, blue from row to row
    codec::image stripes(64, 64, codec::pixel_format::rgb8);
    for (int y : range(64)) {
        slice<byte> row = stripes.row(y);
        for (int x : range(64)) {
            row[3 * x] = byte(x % 2 ? 255 : 0);
            row[3 * x + 2] = byte(y % 2 ? 255 : 0);
        }
    }
    using enum codec::jpeg::subsampling;
    for (codec::jpeg::subsampling s : {s444, s422, s420}) {
        codec::image back = codec::jpeg::decode(codec::jpeg::encode(stripes, {.subsampling = s}));
        println("red across: {} {}, blue down: {} {}", int(back.row(0)[0]), int(back.row(0)[3]),
                int(back.row(0)[2]), int(back.row(1)[2]));
    }
}
```

Output:

```text
red across: 0 250, blue down: 4 251
red across: 88 164, blue down: 0 212
red across: 74 150, blue down: 74 103
```

## See also

- [options](jpeg-options.md): where it is set
- [encode](jpeg/encode.md): what writes it
- [sgcl::codec::jpeg](jpeg/README.md)
