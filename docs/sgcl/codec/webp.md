# sgcl::codec::webp

```cpp
#include "sgcl/codec/webp.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    class webp {
    public:
        // a still image, or an animation's first frame on its canvas
        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {});

        // every frame, read one by one
        static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {});
        static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {});
    };
}
```

`codec::webp::decode(bytes)` reads a file with the defaults: the file's own pixel format (below), at most 100 million pixels and 64 MB of metadata, with EXIF and ICC read. [`decode_options`](decode.md) set anything else: the pixel format wanted, the limits, the metadata. [`codec::decode`](decode.md) reads WebP too, told by its signature.

**What it reads.** The container of RFC 9649, simple and extended (VP8X, ICCP, EXIF, ANIM and ANMF), lossless images (VP8L) and lossy ones (VP8, RFC 6386, with their alpha in an ALPH chunk: raw or lossless, and each of its filters). XMP is passed over.

**The pixel format.** A still image comes as `rgba8` when the file says it has alpha (VP8X's flag, or VP8L's `alpha_is_used` in the simple format) and as `rgb8` when it does not; a lossy image of the simple format is `rgb8`. An animation's frames come as `rgba8`. `decode_options::want` asks for another format, each row converted as it is decoded.

**The pixels are libwebp's.** Every lossless file of libwebp-test-data decodes to the pixels libwebp gives, byte for byte, and to what Go's `x/image/webp` gives. Predictor modes 14 and 15, which RFC 9649 does not name, predict as mode 0 in both.

**Lossy WebP decodes pixel for pixel as libwebp (Chrome).** Every lossy file of libwebp-test-data, 88 of them, gives libwebp's RGBA byte for byte and the MD5s dwebp's `-pam` and `-pgm` are listed with.
- **The planes.** Y, U and V, VP8's own output (RFC 6386: key frames, the four token partitions, segments, both loop filters), are libwebp's and Go's bit for bit.
- **The colors.** RFC 6386 stops at YUV. The chroma is brought to full size by the "fancy" upsampling, derived from the bilinear definition, not from the RFC (neither RFC 6386 nor RFC 9649 defines it): the weights 9, 3, 3 and 1 over 16, rounded to the nearest. The colors are BT.601's in its limited range, in 14-bit fixed point, derived from the standard. Two of the rounding constants are calibrated to the reference decoder's output, since the standard leaves them open. `detail/vp8_yuv.h` gives the derivation.
- **Alpha.** An ALPH chunk is applied whatever VP8X's alpha flag says. For a still image the module follows libwebp's `WebPDecode` (`WebPDecodeRGBA`, dwebp), which applies it; its `WebPAnimDecoder` leaves such a still opaque. The native format then is `rgb8`, as the flag says. An animation's frames follow `WebPAnimDecoder`, which applies an ALPH chunk flag or not.
- **ALPH first.** An ALPH chunk as the first chunk of the simple format (alpha with no VP8X) is refused, as libwebp refuses it.
- **Damaged data is refused.** A cut or damaged partition is refused: one read past its end (`errc::unexpected_end`), or one whose first byte no encoder writes (`errc::corrupt`). So is an alpha stream read past the end of its ALPH chunk. RFC 6386 and RFC 9649 make such data invalid; browsers show whatever the reference decoder reads past the end. One case has no single answer: coefficients beyond what an encoder produces (past 2048 after dequantization). There libwebp's C, NEON and SSE2 code and Go each give different pixels. The module takes such a file, and keeps the transforms' intermediate values whole, so its pixels are its own.

**Frames.** Each frame is the whole canvas as it is shown, drawn by RFC 9649's rules. The canvas starts transparent; the ANIM chunk's background color is a hint RFC 9649 lets a reader leave, and libwebp's `WebPAnimDecoder` and browsers leave it too. A frame disposed of is cleared to transparent before the next one. A frame that blends is alpha-blended by RFC 9649's formula, in integers rounded to the nearest: A = src.A + dst.A × (255 − src.A) / 255, and each color (src.C × src.A + dst.C × dst.A × (255 − src.A) / 255) / A, 0 when A is 0. A frame that does not blend is written over its rectangle.

- **The composition of the canvas is the module's.** The frames' own pixels are libwebp's bit for bit, but a blended pixel may differ from `WebPAnimDecoder`'s. libwebp blends in fixed point: one off in alpha, and off in a color by up to about 255 / A for a pixel of alpha A. That is one or two for most pixels, and more only where the pixel is nearly transparent: its error in the background's weight is divided by A. The tests hold a blended pixel to alpha ± 1 and color ± (1 + 255 / A) against libwebp; matching it exactly would mean taking its approximation for the formula.
- **The loop count** is ANIM's: 0 forever, `n` plays for `n`. `loop_count()` is known after the first frame is read.
- **The delay** is the frame's duration in milliseconds, 0 left as 0.

