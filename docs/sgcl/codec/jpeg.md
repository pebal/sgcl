[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::jpeg

```cpp
#include "sgcl/codec/jpeg.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class jpeg;
}
```

`sgcl::codec::jpeg` reads and writes JPEG as T.81 has it, with JFIF 1.02 and Adobe's APP14.
[decode](jpeg/decode.md) reads the DCT modes with Huffman coding of 8-bit samples: baseline, extended sequential and
progressive, successive approximation included. [encode](jpeg/encode.md) writes baseline JPEG at the quality and the
chroma [subsampling](jpeg-subsampling.md) of its [options](jpeg-options.md). Every member is static.
[codec::decode](decode.md) reads a JPEG too, told by its signature.

Both sides are libjpeg-turbo's to the bit: the pixels decoded are those of `djpeg -dct int`, its integer IDCT, its
"fancy" upsampling and its YCbCr conversion; the file encoded is, byte for byte, what `cjpeg -dct int -baseline`
writes with the same quality, sampling and `-optimize`.

## Rules

- **No block smoothing.** libjpeg-turbo smooths the blocks of a progressive file whose progression stops before
  its low coefficients are whole; the module does not: such a file's pixels are libjpeg's with smoothing off.
- **Refused.** Arithmetic coding, 12-bit samples, lossless and hierarchical files and DNL (the height given after
  the scan) are `errc::unsupported`.
- **Strict where libjpeg only warns.** Data that ends early is `errc::unexpected_end`; a scan cut by a marker
  before its MCUs end and a restart marker out of turn are `errc::corrupt`.
- **Lenient where libjpeg is.** Bytes between the end of a scan and the next marker, and a restart marker outside
  a scan, are passed over.
- **Metadata.** EXIF (APP1) gives the image's [orientation](image/orientation.md), and the image is not turned;
  it is kept as bytes in [exif](image/exif.md), with the ICC profile in [icc](image/icc.md), its APP2 chunks
  joined in their order. `encode` writes an image's own as APP1 and APP2.
- **Errors are values**: an [expected](../core/expected.md) with an [error](error.md), whose code is an
  [errc](errc.md). A program that takes the image directly, `codec::image photo = codec::jpeg::decode(file);`, gets
  a [bad_expected_access](../core/bad_expected_access.md) thrown on an error. What throws is a quality outside 1 to
  100 given to `encode`, a contract of the program (`invalid_argument`).

## Member types

| Type | Definition |
|---|---|
| [options](jpeg-options.md) | the encoder's settings: the quality, the subsampling, optimized tables |
| [subsampling](jpeg-subsampling.md) | the resolution of the chrominance: whole, half across, half both ways |

## Member functions

| Function | Description |
|---|---|
| [decode](jpeg/decode.md) | the image of a JPEG file, from its bytes or from a stream (static) |
| [encode](jpeg/encode.md) | a baseline JPEG of an image, as bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a smooth color gradient, 96 × 64
    codec::image photo(96, 64, codec::pixel_format::rgb8);
    for (int y : range(64)) {
        slice<byte> row = photo.row(y);
        for (int x : range(96)) {
            row[3 * x] = byte(x * 2);
            row[3 * x + 1] = byte(y * 3);
            row[3 * x + 2] = byte(255 - x - y);
        }
    }
    vector<byte> file = codec::jpeg::encode(photo);
    codec::image back = codec::jpeg::decode(file);
    println("quality 85: {} bytes; {}x{}", file.size(), back.width(), back.height());

    vector<byte> finer = codec::jpeg::encode(photo, {.quality = 95});
    println("quality 95: {} bytes", finer.size());
}
```

Output:

```text
quality 85: 1165 bytes; 96x64
quality 95: 1527 bytes
```

## See also

- [png](png.md): the lossless format
- [heif](heif.md): HEIC, through the system's codec
- [decode](decode.md), [save](save.md): any format, told by the signature or by the extension
- [image](image.md), [error](error.md)
