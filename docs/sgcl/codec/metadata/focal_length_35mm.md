[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::focal_length_35mm

```cpp
optional<uint32_t> focal_length_35mm() const noexcept;
```

The focal length a lens on 35 mm film would need for the same field of view, in millimetres: EXIF's
FocalLengthIn35mmFilm (0xA405), else XMP's `exif:FocalLengthIn35mmFilm`. A phone's lens of 4.4 mm is 27 here.

## Parameters

None.

## Return value

The length, or `nullopt` (0, which EXIF writes for "unknown", too).

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
    println("{} mm", camera.focal_length_35mm().value_or(0));
    println("{} mm", edited.focal_length_35mm().value_or(0));
}
```

Output:

```text
105 mm
400 mm
```

## See also

- [focal_length](focal_length.md): the lens's own length
- [sgcl::codec::metadata](README.md)
