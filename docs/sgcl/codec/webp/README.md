[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::webp

```cpp
#include "sgcl/codec/webp.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class webp;
}
```

`sgcl::codec::webp` reads WebP: the container of RFC 9649, simple and extended (VP8X, ICCP, EXIF, ANIM and ANMF),
lossless images (VP8L) and lossy ones (VP8, RFC 6386, with their alpha in an ALPH chunk: raw or lossless, and each of
its filters). XMP is passed over. [decode](decode.md) gives a still image, or an animation's first frame on its
canvas, as an [image](../image/README.md); [frames](frames.md) gives every frame, read one by one as the program asks for
them. Every member is static, and there is no encoder. [codec::decode](../decode.md) and
[codec::decode_frames](../decode_frames.md) read WebP too, told by its signature.

The decoder is the module's own and gives the pixels of libwebp, the decoder of Chrome; Go's `x/image/webp` gives the
same for a lossless file. An animation's canvas is composed here too, by RFC 9649's rules
([frames](frames.md)), each frame the whole canvas as it is shown.

## Rules

- **The pixels are libwebp's.** Every lossless file of libwebp-test-data decodes to the pixels libwebp gives, byte for
  byte, and to what Go's `x/image/webp` gives. Predictor modes 14 and 15, which RFC 9649 does not name, predict as
  mode 0 in both.
- **Lossy WebP decodes pixel for pixel as libwebp (Chrome).** Every lossy file of libwebp-test-data, 88 of them, gives
  libwebp's RGBA byte for byte and the MD5s dwebp's `-pam` and `-pgm` are listed with.
- **The planes.** Y, U and V, VP8's own output (RFC 6386: key frames, the four token partitions, segments, both loop
  filters), are libwebp's and Go's bit for bit. Where RFC 6386's reference decoder reads valid data otherwise, the
  planes follow libwebp and Go: a macroblock's filter level is clamped to 0..63 once, after the deltas, and a key
  frame with segmentation on but no segment data gives its segments a quantizer index and a filter level of 0, the
  frame's deltas added to them (the reference decoder keeps the frame's own index and level).
- **The colors.** RFC 6386 stops at YUV. The chroma is brought to full size by the "fancy" upsampling, derived from
  the bilinear definition, not from the RFC (neither RFC 6386 nor RFC 9649 defines it): the weights 9, 3, 3 and 1 over
  16, rounded to the nearest. The colors are BT.601's in its limited range, in 14-bit fixed point, derived from the
  standard. Two of the rounding constants are calibrated to the reference decoder's output, since the standard leaves
  them open. `sgcl/codec/detail/vp8_yuv.h` gives the derivation.
- **Alpha.** An ALPH chunk is applied whatever VP8X's alpha flag says. For a still image the module follows libwebp's
  `WebPDecode` (`WebPDecodeRGBA`, dwebp), which applies it; its `WebPAnimDecoder` leaves such a still opaque. The
  native format then is `rgb8`, as the flag says. An animation's frames follow `WebPAnimDecoder`, which applies an
  ALPH chunk, flag or not. An ALPH chunk as the first chunk of the simple format (alpha with no VP8X) is refused, as
  libwebp refuses it.
- **Damaged data is refused.** A cut or damaged partition is refused: one read past its end (`errc::unexpected_end`),
  or one whose first byte no encoder writes (`errc::corrupt`). So is an alpha stream read past the end of its ALPH
  chunk. RFC 6386 and RFC 9649 make such data invalid; browsers show whatever the reference decoder reads past the
  end. One case has no single answer: coefficients beyond what an encoder produces (past 2048 after dequantization).
  There libwebp's C, NEON and SSE2 code and Go each give different pixels. The module takes such a file, and keeps
  the transforms' intermediate values whole, so its pixels are its own.
- **The container as libwebp takes it.** What the module accepts and refuses is what libwebp's demuxer accepts and
  refuses; tests and the fuzzer hold the two together. That goes beyond RFC 9649 in places: after the simple
  format's image, chunks are read up to the first one that is not ALPH, and the rest is passed over, as are bytes
  after the RIFF size; VP8X must be first and exactly 10 bytes; a frame ends at the first chunk after its ANMF header
  that is not ALPH or its image, and what follows is read at the top level; ALPH with a lossless image is refused, an
  ALPH chunk comes right before its VP8 image, and a file of the simple format begins with its image.
- **What decoding checks.** Data that ends before the RIFF size or before the image is `errc::unexpected_end`. A
  chunk past the RIFF size, a prefix code that is not a whole tree, a transform used twice, color cache bits outside
  1..11, a back reference outside the image, a frame outside the canvas and a still image of another size than its
  canvas are `errc::corrupt`. So are a VP8 frame that is not a key frame, of a version past 3, not shown, with another
  start code or a first partition past its chunk, and an ALPH header of values it does not define. VP8 data that ends
  before its last macroblock, and alpha that ends before its picture, are `errc::unexpected_end`; a VP8 partition
  beginning with the byte 0xFF is `errc::corrupt`. The [limits](../limits.md) (`max_pixels` for the canvas,
  `max_metadata` for each metadata chunk) give `errc::too_large`.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md), whose code is an
  [errc](../errc.md). A program that takes the image directly, `codec::image photo = codec::webp::decode(file);`, gets a
  [bad_expected_access](../../core/bad_expected_access/README.md) thrown on an error.
- **Memory.** The image, and a buffer of four bytes a pixel for the lossless decoder's words, which back references
  point into. A lossy image adds its planes of Y, U and V (a byte and a half a pixel) and its alpha; its VP8 chunk is
  read whole from a stream, since its partitions are read side by side. From a stream, the file is read a block at a
  time and the bitstream is never copied whole. The decoder's buffers are kept from one frame to the next: after the
  first frame, nothing is allocated per frame but the image handed out.
- **SIMD.** The lossless decoder's inverse transforms, the lossy one's upsampling and conversion of colors, and the
  conversion of words to RGBA use 128-bit vectors: NEON on arm64, SSE2 on x86-64, which is what the compiler assumes
  without a flag. `SGCL_CODEC_PORTABLE` builds the plain loops instead, and the tests run both.

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | a still image, or an animation's first frame on its canvas (static) |
| [frames](frames.md) | every frame, read one by one; a still image is one frame (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> still = io::read_file("tests/codec/fuzz/seeds/webp_decode/lossless4.webp");
    codec::image photo = codec::webp::decode(still);
    println("lossless: {}x{}, {} bytes a row", photo.width(), photo.height(), photo.stride());

    vector<byte> lossy = io::read_file("tests/codec/fuzz/seeds/webp_decode/test.webp");
    codec::image picture = codec::webp::decode(lossy);
    println("lossy: {}x{}, {} bytes a row (rgb8: no alpha)", picture.width(), picture.height(),
            picture.stride());

    vector<byte> moving = io::read_file("tests/codec/fuzz/seeds/webp_decode/made_animation.webp");
    codec::frames clip = codec::webp::frames(moving);
    int count = 0;
    while (optional<codec::frame> shown = clip.next()) {
        ++count;
    }
    println("animation: {}x{}, {} frames", clip.width(), clip.height(), count);
}
```

Output:

```text
lossless: 256x256, 1024 bytes a row
lossy: 128x128, 384 bytes a row (rgb8: no alpha)
animation: 16x12, 4 frames
```

## See also

- [frames](../frames/README.md), [frame](../frame.md): an animation read frame by frame
- [decode](../decode.md), [decode_frames](../decode_frames.md): any format, told by its signature
- [gif](../gif/README.md): the other animated format
- [image](../image/README.md), [error](../error/README.md)