`decode` gives a still image, or an animation's first frame on its canvas; [`frames`](frames.md) gives every frame (a still image is one frame).

**The container as libwebp takes it.** What the module accepts and refuses is what libwebp's demuxer accepts and refuses; tests and the fuzzer hold the two together. That goes beyond RFC 9649 in places:

- after the simple format's image, chunks are read up to the first one that is not ALPH, and the rest is passed over, as are bytes after the RIFF size;
- VP8X must be first and exactly 10 bytes;
- a frame ends at the first chunk after its ANMF header that is not ALPH or its image, and what follows is read at the top level;
- ALPH with a lossless image is refused; an ALPH chunk comes right before its VP8 image, and a file of the simple format begins with its image.

**What decoding checks.** Data that ends before the RIFF size or before the image is `errc::unexpected_end`. A chunk past the RIFF size, a prefix code that is not a whole tree, a transform used twice, color cache bits outside 1..11, a back reference outside the image, a frame outside the canvas and a still image of another size than its canvas are `errc::corrupt`. So are a VP8 frame that is not a key frame, of a version past 3, not shown, with another start code or a first partition past its chunk, and an ALPH header of values it does not define. VP8 data that ends before its last macroblock, and alpha that ends before its picture, are `errc::unexpected_end`; a VP8 partition beginning with the byte 0xFF is `errc::corrupt`. The [limits](README.md#limits) (`max_pixels` for the canvas, `max_metadata` for each metadata chunk) give `errc::too_large`.

**Memory.** The image, and a buffer of four bytes a pixel for the lossless decoder's words, which back references point into. A lossy image adds its planes of Y, U and V (a byte and a half a pixel) and its alpha; its VP8 chunk is read whole from a stream, since its partitions are read side by side. From a stream, the file is read a block at a time and the bitstream is never copied whole. The decoder's buffers are kept from one frame to the next: after the first frame, nothing is allocated per frame but the image handed out.

**SIMD.** The lossless decoder's inverse transforms, the lossy one's upsampling and conversion of colors, and the conversion of words to RGBA use 128-bit vectors: NEON on arm64, SSE2 on x86-64, which is what the compiler assumes without a flag. `SGCL_CODEC_PORTABLE` builds the plain loops instead, and the tests run both.

## Members

### decode

```cpp
static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});
```

A still image, or an animation's first frame on its canvas, from the file's bytes or from a stream.

### frames

```cpp
static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {});
static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {});
```

Every frame, read one by one as [`frames`](frames.md) asks for them; a still image is one frame.

## Examples

### Still images

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> still = io::read_file("tests/codec/fuzz/seeds/webp_decode/lossless4.webp");
    codec::image photo = codec::webp::decode(still);
    println("still: {}x{}, {} bytes a row", photo.width(), photo.height(), photo.stride());

    vector<byte> lossy = io::read_file("tests/codec/fuzz/seeds/webp_decode/test.webp");
    codec::image picture = codec::webp::decode(lossy);
    println("lossy: {}x{}, {} bytes a row (rgb8: no alpha)", picture.width(), picture.height(),
            picture.stride());
}
```

Output:

```text
still: 256x256, 1024 bytes a row
lossy: 128x128, 384 bytes a row (rgb8: no alpha)
```

### An animation

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

#include <chrono>

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/webp_decode/made_animation.webp");
    codec::frames clip = codec::webp::frames(file);
    int count = 0;
    std::chrono::milliseconds total{0};
    while (optional<codec::frame> shown = clip.next()) {
        total += std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::nanoseconds(shown->delay));
        ++count;
    }
    println("animation: {}x{}, {} frames, {} ms, plays {}", clip.width(), clip.height(), count,
            total.count(), clip.loop_count());
}
```

Output:

```text
animation: 16x12, 4 frames, 160 ms, plays 3
```

### Options

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> still = io::read_file("tests/codec/fuzz/seeds/webp_decode/lossless4.webp");
    codec::image small = codec::webp::decode(still, {.want = codec::pixel_format::gray8});
    println("as gray8: {} bytes a row", small.stride());
    auto refused = codec::webp::decode(still, {.limits = {.max_pixels = 1000}});
    if (!refused) println(refused.error().message());
}
```

Output:

```text
as gray8: 256 bytes a row
offset 21: size limit exceeded
```

## See also

[`frames`](frames.md), [`image`](image.md), [`decode`](decode.md), [`error`](error.md).
