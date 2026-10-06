[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::flip

```cpp
#include "sgcl/codec/image.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    enum class flip : uint8_t {
        horizontal,
        vertical
    };
}
```

Which way [image::flipped](image/flipped.md) mirrors an image.

| Value | Description |
|---|---|
| `horizontal` | left to right: each row's pixels reversed, what a mirror shows |
| `vertical` | top to bottom: the rows reversed |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(2, 2, codec::pixel_format::gray8);
    for (int i : range(4)) {
        picture.pixels()[i] = byte(i);
    }
    for (codec::flip f : {codec::flip::horizontal, codec::flip::vertical}) {
        slice<const byte> px = picture.flipped(f).pixels();
        println("{} {} {} {}", int(px[0]), int(px[1]), int(px[2]), int(px[3]));
    }
}
```

Output:

```text
1 0 3 2
2 3 0 1
```

## See also

- [image::flipped](image/flipped.md): what takes it
- [image::rotated](image/rotated.md): the turns
- [codec](README.md)
