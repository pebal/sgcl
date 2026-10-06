[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::iso

```cpp
optional<uint32_t> iso() const noexcept;
```

The sensitivity, ISO 400 as 400: EXIF's PhotographicSensitivity (0x8827, ISOSpeedRatings before EXIF 2.3), its first
value; when that is 65535, which means "65535 or more", or missing, ISOSpeed (0x8833); else XMP's
`exif:ISOSpeedRatings` or `exifEX:PhotographicSensitivity`.

## Parameters

None.

## Return value

The sensitivity, or `nullopt`.

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
    println("ISO {}", camera.iso().value_or(0));
    println("ISO {}", edited.iso().value_or(0));
}
```

Output:

```text
ISO 400
ISO 3200
```

## See also

- [exposure_time](exposure_time.md), [f_number](f_number.md): the rest of the exposure
- [sgcl::codec::metadata](README.md)
