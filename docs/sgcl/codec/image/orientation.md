[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::orientation

```cpp
uint8_t orientation() const noexcept;
```

EXIF's orientation of the image, 1 to 8: how the rows as stored are to be turned and mirrored to be shown. 1 is the
image as stored, and also what an image whose file said nothing has; 2 to 8 are a mirror, a turn by a multiple of
90 degrees, or both, as [oriented](oriented.md) applies them:

| Value | To be shown |
|---|---|
| 1 | as stored |
| 2 | mirrored left to right |
| 3 | turned by 180 degrees |
| 4 | mirrored top to bottom |
| 5 | transposed: row `y` becomes column `y` |
| 6 | turned clockwise |
| 7 | transposed across the other diagonal |
| 8 | turned counterclockwise |

PNG, JPEG and WebP take it from the tag of their [EXIF block](exif.md), where a value outside 1 to 8, or a block that
is not whole, reads as 1; HEIF from what the system reads of the file's rotation and mirror, a number outside 1 to 8
read as 1 too. A GIF, an image decoded with [decode_options](../decode_options.md)`.metadata` false and an image the
program made have 1, until [set_orientation](set_orientation.md) or [set_exif](set_exif.md) sets another. The decoders
never turn an image themselves, as neither Go nor libjpeg does: the rows are as the file stores them.

## Parameters

None.

## Return value

The orientation, 1 to 8.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

// A TIFF structure of one tag, the orientation (0x0112) o, big-endian ("MM")
vector<byte> exif_of(int o) {
    vector<byte> block;
    for (int v : {0x4D, 0x4D, 0, 42, 0, 0, 0, 8, 0, 1, 1, 0x12, 0, 3, 0, 0, 0, 1, 0, o}) {
        block.push_back(byte(v));
    }
    block.resize(26);  // the rest of the tag's value and the end of the directories: zeros
    return block;
}

int main() {
    codec::image made(9, 7, codec::pixel_format::rgb8);
    println("{}", made.orientation());
    made.set_exif(exif_of(6));
    vector<byte> file = codec::jpeg::encode(made);
    codec::image photo = codec::jpeg::decode(file);
    codec::image bare = codec::jpeg::decode(file, {.metadata = false});
    println("{} {}", photo.orientation(), bare.orientation());
}
```

Output:

```text
1
6 1
```

## See also

- [oriented](oriented.md): the image as it is meant to be shown
- [exif](exif.md): the block the orientation is read from
- [set_orientation](set_orientation.md): sets the orientation
- [sgcl::codec::image](../image.md)
