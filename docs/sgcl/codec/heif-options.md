[sgcl](../README.md) › [codec](README.md) › [heif](heif/README.md)

# sgcl::codec::heif::options

```cpp
#include "sgcl/codec/heif.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class heif {
    public:
        struct options {
            int quality = 85;
        };
    };
}
```

`sgcl::codec::heif::options` is what an [encode](heif/encode.md) takes: a plain struct, filled by a designated
initializer, `codec::heif::encode(photo, {.quality = 60})`. The overloads of `encode` without it encode with its
defaults.

## Member objects

| Member | Description |
|---|---|
| `quality` | the quality of the encoding, 1 to 100, given to ImageIO as a lossy quality of `quality / 100`; 85 by default. A value outside is `errc::invalid_argument` at `encode` |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::rgb8);
    for (int y : range(48)) {
        slice<byte> row = picture.row(y);
        for (int x : range(64)) {
            row[3 * x] = byte(x * 4);
            row[3 * x + 1] = byte(y * 5);
            row[3 * x + 2] = byte(x * y);
        }
    }
    vector<byte> small = codec::heif::encode(picture, {.quality = 20});
    vector<byte> large = codec::heif::encode(picture, {.quality = 95});
    println("quality 20 is smaller than 95: {}", small.size() < large.size());

    auto refused = codec::heif::encode(picture, {.quality = 0});
    if (!refused) println(refused.error().message());
}
```

Output:

```text
quality 20 is smaller than 95: true
offset 0: heif: quality outside 1..100
```

## See also

- [encode](heif/encode.md): what takes it
- [jpeg::options](jpeg-options.md): the quality of a JPEG
- [sgcl::codec::heif](heif/README.md)
