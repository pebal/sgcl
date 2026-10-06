[sgcl](../README.md) › [codec](README.md) › [qr](qr/README.md)

# sgcl::codec::qr::options

```cpp
#include "sgcl/codec/qr.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class qr {
    public:
        struct options {
            qr::level level = qr::level::medium;
            int min_version = 1;
            int max_version = 40;
            int mask = -1;
            bool boost_level = true;
            bool kanji = true;
            bool eci = true;
        };
    };
}
```

`sgcl::codec::qr::options` is what [encode](qr/encode.md) is told, a plain struct written in place:
`{.level = codec::qr::level::high, .min_version = 5}`. The defaults make the smallest symbol at level M that
readers take.

## Member objects

| Member | Description |
|---|---|
| `level` | the error correction, a [level](qr-level.md); `level::medium` unless told |
| `min_version` | the smallest version allowed, 1 to 40; 1 unless told (a fixed size: both versions the same) |
| `max_version` | the largest version allowed, `min_version` to 40; past what it holds is `errc::too_large`; 40 unless told |
| `mask` | the mask pattern, 0 to 7, or −1 for the one of least penalty, the standard's rule; −1 unless told |
| `boost_level` | a higher level where the version chosen still holds the data; true unless told |
| `kanji` | Kanji mode for characters of Shift JIS's Kanji ranges, 13 bits for what UTF-8 takes 24; true unless told |
| `eci` | an ECI of UTF-8 in front of a byte segment that is not ASCII; true unless told (off writes the bytes bare, as some old generators do) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr plain = codec::qr::encode("SGCL");
    codec::qr fixed = codec::qr::encode("SGCL", {.level = codec::qr::level::high, .min_version = 5,
                                                 .mask = 2});
    println("version {}, level {}, mask {}", fixed.version(), int(fixed.correction()),
            fixed.mask());
    println("version {}, level {}", plain.version(), int(plain.correction()));
}
```

Output:

```text
version 5, level 3, mask 2
version 1, level 3
```

## See also

- [level](qr-level.md): the levels of correction
- [encode](qr/encode.md): what takes the options
- [sgcl::codec::qr](qr/README.md)
