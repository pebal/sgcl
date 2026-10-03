[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::pixel_format

```cpp
#include "sgcl/codec/image.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    enum class pixel_format : uint8_t {
        gray8,
        gray_alpha8,
        rgb8,
        rgba8,
        gray16,
        gray_alpha16,
        rgb16,
        rgba16,
        cmyk8
    };
}
```

How the pixels of an [image](image.md) lie in its rows: the channels in the order of the name, each a byte (`…8`)
or a 16-bit value (`…16`), with no padding between the pixels or at the end of a row. Where Go has a type per
layout (`image.Gray`, `image.NRGBA64`, `image.CMYK`), the module has one `image` and this value.

Alpha is straight, not premultiplied, as PNG, GIF and WebP store it: a pixel's color is what it is, whatever its
alpha. A 16-bit channel is a `uint16_t` in the byte order of the machine, so a program reads it with a plain load:
PNG's decoder turns the file's big-endian samples around, and its encoder turns them back. `cmyk8` is what an Adobe
CMYK or YCCK JPEG holds: the ink of each channel, 0 for none and 255 for full, as Go's `image.CMYK` has it, so a
`cmyk8` image of all-zero pixels is white.

An image goes from one format to another by [image::convert](image/convert.md), or as it is decoded, by
[decode_options](decode_options.md)`.want`. A value outside the list, `pixel_format(9)`, is refused: the
[image](image/image.md) constructor and [convert](image/convert.md) throw `invalid_argument`, a decoder asked for it
returns [errc](errc.md)`::invalid_argument`.

| Value | Description |
|---|---|
| `gray8` | gray, a byte: 1 byte a pixel |
| `gray_alpha8` | gray and alpha, a byte each: 2 bytes a pixel |
| `rgb8` | red, green and blue, a byte each: 3 bytes a pixel |
| `rgba8` | red, green, blue and alpha, a byte each: 4 bytes a pixel |
| `gray16` | gray, 16 bits: 2 bytes a pixel |
| `gray_alpha16` | gray and alpha, 16 bits each: 4 bytes a pixel |
| `rgb16` | red, green and blue, 16 bits each: 6 bytes a pixel |
| `rgba16` | red, green, blue and alpha, 16 bits each: 8 bytes a pixel |
| `cmyk8` | the ink of cyan, magenta, yellow and black, a byte each: 4 bytes a pixel |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstring>

using namespace sgcl;

int main() {
    codec::image deep(1, 1, codec::pixel_format::gray16);
    uint16_t level = 4660;
    std::memcpy(deep.pixels().data(), &level, 2);
    codec::image read = codec::png::decode(codec::png::encode(deep));
    uint16_t back = 0;
    std::memcpy(&back, read.pixels().data(), 2);
    println("{} after PNG, stride {}", back, read.stride());

    codec::image red(1, 1, codec::pixel_format::rgb8);
    red.pixels()[0] = byte(255);
    vector<int> ink;
    for (byte b : red.convert(codec::pixel_format::cmyk8).pixels()) {
        ink.push_back(std::to_integer<int>(b));
    }
    println("red as ink: {}", ink);
}
```

Output:

```text
4660 after PNG, stride 2
red as ink: [0, 255, 255, 0]
```

## See also

- [image](image.md): the pixels
- [image::convert](image/convert.md): from one format to another
- [decode_options](decode_options.md): the format a decoder makes
- [sgcl::codec](README.md)
