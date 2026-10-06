[sgcl](../README.md) › [codec](README.md) › [webp](webp/README.md)

# sgcl::codec::webp::options

```cpp
#include "sgcl/codec/webp.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class webp {
    public:
        struct options {
            bool lossless = false;
            int quality = 85;
            uint32_t loop_count = 0;
        };
    };
}
```

`sgcl::codec::webp::options` is what [encode](webp/encode.md) is told, a plain struct written in place:
`{.quality = 90}`, `{.lossless = true}`, `{.lossless = true, .loop_count = 3}`. `lossless` picks the bitstream: VP8L,
every pixel as it is, or VP8, lossy, its alpha exact. `quality` means what cwebp's `-q` means: for a lossy file the
quality, for a lossless file the effort, from 1, the fastest, to 100, the smallest; 85 by default, the default of
[save_options](save_options.md) and of JPEG, so that `encode` and `save` write the same file.
`loop_count` is how many times an animation plays, as [frames::loop_count](frames/loop_count.md) reads it back: 0
forever, n times; a still image has none.

## Member objects

| Member | Description |
|---|---|
| `lossless` | VP8L rather than VP8; false unless told |
| `quality` | 1 to 100 (one outside is `errc::invalid_argument` from `encode`): the quality of a lossy file, the effort of a lossless one; 85 unless told |
| `loop_count` | an animation's plays: 0 forever (the default), n times, at most 65 535 (ANIM's 16 bits) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(128, 128, codec::pixel_format::rgb8);
    for (int y : range(128)) {
        slice<byte> row = picture.row(y);
        for (int x : range(128)) {
            row[3 * x] = byte(2 * x);
            row[3 * x + 1] = byte(2 * y);
            row[3 * x + 2] = byte(x ^ y);
        }
    }
    for (int quality : {1, 50, 100}) {
        vector<byte> lossy = codec::webp::encode(picture, {.quality = quality});
        vector<byte> lossless = codec::webp::encode(picture, {.lossless = true,
                .quality = quality});
        println("quality {}: lossy {} bytes, lossless {} bytes", quality, lossy.size(),
                lossless.size());
    }
    auto refused = codec::webp::encode(picture, {.quality = 101});
    println(refused.error().message());
}
```

Output:

```text
quality 1: lossy 236 bytes, lossless 1014 bytes
quality 50: lossy 386 bytes, lossless 882 bytes
quality 100: lossy 2202 bytes, lossless 830 bytes
offset 0: webp: options.quality outside 1..100
```

## See also

- [encode](webp/encode.md): what takes the options
- [frames](frames/README.md): an animation read back, its loop count
- [save_options](save_options.md): `quality` and `lossless` for a `.webp` written by `save`
- [sgcl::codec::webp](webp/README.md)
