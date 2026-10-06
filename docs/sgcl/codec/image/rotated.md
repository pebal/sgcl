[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::rotated

```cpp
image rotated(int degrees) const;
```

A new image turned clockwise by `degrees`, a multiple of 90: a negative angle turns counterclockwise, and 0, 360 or
any whole turn gives a copy. For a quarter turn the [width](width.md) and the [height](height.md) are swapped. The
[format](format.md) and the metadata go with the pixels, the [orientation](orientation.md) as it was (the program
turned the pixels on purpose; [oriented](oriented.md) applies the stored orientation instead). The image `rotated` is
called on keeps its pixels. Turning by other angles, with resampling, is not the module's: it leaves pixels as they
are.

## Parameters

| Parameter | Description |
|---|---|
| `degrees` | the angle, clockwise: 90, 180, 270, −90 and so on |

## Return value

The new image.

## Complexity

Linear in the number of pixels.

## Exceptions

`invalid_argument` for an angle that is not a multiple of 90, a contract of the program. The image is left as it was.

## Notes

On arm64 the quarter turns go by blocks of 8 × 8 pixels transposed in NEON registers (4 × 4 for pixels of 4 bytes,
2 × 2 for 8), in tiles that stay in the cache, and the half turn reverses rows in registers; elsewhere, and under
`SGCL_CODEC_PORTABLE`, pixel by pixel. Both give the same bytes.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(3, 2, codec::pixel_format::gray8);
    for (int i : range(6)) {
        picture.pixels()[i] = byte(i + 1);
    }
    codec::image turned = picture.rotated(90);
    println("{}x{}", turned.width(), turned.height());
    for (int y : range(3)) {
        println("{} {}", int(turned.row(y)[0]), int(turned.row(y)[1]));
    }
    println("back: {}", turned.rotated(-90).pixels() == picture.pixels());
    try {
        picture.rotated(45);
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
2x3
4 1
5 2
6 3
back: true
sgcl::codec::image::rotated: an angle that is not a multiple of 90 degrees
```

## See also

- [flipped](flipped.md), [cropped](cropped.md): the other new images of the same pixels
- [oriented](oriented.md): the image as its orientation says it is shown
- [sgcl::codec::image](README.md)
