[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::flipped

```cpp
image flipped(flip direction = flip::horizontal) const noexcept;
```

A new image mirrored: left to right unless `direction` is [flip](../flip.md)`::vertical`, top to bottom. The
[format](format.md) and the metadata go with the pixels, the [orientation](orientation.md) as it was (the program
mirrored the pixels on purpose; [oriented](oriented.md) applies the stored orientation instead). The image `flipped`
is called on keeps its pixels.

## Parameters

| Parameter | Description |
|---|---|
| `direction` | `flip::horizontal` (the default): each row's pixels reversed; `flip::vertical`: the rows reversed |

## Return value

The new image.

## Complexity

Linear in the number of pixels.

## Exceptions

None.

## Notes

On arm64 a row is reversed in NEON registers (for pixels of 1 to 4 bytes) and the vertical flip copies whole rows;
elsewhere, and under `SGCL_CODEC_PORTABLE`, pixel by pixel. Both give the same bytes.

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
    codec::image mirror = picture.flipped();
    codec::image upside = picture.flipped(codec::flip::vertical);
    for (int y : range(2)) {
        println("{} {} {} | {} {} {}", int(mirror.row(y)[0]), int(mirror.row(y)[1]), int(mirror.row(y)[2]),
                int(upside.row(y)[0]), int(upside.row(y)[1]), int(upside.row(y)[2]));
    }
}
```

Output:

```text
3 2 1 | 4 5 6
6 5 4 | 1 2 3
```

## See also

- [flip](../flip.md): the two directions
- [rotated](rotated.md), [cropped](cropped.md): the other new images of the same pixels
- [oriented](oriented.md): the image as its orientation says it is shown
- [sgcl::codec::image](README.md)
