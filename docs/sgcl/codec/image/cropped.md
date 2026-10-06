[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::cropped

```cpp
image cropped(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const;
```

A new image of the rectangle whose top-left corner is the pixel (`x`, `y`), `width` × `height` pixels: its pixels
copied row by row, the [format](format.md) and the metadata (EXIF, the ICC profile, the
[orientation](orientation.md)) with them. The image `cropped` is called on keeps its pixels. Where Go's `SubImage` gives
a view that shares the pixels, `cropped` always copies, as every new image of the class does.

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the rectangle's top-left pixel |
| `width`, `height` | its sides, in pixels |

## Return value

The new image.

## Complexity

Linear in the pixels of the rectangle.

## Exceptions

`out_of_range` for a side of zero or a rectangle that reaches past the image, a contract of the program (no
arithmetic wraps: `x + width` is never computed). The image is left as it was.

## Notes

EXIF's tags of the image's size, when the block has them, are not changed: the module keeps the block as bytes and
edits only its orientation tag.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(8, 6, codec::pixel_format::gray8);
    for (int y : range(6)) {
        for (int x : range(8)) {
            picture.row(y)[x] = byte(10 * y + x);
        }
    }
    codec::image part = picture.cropped(2, 1, 3, 2);
    println("{}x{}", part.width(), part.height());
    for (int y : range(2)) {
        println("{} {} {}", int(part.row(y)[0]), int(part.row(y)[1]), int(part.row(y)[2]));
    }
    try {
        picture.cropped(6, 0, 3, 1);
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
3x2
12 13 14
22 23 24
sgcl::codec::image::cropped: a rectangle outside the image
```

## See also

- [flipped](flipped.md), [rotated](rotated.md): the other new images of the same pixels
- [row](row.md): a row's pixels in place
- [sgcl::codec::image](README.md)
