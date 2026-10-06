[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::bmp

```cpp
#include "sgcl/codec/bmp.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class bmp;
}
```

`sgcl::codec::bmp` reads and writes BMP, Windows' bitmap: every header from OS/2's and BITMAPCOREHEADER to V5,
1, 2, 4 and 8 bits through a palette, 16, 24 and 32 bits, bit fields, RLE4 and RLE8, rows bottom-up or top-down.
[decode](decode.md) gives the image of a file, [encode](encode.md) writes any [image](../image/README.md). Every
member is static. [codec::decode](../decode.md) reads BMP too, told by `BM` and a header's size, and
[save](../save.md) writes it for a path ending in `.bmp`. Where Go's `x/image/bmp` reads 8, 24 and 32 bits without
compression, the module reads every depth and RLE.

## Rules

- **Alpha as Chromium and Go read it.** From an alpha mask (V3 and later headers, or BI_ALPHABITFIELDS); for 32
  bits without masks only under a V4 or V5 header; any other 32-bit pixel is opaque. Pixels an RLE stream skips (its
  delta and end-of-line codes) are transparent black, so an RLE image comes as `rgba8`.
- **Channels of fewer than 8 bits** are widened by repeating their bits (5 bits abcde as abcdeabc), as ffmpeg and
  Chromium widen them. A palette of grays gives `gray8`.
- **Encoding** writes `gray8` as 8 bits through a palette of the 256 grays, an image with alpha as 32 bits under a
  V4 header with its masks, any other as 24 bits; 16-bit channels are narrowed to 8, CMYK written as RGB.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md). JPEG and
  PNG inside a BMP are `errc::unsupported`.

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the image of a BMP file, from its bytes or a stream (static) |
| [encode](encode.md) | a BMP of an image, as bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(5, 3, codec::pixel_format::rgb8);
    picture.row(1)[3] = byte(255);
    vector<byte> file = codec::bmp::encode(picture);
    codec::image back = codec::bmp::decode(file);
    println("{} bytes: 54 of headers, 3 rows of 16; the same pixels: {}", file.size(),
            back.pixels() == picture.pixels());
}
```

Output:

```text
102 bytes: 54 of headers, 3 rows of 16; the same pixels: true
```

## See also

- [ico](../ico/README.md): icons, whose entries are BMPs or PNGs
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [image](../image/README.md), [error](../error/README.md)
