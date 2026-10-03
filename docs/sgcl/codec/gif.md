[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::gif

```cpp
#include "sgcl/codec/gif.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class gif;
}
```

`sgcl::codec::gif` reads GIF, GIF87a and GIF89a, still or animated: [decode](gif/decode.md) gives the first frame as
an [image](image.md), [frames](gif/frames.md) every frame, read one by one as the program asks for them. Every member
is static, and there is no encoder. [codec::decode](decode.md) and [codec::decode_frames](decode_frames.md) read a GIF
too, told by its signature.

Where Go's `gif.DecodeAll` hands out each image as the file has it, a paletted rectangle with a disposal for the
program to apply, here every frame is the whole canvas as it is shown, its palette, transparency and disposal done.

## Rules

- **Each frame is the whole canvas** (the logical screen) as it is shown, `rgba8` unless
  [decode_options](decode_options.md)`::want` asks for another format. The canvas starts transparent. A frame's own
  pixels are drawn over what the frames before left, the pixels of its transparent index are left as they were, and
  an interlaced frame's rows are put in their places.
- **The disposal** of a frame is done after it is shown, before the next frame is drawn: 0 and 1 leave the frame as
  it is; 2 clears its rectangle to transparent (as browsers do; the background color is not used); 3 puts back what
  was under it; 4 to 7 act as 1.
- **The LZW data** is compress's [LZW](../compress/lzw.md), with the deferred clear GIF encoders use.
- **Strict, as Go is.** An unknown block, a code past the LZW table, an image with fewer pixels than its size and an
  image with no color table are `errc::corrupt`; data that ends early is `errc::unexpected_end`. A logical screen or
  a frame of more pixels than [limits](limits.md)`::max_pixels` is `errc::too_large`.
- **Lenient.** Pixels past an image's size are dropped (giflib does the same; Go refuses the file). An index past
  the palette is opaque black, and a frame reaching past the canvas is clipped to it.
- **Errors are values**: an [expected](../core/expected.md) with an [error](error.md), whose code is an
  [errc](errc.md). A program that takes the image directly, `codec::image photo = codec::gif::decode(file);`, gets a
  [bad_expected_access](../core/bad_expected_access.md) thrown on an error.
- **Memory.** The canvas, the indices of the largest frame so far, and for disposal 3 a copy of the canvas made the
  first time it is needed. Nothing is allocated for each frame but the image handed out.

## Member functions

| Function | Description |
|---|---|
| [decode](gif/decode.md) | the first frame, as the whole canvas (static) |
| [frames](gif/frames.md) | every frame, read one by one (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/gif_decode/fire.gif");
    codec::image first = codec::gif::decode(file);
    println("the first frame: {}x{}", first.width(), first.height());

    codec::frames clip = codec::gif::frames(file);
    int count = 0;
    while (optional<codec::frame> shown = clip.next()) {
        ++count;
    }
    println("{} frames, plays {} (0: forever)", count, clip.loop_count());
}
```

Output:

```text
the first frame: 30x60
33 frames, plays 0 (0: forever)
```

## See also

- [frames](frames.md), [frame](frame.md): an animation read frame by frame
- [decode](decode.md), [decode_frames](decode_frames.md): any format, told by its signature
- [webp](webp.md): the other animated format
- [image](image.md), [error](error.md)
