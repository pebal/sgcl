[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::focal_length

```cpp
optional<double> focal_length() const noexcept;
```

The focal length of the lens in millimetres, as it is (not in 35 mm terms): EXIF's FocalLength (0x920A), else XMP's `exif:FocalLength`.

## Parameters

None.

## Return value

A positive number of millimetres, or `nullopt` (a rational of denominator 0 too).

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
    println("{}", camera.focal_length().value_or(0));
    println("{}", edited.focal_length().value_or(0));
}
```

Output:

```text
105
400
```

## See also

- [iso](iso.md), [exposure_time](exposure_time.md), [f_number](f_number.md): the exposure
- [sgcl::codec::metadata](README.md)
