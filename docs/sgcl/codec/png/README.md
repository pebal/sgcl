[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::png

```cpp
#include "sgcl/codec/png.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class png;
}
```

`sgcl::codec::png` reads and writes PNG as the W3C's third edition has it: every color type and bit depth, Adam7
interlacing and tRNS transparency, with the CRC-32 of every chunk and the Adler-32 of the image data checked.
[decode](decode.md) gives the image of a file, in the file's own pixel format unless asked for another;
[encode](encode.md) writes any [image](../image/README.md) as the PNG type that holds its format, its rows filtered as
libpng filters them, at the DEFLATE level of its [options](../png-options.md). Every member is static.
[codec::decode](../decode.md) reads a PNG too, told by its signature.

The pixels decoded are libpng's and Go's, pixel for pixel. As in Go's `image/png`, gamma and color are not managed:
gAMA, cHRM and sRGB are read past, never applied. The metadata is kept: EXIF and the ICC profile of the file come
with the image, and an image written takes its own with it.

## Rules

- **Strict where the format is.** A critical chunk the decoder does not know, or one out of place, a bad CRC-32 or
  Adler-32, image data that ends short and a missing IEND are errors.
- **Lenient where libpng is.** An ancillary chunk it does not know, or one out of place (a tRNS of the wrong size,
  or in an image with an alpha channel), is passed over; a palette index past the palette is opaque black; the
  bytes of the zlib stream past the image are ignored, and a stream whose rows are all there but whose end never
  comes is taken, its Adler-32 then unchecked.
- **Metadata.** eXIf and iCCP are read into the image's [exif](../image/exif.md) and [icc](../image/icc.md) as bytes (an
  ICC profile that does not decompress is dropped, as libpng drops it), the [orientation](../image/orientation.md)
  taken from the EXIF; `encode` writes an image's own as eXIf and iCCP. Text, time, background and sBIT are not
  read, and of an APNG only the default image is.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md), whose code is an
  [errc](../errc.md). A program that takes the image directly, `codec::image picture = codec::png::decode(file);`,
  gets a [bad_expected_access](../../core/bad_expected_access/README.md) thrown on an error.

## Member types

| Type | Definition |
|---|---|
| [options](../png-options.md) | the encoder's settings: the DEFLATE level |

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the image of a PNG file, from its bytes or from a stream (static) |
| [encode](encode.md) | a PNG of an image, as bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a gradient, 64 × 32, with a transparent left half
    codec::image picture(64, 32, codec::pixel_format::rgba8);
    for (int y : range(32)) {
        slice<byte> row = picture.row(y);
        for (int x : range(64)) {
            row[4 * x] = byte(x * 4);
            row[4 * x + 1] = byte(y * 8);
            row[4 * x + 2] = byte(128);
            row[4 * x + 3] = byte(x < 32 ? 0 : 255);
        }
    }
    vector<byte> file = codec::png::encode(picture);
    codec::image back = codec::png::decode(file);
    println("{} bytes; {}x{}", file.size(), back.width(), back.height());
    println("the same pixels: {}", back.pixels() == picture.pixels());
}
```

Output:

```text
131 bytes; 64x32
the same pixels: true
```

## See also

- [jpeg](../jpeg/README.md): the lossy format for photographs
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [compress::zlib](../../compress/zlib/README.md): the stream PNG's image data is written and read through
- [image](../image/README.md), [error](../error/README.md)
