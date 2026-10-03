[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::oriented

```cpp
image oriented() const noexcept;
```

A new image as it is meant to be shown: the pixels turned and mirrored as [orientation](orientation.md) says, the
[width](width.md) and the [height](height.md) swapped for 5 to 8, and `orientation()` 1. An image of orientation 1
gives a copy of itself. The format and the metadata go with the pixels, and the orientation tag of the
[EXIF block](exif.md), when it has one, is set to 1 in the new image's copy, the rest of the block as it was: a file
written from the new image is shown as it is, not turned a second time. The image `oriented` is called on keeps
its pixels and its block.

## Parameters

None.

## Return value

The new image.

## Complexity

Linear in the number of pixels.

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
    codec::image photo(9, 7, codec::pixel_format::rgb8);
    photo.set_exif(exif_of(6));
    codec::image upright = photo.oriented();
    println("{}x{}, orientation {}", photo.width(), photo.height(), photo.orientation());
    println("{}x{}, orientation {}", upright.width(), upright.height(), upright.orientation());
    codec::image again = codec::jpeg::decode(codec::jpeg::encode(upright));
    println("{}x{}, orientation {} from JPEG", again.width(), again.height(), again.orientation());
}
```

Output:

```text
9x7, orientation 6
7x9, orientation 1
7x9, orientation 1 from JPEG
```

## See also

- [orientation](orientation.md): the eight orientations
- [exif](exif.md): the block the orientation is read from
- [set_orientation](set_orientation.md): sets the orientation
- [sgcl::codec::image](../image.md)
