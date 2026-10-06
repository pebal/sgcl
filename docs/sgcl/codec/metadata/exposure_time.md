[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::exposure_time

```cpp
optional<double> exposure_time() const noexcept;
```

The exposure in seconds, 0.004 for 1/250 s: EXIF's ExposureTime (0x829A), else its APEX ShutterSpeedValue (0x9201) as 2^−Tv, else XMP's `exif:ExposureTime` or `exif:ShutterSpeedValue`.

## Parameters

None.

## Return value

A positive number of seconds, or `nullopt` (a rational of denominator 0 too).

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
    println("{}", camera.exposure_time().value_or(0));
    println("{}", edited.exposure_time().value_or(0));
}
```

Output:

```text
0.004
0.001
```

## See also

- [iso](iso.md), [exposure_time](exposure_time.md), [f_number](f_number.md): the exposure
- [sgcl::codec::metadata](README.md)
