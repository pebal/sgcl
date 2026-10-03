[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::set_orientation

```cpp
void set_orientation(unsigned value);
```

Sets EXIF's orientation of the image to `value`, 1 to 8, as [orientation](orientation.md) lists them: how the rows as
stored are to be turned and mirrored to be shown. When the [EXIF block](exif.md) has the orientation tag, the tag is
set to `value` too, so that a file written from the image says what the image says. A program that has turned the
pixels itself sets 1; one that made an image lying on its side sets the turn that shows it upright.

PNG and JPEG carry the orientation only in the EXIF block: [png::encode](../png/encode.md) and
[jpeg::encode](../jpeg/encode.md) write the block as it is, so an image without one, or with a block that has no
orientation tag, is written without the orientation. [heif::encode](../heif/encode.md) writes `orientation()` itself.

The image is a handle: the orientation is its state, and every copy of the handle sees the new value, as it sees a
write to the pixels.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the orientation, 1 to 8 |

## Return value

None.

## Complexity

Linear in the entries of the EXIF block's first directory, found before the tag; constant without a block.

## Exceptions

`invalid_argument` when `value` is outside 1 to 8; the image is left as it was.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image photo(9, 7, codec::pixel_format::rgb8);
    photo.set_orientation(6);
    codec::image upright = photo.oriented();
    println("{}, shown {}x{}", photo.orientation(), upright.width(), upright.height());
    try {
        photo.set_orientation(9);
    } catch (const std::invalid_argument& e) {
        println(e.what());
    }
    println("{}", photo.orientation());
}
```

Output:

```text
6, shown 7x9
sgcl::codec::image::set_orientation: an orientation outside 1..8
6
```

## See also

- [orientation](orientation.md): the eight orientations
- [oriented](oriented.md): the image as it is meant to be shown
- [set_exif](set_exif.md): sets the EXIF block
- [sgcl::codec::image](README.md)
