[sgcl](../README.md) › [codec](README.md) › [ico](ico/README.md)

# sgcl::codec::ico::options

```cpp
#include "sgcl/codec/ico.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class ico {
    public:
        struct options {
            bool cursor = false;
            uint16_t hotspot_x = 0;
            uint16_t hotspot_y = 0;
        };
    };
}
```

`sgcl::codec::ico::options` is what [encode](ico/encode.md) is told: an icon (ICO, type 1) or a cursor (CUR, type
2) and the cursor's hotspot, the pixel that points, a plain struct written in place as `{.cursor = true,
.hotspot_x = 3}`. An icon's directory has planes and bits where a cursor's has the hotspot; the hotspot of an icon
is not written.

## Member objects

| Member | Description |
|---|---|
| `cursor` | a CUR rather than an ICO; false unless told |
| `hotspot_x` | the cursor's hot pixel from the left, in every entry; 0 unless told |
| `hotspot_y` | the cursor's hot pixel from the top; 0 unless told |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image pointer(32, 32, codec::pixel_format::rgba8);
    vector<byte> icon = codec::ico::encode(pointer);
    vector<byte> cursor = codec::ico::encode(pointer, {.cursor = true, .hotspot_x = 5,
            .hotspot_y = 7});
    println("types {} and {}; hotspot {},{}", int(icon[2]), int(cursor[2]), int(cursor[10]),
            int(cursor[12]));
}
```

Output:

```text
types 1 and 2; hotspot 5,7
```

## See also

- [encode](ico/encode.md): what takes the options
- [sgcl::codec::ico](ico/README.md)
