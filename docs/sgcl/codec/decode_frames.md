[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::decode_frames

```cpp
#include "sgcl/codec/decode.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    expected<codec::frames, error> decode_frames(const slice<const byte>& data,             // (1)
                                                 const decode_options& o = {}) noexcept;
    expected<codec::frames, error> decode_frames(const io::reader& in,                      // (2)
                                                 const decode_options& o = {});
}
```

Reads an animation, GIF or WebP, told by its signature ([sniff](sniff.md)), as [gif::frames](gif/frames.md) and
[webp::frames](webp/frames.md) read it: the [frames](frames/README.md), each decoded when [next](frames/next.md) asks for it.
A still WebP, and a GIF of one image, are one frame; a file of no image is `errc::corrupt` at the first
[next](frames/next.md), as `decode` of it is. Any other format is `errc::unsupported`, PNG and JPEG among
them; [decode](decode.md) gives the image of those, and the first frame of an animation.

1. The file in memory. The bytes are held while the frames live: a slice of memory that is not managed must
   outlive them.
2. The file from a stream: the first bytes are read to tell the format, and the stream is held and read as the
   frames are asked for, not whole first.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format of the frames (`rgba8` when not given), the limits the canvas and the metadata may not pass, and whether the metadata is read ([decode_options](decode_options.md)) |

## Return value

The frames, with the size of the canvas and what is known of the loop count; or the [error](error/README.md) of what is
read before the first frame: `errc::unsupported` for a file that is not GIF or WebP, `errc::too_large` for a canvas
past `o.limits`, `errc::invalid_argument` for an `o.want` outside the list of pixel formats, the decoder's error for
a header the format does not allow, and (2) `errc::io` when the stream fails, with io's error inside.

## Complexity

Linear in what precedes the first frame: the header, and a GIF's global palette and the extensions before its first
image. The frames are decoded by [next](frames/next.md).

## Exceptions

- (1) None.
- (2) What the read of the stream throws: an `io::reader` calls the read of the object it holds.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* path : {"tests/codec/fuzz/seeds/gif_decode/welcome2.gif",
                             "tests/codec/fuzz/seeds/webp_decode/made_animation.webp",
                             "tests/codec/fuzz/seeds/webp_decode/lossless4.webp"}) {
        vector<byte> file = io::read_file(path);
        codec::frames clip = codec::decode_frames(file);
        int count = 0;
        while (optional<codec::frame> shown = clip.next()) {
            ++count;
        }
        println("{}x{}, frames: {}", clip.width(), clip.height(), count);
    }

    codec::image picture(8, 8, codec::pixel_format::rgb8);
    println(codec::decode_frames(codec::png::encode(picture)).error().message());
}
```

Output:

```text
290x48, frames: 6
16x12, frames: 4
256x256, frames: 1
offset 0: not an animation format the module reads (GIF, WebP)
```

## See also

- [frames](frames/README.md): what it returns
- [gif::frames](gif/frames.md), [webp::frames](webp/frames.md): the same of one format
- [decode](decode.md): a still image, or the first frame
- [codec](README.md)
