[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::exposure_bias

```cpp
optional<double> exposure_bias() const noexcept;
```

The exposure compensation in EV, negative for darker: EXIF's ExposureBiasValue (0x9204), else XMP's `exif:ExposureBiasValue`.

## Parameters

None.

## Return value

A number of EV, or `nullopt` (a rational of denominator 0 too).

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
    println("{}", camera.exposure_bias().value_or(0));
    println("{}", edited.exposure_bias().value_or(0));
}
```

Output:

```text
-0.6666666666666666
0.3333333333333333
```

## See also

- [iso](iso.md), [exposure_time](exposure_time.md), [f_number](f_number.md): the exposure
- [sgcl::codec::metadata](README.md)
