[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::frame

```cpp
#include "sgcl/codec/frames.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    struct frame {
        image picture;
        duration delay;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::codec::frame` is one frame of an animation, what [frames::next](frames/next.md) gives: the canvas as it is
shown with this frame drawn on what came before, and how long it is shown. A plain struct of two fields.

## Member objects

| Member | Description |
|---|---|
| `picture` | the whole canvas as it is shown, a new [image](image/README.md) for every frame: `rgba8`, or the pixel format `decode_options.want` asked for |
| `delay` | how long the frame is shown, as the file says: GIF in hundredths of a second, 0 as often as not, which browsers show as 100 ms; WebP in milliseconds |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/fire.gif");
    codec::frames clip = codec::decode_frames(file);
    duration total;
    int count = 0;
    while (optional<codec::frame> shown = clip.next()) {
        total += shown->delay;
        ++count;
    }
    println("{} frames, {} in all", count, total);

    codec::frames again = codec::decode_frames(file);
    optional<codec::frame> first = again.next();
    println("{}x{}, rgba8: {}, {}", first->picture.width(), first->picture.height(),
            first->picture.format() == codec::pixel_format::rgba8, first->delay);
}
```

Output:

```text
33 frames, 1.65s in all
30x60, rgba8: true, 50ms
```

## See also

- [frames](frames/README.md): what reads them
- [image](image/README.md): the picture
- [codec](README.md)
