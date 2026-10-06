[sgcl](../../README.md) › [codec](../README.md) › [bmp](README.md)

# sgcl::codec::bmp::encode

```cpp
static expected<vector<byte>, error> encode(const image& im) noexcept;          // (1)
static expected<void, error> encode(const image& im, const io::writer& out);    // (2)
```

Encodes an image as a BMP file, its rows bottom-up as every reader takes them.

1. Returns the file as bytes.
2. Writes the file into a stream.

- `gray8` is written at 8 bits through a palette of the 256 grays (a BITMAPINFOHEADER).
- An image with alpha is written at 32 bits, BI_BITFIELDS under a V4 header with the masks of BGRA, which Windows,
  browsers, Go and ffmpeg read alpha from.
- Any other is written at 24 bits (a BITMAPINFOHEADER): `gray16` and the colors without alpha, 16-bit channels
  narrowed to 8, `cmyk8` through [convert](../image/convert.md)'s conversion.

The resolution written is 72 dots an inch (2835 a meter); no ICC profile is written.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `out` | the stream the file is written into |

## Return value

1. The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for an image whose pixels
   pass the 4 GB that BMP's 32-bit sizes hold.
2. Nothing, or the error: `errc::invalid_argument` as (1), before anything is written; `errc::io` when the stream
   fails, at the offset of the bytes written before, the stream's own error in [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The memory is a row converted, made once; (1) adds the file, (2) writes a row at a time.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("gray8 {} bytes", codec::bmp::encode(codec::image(4, 4,
            codec::pixel_format::gray8))->size());
    println("rgb8 {} bytes", codec::bmp::encode(codec::image(4, 4,
            codec::pixel_format::rgb8))->size());
    println("rgba8 {} bytes", codec::bmp::encode(codec::image(4, 4,
            codec::pixel_format::rgba8))->size());

    io::buffer out;
    expected<void, codec::error> written = codec::bmp::encode(codec::image(4, 4,
            codec::pixel_format::rgb8), out);
    println("into the stream {}, {} bytes", written.has_value(), out.size());
}
```

Output:

```text
gray8 1094 bytes
rgb8 102 bytes
rgba8 186 bytes
into the stream true, 102 bytes
```

## See also

- [decode](decode.md): the image of a BMP file
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::bmp](README.md)
