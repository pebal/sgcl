[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::gps_time

```cpp
optional<time::datetime> gps_time() const;
```

When the GPS receiver fixed the position, in UTC: EXIF's GPSDateStamp (0x1D) and GPSTimeStamp (7, three rationals:
hours, minutes and seconds with their fraction), else XMP's `exif:GPSTimeStamp`. Unlike the camera's clock it is
UTC by definition, and the difference between the two tells the zone the camera's clock was set to.

## Parameters

None.

## Return value

The time in UTC, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    codec::metadata camera = codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg");
    codec::metadata edited = codec::metadata::load("tests/codec/fuzz/seeds/metadata/xmp.webp");
    println("{}", *camera.gps_time());
    println("{}", *edited.gps_time());
}
```

Output:

```text
2024-05-06T16:29:40.5Z
2023-08-01T10:15:30Z
```

## See also

- [date_time_original](date_time_original.md): the camera's clock
- [location](location.md): the position
- [sgcl::codec::metadata](README.md)
