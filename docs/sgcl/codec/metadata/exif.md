[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::exif

```cpp
slice<const byte> exif() const noexcept;
```

The EXIF block the fields were read from, as the file holds it: the TIFF structure from its byte-order mark (`II` or
`MM`), without JPEG's `Exif\0\0`. Empty for a file without one, and for a TIFF, whose EXIF is the file itself
(its IFD0 and the IFDs it points to, read in place). A program reads the tags the type has no field for from it,
and [image::set_exif](../image/set_exif.md) takes it as it is.

## Parameters

None.

## Return value

The bytes of the block, or an empty slice. The slice keeps them alive after the metadata is gone.

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
    slice<const byte> block = camera.exif();
    println("{} bytes, {}{}", block.size(), char(block[0]), char(block[1]));
    println("{} bytes", edited.exif().size());
}
```

Output:

```text
758 bytes, II
0 bytes
```

## See also

- [xmp](xmp.md): the other block
- [image::exif](../image/exif.md): the block a decoded image keeps
- [sgcl::codec::metadata](README.md)
