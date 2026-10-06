[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::location

```cpp
optional<codec::location> location() const noexcept;
```

Where the picture was taken: EXIF's GPS IFD, the latitude (2) and longitude (4) in degrees, minutes and seconds with
their references (1, 3: `S` and `W` negative) and the altitude (6) with its reference (5: 1 below sea level), else
XMP's `exif:GPSLatitude` and `exif:GPSLongitude` (`"52,13.5N"`, `"21,0,36W"`, or a signed decimal) and
`exif:GPSAltitude`. A latitude past 90° or a longitude past 180° gives no place.

## Parameters

None.

## Return value

The [location](../location.md), its altitude `nullopt` when the file gives none; or `nullopt`.

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
    codec::location here = *camera.location();
    println("{:.4f}, {:.4f}, {} m", here.latitude, here.longitude, *here.altitude);
    codec::location there = *edited.location();
    println("{:.4f}, {:.4f}", there.latitude, there.longitude);
}
```

Output:

```text
-33.8598, 151.2084, -12.5 m
52.2250, -21.0100
```

## See also

- [location](../location.md): latitude, longitude and altitude
- [gps_time](gps_time.md): when the position was fixed
- [sgcl::codec::metadata](README.md)
