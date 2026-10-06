[sgcl](../../README.md) › [codec](../README.md) › [webp](README.md)

# sgcl::codec::webp::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;      // (1)
static expected<void, error> encode(const image& im, const io::writer& out,                        // (2)
                                    const options& o = {});
static expected<vector<byte>, error> encode(const slice<const frame>& animation,                   // (3)
                                            const options& o = {}) noexcept;
static expected<void, error> encode(const slice<const frame>& animation, const io::writer& out,    // (4)
                                    const options& o = {});
```

Encodes an image, or an animation, as a WebP file.

1. Returns the file of a still image as bytes.
2. Writes it into a stream.
3. Returns the file of an animation as bytes: each [frame](../frame.md) the whole canvas, shown for its delay, the
   animation played `o.loop_count` times.
4. Writes it into a stream.

Any image is written, whatever its [pixel format](../pixel_format.md), as 8-bit RGBA: 16-bit channels, gray and
CMYK through [convert](../image/convert.md)'s rules.

- **Lossless** (`o.lossless`): VP8L, every pixel as it is, the color of a transparent pixel too. An image of at most
  256 colors is written as indices into a palette (bundled 8, 4 or 2 to a pixel for at most 2, 4 or 16 colors); any
  other through subtract-green and the predictor transform (a mode for each tile of 8 × 8 pixels, the one whose
  residuals cost least), at the highest efforts the cross-color transform as well where it makes the file smaller.
  Then LZ77 over a hash chain, the copies chosen by the cheapest path through the image, the color cache of the
  size that costs least, Huffman codes of at most 15 bits, in groups by tile where that saves bits. `o.quality` is
  the effort, as cwebp's `-q` with `-lossless`: how far the search goes, from 1, the fastest, to 100.
- **Lossy**: VP8. The image in YUV 4:2:0 (BT.601's limited range); each macroblock predicted as the decoder will
  predict it, by the 16 × 16 or the 4 × 4 luma modes and the chroma mode of least distortion plus a weight of the
  bits they take; the residue through the forward DCT (and the WHT of the luma DCs) and quantized at the quantizer
  `o.quality` gives; the probabilities of the tokens updated for the image where that saves bits; the normal loop
  filter at a level from the quantizer. `o.quality` reads as cwebp's `-q`: the same quality gives about the PSNR
  cwebp gives, in a file a few percent smaller. An image with alpha keeps it exact, in an ALPH chunk compressed by
  VP8L.
- **The container.** A still image is the simple form, one VP8L or VP8 chunk, unless it carries an EXIF block or an
  ICC profile, or is lossy with alpha: then VP8X, ICCP, ALPH, the image and EXIF. An animation is VP8X, ANIM (the
  loop count, a transparent background) and an ANMF chunk a frame: the first covers the canvas, each one after it
  the rectangle of the pixels that changed (its corner at even coordinates, as ANMF stores it), blended onto the
  canvas with the unchanged pixels transparent when every changed pixel is opaque, otherwise written over it.
  Delays are written in milliseconds, at most 2^24 − 1. The first frame's metadata is the animation's.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `animation` | the frames: their pictures, all of one size, and their delays |
| `out` | the stream the file is written into |
| `o` | lossless or lossy, the quality (the effort when lossless), the loop count of an animation ([options](../webp-options.md)) |

## Return value

- (1, 3) The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for a side past 16 384
  pixels, which VP8 and VP8L cannot hold, for `o.quality` outside 1 to 100, and (3) for no frame or a frame of
  another size than the first; any other image or animation encodes.
- (2, 4) Nothing, or the error: the same refusals, before anything is written; `errc::io` when the stream fails, at
  the offset of the bytes written before, the stream's own error in [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image or of every frame; the constant grows with `o.quality` for a lossless file.

## Exceptions

- (1, 3) None.
- (2, 4) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The file is made in memory and given to the stream whole, since RIFF puts its size first: (2) and (4) hold the file
as (1) and (3) do. The work memory is about 40 bytes a pixel for a lossless image (the pixels, their copies and
the tokens), about 10 for a lossy one (the pixels, the planes and what the macroblocks decided), and an animation's
two canvases of 4 bytes a pixel.

Go has no WebP encoder (`golang.org/x/image/webp` decodes). Against libwebp at cwebp's defaults (method 4), the
lossy files come out at the same PSNR a few percent smaller, and the lossless ones about as small; the times are in
[the benchmarks](../benchmarks.md).

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image gradient(256, 256, codec::pixel_format::rgba8);
    for (int y : range(256)) {
        slice<byte> row = gradient.row(y);
        for (int x : range(256)) {
            row[4 * x] = byte(x);
            row[4 * x + 1] = byte(y);
            row[4 * x + 2] = byte(128);
            row[4 * x + 3] = byte(x < 128 ? 255 : 2 * (255 - x));
        }
    }
    vector<byte> exact = codec::webp::encode(gradient, {.lossless = true});
    codec::image back = codec::webp::decode(exact);
    println("lossless: {} bytes, the same pixels: {}", exact.size(), back.pixels() == gradient.pixels());

    for (int quality : {30, 75, 95}) {
        vector<byte> lossy = codec::webp::encode(gradient, {.quality = quality});
        codec::image seen = codec::webp::decode(lossy);
        bool alpha = true;
        for (int i = 3; i < 4 * 256 * 256; i += 4) {
            alpha = alpha && seen.pixels()[i] == gradient.pixels()[i];
        }
        println("quality {}: {} bytes, alpha exact: {}", quality, lossy.size(), alpha);
    }
}
```

Output:

```text
lossless: 656 bytes, the same pixels: true
quality 30: 742 bytes, alpha exact: true
quality 75: 854 bytes, alpha exact: true
quality 95: 1292 bytes, alpha exact: true
```

An animation: a square moving across, played forever, written losslessly.

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
    vector<byte> file = codec::webp::encode(clip, {.lossless = true});
    println("{} frames in {} bytes", clip.size(), file.size());

    codec::frames back = codec::webp::frames(file);
    int same = 0;
    while (optional<codec::frame> shown = back.next()) {
        same += shown->picture.convert(codec::pixel_format::rgb8).pixels() == clip[same].picture.pixels();
    }
    println("{} frames back as they were, plays {} (0: forever)", same, back.loop_count());
}
```

Output:

```text
8 frames in 496 bytes
8 frames back as they were, plays 0 (0: forever)
```

## See also

- [options](../webp-options.md): lossless, the quality, the loop count
- [decode](decode.md), [frames](frames.md): the other way
- [save](../save.md): an image into a file, in the format its extension names
- [gif::encode](../gif/encode.md): the other animated format
- [sgcl::codec::webp](README.md)
