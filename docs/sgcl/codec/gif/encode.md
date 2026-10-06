[sgcl](../../README.md) › [codec](../README.md) › [gif](README.md)

# sgcl::codec::gif::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;      // (1)
static expected<void, error> encode(const image& im, const io::writer& out,                        // (2)
                                    const options& o = {});
static expected<vector<byte>, error> encode(const slice<const frame>& animation,                   // (3)
                                            const options& o = {}) noexcept;
static expected<void, error> encode(const slice<const frame>& animation, const io::writer& out,    // (4)
                                    const options& o = {});
```

Encodes an image, or an animation, as a GIF89a file.

1. Returns the file of a still image as bytes.
2. Writes it into a stream.
3. Returns the file of an animation as bytes: each [frame](../frame.md) the whole canvas, shown for its delay, the
   animation played `o.loop_count` times.
4. Writes it into a stream.

Any image is written, whatever its [pixel format](../pixel_format.md): a pixel of alpha below 128 is transparent, any
other opaque with its color, 16-bit and gray formats through [convert](../image/convert.md)'s rules.

- **The palette.** Each frame gets a palette of its own, at most `o.colors` entries, the transparent one among them
  when a pixel needs it. An image of no more colors than that is written with its own colors, pixel for pixel. Any
  other is reduced by median cut (the box of the most pixels split at its median, then the box of the most pixels
  times its volume) and each pixel written as the entry nearest to it; with `o.dither`, the default, by
  Floyd–Steinberg, the error of each pixel carried to the pixels after it.
- **An animation** is a slice of whole canvases, as [frames](../frames/README.md) gives them, all of the first one's
  size. The first frame covers the screen; each one after it covers the pixels that differ from what the frame
  before leaves, and the pixels inside its rectangle that do not change are written transparent, so that drawing it
  leaves them. A pixel that goes from opaque to transparent, which drawing over the canvas cannot do, has the frame
  before it disposed of to the background (cleared to transparent, as decoders and browsers show it). The delays are
  written in hundredths of a second, rounded to the nearest, at most 655.35 s; the loop count as NETSCAPE2.0 has it.
- **The data** is compress's [LZW](../../compress/lzw/README.md), least significant bit first, cut into sub-blocks of
  255 bytes.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `animation` | the frames: their pictures, all of one size, and their delays |
| `out` | the stream the file is written into |
| `o` | the colors of a palette, dithering, the loop count of an animation ([options](../gif-options.md)) |

## Return value

- (1, 3) The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for a side past 65 535
  pixels, which the logical screen cannot hold, for `o.colors` outside 2 to 256, and (3) for no frame or a frame of
  another size than the first; any other image or animation encodes.
- (2, 4) Nothing, or the error: the same refusals, before anything is written; `errc::io` when the stream fails, at
  the offset of the bytes written before, the stream's own error in [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image or of every frame.

## Exceptions

- (1, 3) None.
- (2, 4) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The memory is a row of the image and of its indices, and the palette's tables, made for each frame: for an image of
few colors a byte a pixel of indices, for any other about 1.6 MB of histogram, candidates and colors met, 512 KB more
with dithering. An animation adds three canvases of 4 bytes a pixel. (1) and (3) add the file; (2) and (4) do not
hold it.

The frames of a file decoded by [gif::frames](frames.md) and collected into a [vector](../../core/vector/README.md)
encode again as they are shown, though not as the bytes they came from: the rectangles, disposals and palettes are
this encoder's.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // four colors and a transparent corner
    codec::image tiles(32, 32, codec::pixel_format::rgba8);
    for (int y : range(32)) {
        slice<byte> row = tiles.row(y);
        for (int x : range(32)) {
            if (x >= 4 || y >= 4) {
                row[4 * x] = byte(x < 16 ? 255 : 0);
                row[4 * x + 1] = byte(y < 16 ? 255 : 0);
                row[4 * x + 2] = byte(64);
                row[4 * x + 3] = byte(255);
            }
        }
    }
    vector<byte> file = codec::gif::encode(tiles);
    codec::image back = codec::gif::decode(file);
    println("{} bytes; the same pixels: {}", file.size(), back.pixels() == tiles.pixels());

    // a gradient of 65536 colors in 16
    codec::image gradient(256, 256, codec::pixel_format::rgb8);
    for (int y : range(256)) {
        slice<byte> row = gradient.row(y);
        for (int x : range(256)) {
            row[3 * x] = byte(x);
            row[3 * x + 1] = byte(y);
            row[3 * x + 2] = byte(128);
        }
    }
    vector<byte> smooth = codec::gif::encode(gradient, {.colors = 16});
    vector<byte> banded = codec::gif::encode(gradient, {.colors = 16, .dither = false});
    println("dithered {} bytes, banded {} bytes", smooth.size(), banded.size());
}
```

Output:

```text
166 bytes; the same pixels: true
dithered 15420 bytes, banded 2642 bytes
```

An animation: a square moving across, played forever.

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    vector<codec::frame> clip;
    for (int i : range(8)) {
        codec::image canvas(64, 16, codec::pixel_format::rgb8);
        for (int y : range(4, 12)) {
            slice<byte> row = canvas.row(y);
            for (int x : range(8 * i, 8 * i + 8)) {
                row[3 * x] = byte(255);
            }
        }
        clip.push_back({canvas, 100ms});
    }
    vector<byte> file = codec::gif::encode(clip);
    println("{} frames in {} bytes", clip.size(), file.size());

    codec::frames back = codec::gif::frames(file);
    int count = 0;
    while (optional<codec::frame> shown = back.next()) {
        ++count;
    }
    println("{} frames back, plays {} (0: forever)", count, back.loop_count());
}
```

Output:

```text
8 frames in 385 bytes
8 frames back, plays 0 (0: forever)
```

## See also

- [options](../gif-options.md): the colors, the dithering, the loop count
- [decode](decode.md), [frames](frames.md): the other way
- [save](../save.md): an image into a file, in the format its extension names
- [frame](../frame.md): a frame of an animation
- [sgcl::codec::gif](README.md)
