[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::f_number

```cpp
optional<double> f_number() const noexcept;
```

The aperture as an f-number, 2.8 for f/2.8: EXIF's FNumber (0x829D), else its APEX ApertureValue (0x9202) as √2^Av, else XMP's `exif:FNumber` or `exif:ApertureValue`.

## Parameters

None.

## Return value

A positive number, or `nullopt` (a rational of denominator 0 too).

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
    println("{}", camera.f_number().value_or(0));
    println("{}", edited.f_number().value_or(0));
}
```

Output:

```text
2.8
5.6
```

## See also

- [iso](iso.md), [exposure_time](exposure_time.md), [f_number](f_number.md): the exposure
- [sgcl::codec::metadata](README.md)
