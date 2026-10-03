[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::convert

```cpp
image convert(pixel_format f) const;
```

A new image of the same pixels in the format `f`, with the metadata of this one ([exif](exif.md), [icc](icc.md),
[orientation](orientation.md)). It is always a copy, even to the same format. Each pixel converts by these rules:

- Between depths a channel goes up as `v` × 257 (255 becomes 65535) and down to the nearest value,
  round(`v` / 257).
- Color to gray is the luma of Rec. 601, 0.299 red, 0.587 green and 0.114 blue, in 16-bit fixed point. A gray
  pixel keeps its value through color and back.
- A format without alpha drops it; one with alpha gets it opaque where the source had none.
- CMYK and RGB convert through each other with no color profile: each color channel is (1 − c)(1 − k) of the
  white, and the way back takes black as what the brightest channel lacks.

A decoder asked for a format by [decode_options](../decode_options.md)`.want` converts each row by the same rules as
it decodes it, with no second pass over the image.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the [pixel format](../pixel_format.md) of the new image |

## Return value

The new image, of the same sides.

## Complexity

Linear in the number of pixels.

## Exceptions

`invalid_argument` when `f` is outside the list of [pixel_format](../pixel_format.md).

## Notes

Going down to 8 bits rounds as libpng's `png_set_scale_16` does. Go and libpng's `png_set_strip_16` truncate,
`v >> 8`, so an image taken down to 8 bits here can differ from Go's by one level.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"
#include <cstring>

using namespace sgcl;

int main() {
    codec::image deep(1, 1, codec::pixel_format::gray16);
    uint16_t level = 511;
    std::memcpy(deep.pixels().data(), &level, 2);
    codec::image shallow = deep.convert(codec::pixel_format::gray8);
    println("511 at 8 bits: {}", std::to_integer<int>(shallow.pixels()[0]));

    codec::image red(1, 1, codec::pixel_format::rgb8);
    red.pixels()[0] = byte(255);
    codec::image gray = red.convert(codec::pixel_format::gray8);
    codec::image rgba = red.convert(codec::pixel_format::rgba8);
    println("luma {}, alpha {}", std::to_integer<int>(gray.pixels()[0]),
            std::to_integer<int>(rgba.pixels()[3]));
    codec::image same = red.convert(codec::pixel_format::rgb8);
    println("a copy: {}", same.pixels().data() != red.pixels().data());
}
```

Output:

```text
511 at 8 bits: 2
luma 76, alpha 255
a copy: true
```

## See also

- [pixel_format](../pixel_format.md): the formats
- [decode_options](../decode_options.md): the format a decoder makes
- [clone](clone.md): a copy in the same format
- [sgcl::codec::image](../image.md)
