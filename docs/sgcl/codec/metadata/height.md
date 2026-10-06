[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::height

```cpp
optional<uint32_t> height() const noexcept;
```

The height of the picture in pixels as the metadata says it: EXIF's PixelYDimension (0xA003), else ImageLength (0x0101) (a TIFF's own), else XMP's
`exif:PixelYDimension` or `tiff:ImageLength`. It is what the camera or the last program wrote, not the pixels: a program that crops a picture and keeps its
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
    println("{}", camera.height().value_or(0));
    println("{}", edited.height().value_or(0));
}
```

Output:

```text
5464
5504
```

## See also

- [width](width.md)
- [sgcl::codec::metadata](README.md)
