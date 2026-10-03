[sgcl](../../README.md) › [codec](../README.md) › [heif](../heif.md)

# sgcl::codec::heif::encode

```cpp
static expected<vector<byte>, error> encode(const image& im) noexcept;                      // (1)
static expected<vector<byte>, error> encode(const image& im, const options& o) noexcept;    // (2)
static expected<void, error> encode(const image& im, const io::writer& out);                // (3)
static expected<void, error> encode(const image& im, const io::writer& out,                 // (4)
                                    const options& o);
```

Encodes the image as HEIC through the system's encoder, ImageIO on macOS, with its ICC profile and orientation; on
other systems it is `errc::unsupported`.

- (1–2) Returns the bytes of the file.
- (3–4) Writes the file into `out` as ImageIO produces the bytes, without holding the whole file.
- (1, 3) Encode with the default [options](../heif-options.md), at a quality of 85.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode, of any pixel format |
| `out` | the stream the file is written to |
| `o` | the quality of the encoding |

## Return value

- (1–2) The bytes of the file, or the error.
- (3–4) Nothing, or the error; `errc::io` when the stream fails, at the offset of the bytes written before.

The errors: `errc::invalid_argument` for a quality outside 1..100 or an image CoreGraphics does not take;
`errc::unsupported` on a system without the codec, where the system has no HEIC encoder (some virtual machines) and
when its encoder does not write the image.

## Complexity

Linear in the pixels of the image, as the system's encoder is.

## Exceptions

- (1–2) None.
- (3–4) What the stream's `write` throws: `out` calls the `write` of the object it is bound to. The write is called
  from inside the system's encoder, which an exception must not cross: it is caught there, the encoding stopped, and
  thrown again when the encoder has returned.

## Notes

The image's ICC profile goes with it when it is a profile of the image's color model, gray or RGB; otherwise the
image is written in sRGB, a gray one in gray of gamma 2.2. The orientation is written as the image has it; the
pixels are not turned. 16-bit images are written as the system's encoder writes them (10 bits). A CMYK image goes
through RGB, its profile left behind, and a `gray_alpha16` image through `rgba16`, its gray profile left behind:
the system's encoder writes 16-bit gray and 8-bit gray with alpha, not the two together. AVIF is not written in 1.0.0; writing it is an addition planned for 1.x.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::cmyk8);
    vector<byte> file = codec::heif::encode(picture);
    println("HEIC: {}", codec::sniff(file) == codec::format::heif);

    io::buffer out;
    expected<void, codec::error> written = codec::heif::encode(picture, out, {.quality = 60});
    codec::image back = codec::heif::decode(out);
    println("written: {}, read back: {}x{}, rgb8: {}", written.has_value(), back.width(),
            back.height(), back.format() == codec::pixel_format::rgb8);
}
```

Output:

```text
HEIC: true
written: true, read back: 64x48, rgb8: true
```

## See also

- [options](../heif-options.md): the quality
- [decode](decode.md): the image of a HEIC
- [png::encode](../png/encode.md), [jpeg::encode](../jpeg/encode.md): the formats the module writes itself
- [sgcl::codec::heif](../heif.md)
