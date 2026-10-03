[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::set_exif

```cpp
void set_exif(const slice<const byte>& bytes) noexcept;
```

Sets the EXIF block of the image to a copy of `bytes`, which [exif](exif.md) then gives and the encoders write: a
TIFF structure from its byte-order mark (`II` or `MM`), without the `Exif` header of JPEG's segment. An empty slice
removes the block.

When the block has the orientation tag (0x0112 in its first directory, a SHORT), [orientation](orientation.md)
becomes the tag's value, as a decoder reads it: 1 for a value outside 1 to 8. A block without the tag leaves the
orientation as it was. The module reads nothing else of the block and checks nothing else: the bytes are the
program's, and [png::encode](../png/encode.md) and [jpeg::encode](../jpeg/encode.md) write them as they are.

The image is a handle: every copy of the handle sees the new block.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the EXIF block, or an empty slice for none |

## Return value

None.

## Complexity

Linear in the size of the block.

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
    println("{} bytes, orientation {}", photo.exif().size(), photo.orientation());
    photo.set_orientation(3);
    println("the tag: {}", std::to_integer<int>(photo.exif()[19]));
    photo.set_exif({});
    println("{} bytes, orientation {}", photo.exif().size(), photo.orientation());
}
```

Output:

```text
26 bytes, orientation 6
the tag: 3
0 bytes, orientation 3
```

## See also

- [exif](exif.md): the EXIF block
- [set_orientation](set_orientation.md): sets the orientation
- [set_icc](set_icc.md): sets the ICC profile
- [sgcl::codec::image](README.md)
