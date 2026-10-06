[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::width

```cpp
optional<uint32_t> width() const noexcept;
```

The width of the picture in pixels as the metadata says it: EXIF's PixelXDimension (0xA002), else ImageWidth (0x0100) (a TIFF's own), else XMP's
`exif:PixelXDimension` or `tiff:ImageWidth`. It is what the camera or the last program wrote, not the pixels: a program that crops a picture and keeps its
EXIF leaves the old size here. The pixels' size is the decoded [image](../image/README.md)'s.

## Parameters

None.

## Return value

The size, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::metadata camera = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    codec::metadata edited = codec::metadata::load("tests/codec/fuzz/seeds/metadata/xmp.webp");
    println("{}", camera.width().value_or(0));
    println("{}", edited.width().value_or(0));
}
```

Output:

```text
8192
8256
```

## See also

- [height](height.md)
- [sgcl::codec::metadata](README.md)
