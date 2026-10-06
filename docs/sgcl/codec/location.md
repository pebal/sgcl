[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::location

```cpp
#include "sgcl/codec/metadata.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    struct location {
        double latitude = 0;
        double longitude = 0;
        optional<double> altitude;
    };
}
```

`sgcl::codec::location` is where a photo was taken, as [metadata::location](metadata/location.md) reads it from
EXIF's GPS IFD or XMP: degrees as decimals on WGS 84, the datum of GPS, a plain struct of values.

## Member objects

| Member | Description |
|---|---|
| `latitude` | degrees north of the equator, −90 to 90, negative south |
| `longitude` | degrees east of Greenwich, −180 to 180, negative west |
| `altitude` | metres above sea level, negative below; `nullopt` when the file gives none |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

#include <cmath>

using namespace sgcl;

int main() {
    codec::location here =
            *codec::metadata::load("tests/codec/fuzz/seeds/metadata/camera.jpg")->location();
    println("{:.6f} {}", std::abs(here.latitude), here.latitude < 0 ? 'S' : 'N');
    println("{:.6f} {}", std::abs(here.longitude), here.longitude < 0 ? 'W' : 'E');
    println("{} m", here.altitude.value_or(0));
}
```

Output:

```text
33.859800 S
151.208433 E
-12.5 m
```

## See also

- [metadata::location](metadata/location.md): where it comes from
- [metadata](metadata/README.md)
