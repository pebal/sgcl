[sgcl](../README.md) › [codec](README.md) › [gif](gif/README.md)

# sgcl::codec::gif::options

```cpp
#include "sgcl/codec/gif.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class gif {
    public:
        struct options {
            int colors = 256;
            bool dither = true;
            uint32_t loop_count = 0;
        };
    };
}
```

`sgcl::codec::gif::options` is what [encode](gif/encode.md) is told, a plain struct written in place:
`{.colors = 64}`, `{.dither = false}`, `{.loop_count = 3}`. `colors` bounds every palette the encoder makes, the
transparent entry counted; an image of no more colors is written exactly, and one of more is reduced to them,
dithered by Floyd–Steinberg while `dither` is true (Go's `gif.Encode` dithers by default too). `loop_count` is how many
times an animation plays, as [frames::loop_count](frames/loop_count.md) reads it back: 0 forever, 1 once, n times;
a still image has no loop count.

## Member objects

| Member | Description |
|---|---|
| `colors` | the most entries of a palette, 2 to 256 (one outside is `errc::invalid_argument` from `encode`); 256 unless told |
| `dither` | Floyd–Steinberg where an image has more colors than the palette; true unless told |
| `loop_count` | an animation's plays: 0 forever (the default), 1 once, n times, at most 65 536 (NETSCAPE2.0's 16 bits plus the first play) |

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
    for (int colors : {256, 64, 16, 2}) {
        vector<byte> dithered = codec::gif::encode(picture, {.colors = colors});
        vector<byte> plain = codec::gif::encode(picture, {.colors = colors, .dither = false});
        println("{} colors: {} bytes dithered, {} not", colors, dithered.size(), plain.size());
    }
    auto refused = codec::gif::encode(picture, {.colors = 300});
    println(refused.error().message());
}
```

Output:

```text
256 colors: 10774 bytes dithered, 6235 not
64 colors: 7258 bytes dithered, 2751 not
16 colors: 4918 bytes dithered, 1228 not
2 colors: 1230 bytes dithered, 406 not
offset 0: gif: options.colors outside 2..256
```

## See also

- [encode](gif/encode.md): what takes the options
- [frames](frames/README.md): an animation read back, its loop count
- [save](save.md): a `.gif` written with the default options
- [sgcl::codec::gif](gif/README.md)
