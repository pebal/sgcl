[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::qoi

```cpp
#include "sgcl/codec/qoi.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class qoi;
}
```

`sgcl::codec::qoi` reads and writes QOI, the Quite OK Image format of 2022: lossless RGB and RGBA coded in one pass,
each pixel an index into the 64 colors seen last, a small difference from the pixel before, a run of it, or the
color itself. [decode](decode.md) gives the image of a file, [encode](encode.md) writes any
[image](../image/README.md). Every member is static. [codec::decode](../decode.md) reads QOI too, told by its
signature `qoif`, and [save](../save.md) writes it for a path ending in `.qoi`. Go and C have it only in libraries
of their own; the module's coder is written from the specification.

## Rules

- **Lossless at 8 bits.** An image of an alpha format becomes 4 channels, any other 3; 16-bit channels are narrowed
  to 8, gray written as RGB, CMYK through RGB ([convert](../image/convert.md)'s rules).
- **Decoding** gives `rgb8` for 3 channels and `rgba8` for 4, or the format of
  [decode_options](../decode_options.md)`::want`. The end marker is not required, as the reference decoder does not
  look at it; data that ends before the last pixel is `errc::unexpected_end`.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md).

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the image of a QOI file, from its bytes or a stream (static) |
| [encode](encode.md) | a QOI of an image, as bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 32, codec::pixel_format::rgba8);
    for (int y : range(32)) {
        for (int x : range(64)) {
            picture.row(y)[4 * x] = byte(x * 4);
            picture.row(y)[4 * x + 3] = byte(255);
        }
    }
    vector<byte> file = codec::qoi::encode(picture);
    codec::image back = codec::qoi::decode(file);
    println("{} bytes, the same pixels: {}", file.size(), back.pixels() == picture.pixels());
}
```

Output:

```text
4117 bytes, the same pixels: true
```

## See also

- [png](../png/README.md): lossless and smaller, slower
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [image](../image/README.md), [error](../error/README.md)
