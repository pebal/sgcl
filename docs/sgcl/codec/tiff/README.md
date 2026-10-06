[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::tiff

```cpp
#include "sgcl/codec/tiff.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class tiff;
}
```

`sgcl::codec::tiff` reads and writes TIFF: baseline TIFF 6.0 and its common extensions, little- and big-endian,
strips and tiles, several pages. [decode](decode.md) gives the first page, [decode_all](decode_all.md) every page,
[encode](encode.md) writes one image or several, uncompressed, with LZW or with Deflate as its
[options](../tiff-options.md) ask. Every member is static. [codec::decode](../decode.md) reads TIFF too, told by `II`
or `MM` and 42, and [save](../save.md) writes it, with LZW, for `.tif` and `.tiff`. Go's `x/image/tiff` reads much of
the same and writes uncompressed or Deflate; libtiff, the reference, is the oracle of the module's benchmarks.

## Rules

- **What is read.** Compression none, PackBits, LZW (TIFF's, with its early change), Deflate (both codes) and JPEG
  (through the module's JPEG decoder, JPEGTables in front of each strip); the horizontal predictor; bilevel and gray
  of 1, 2, 4, 8 and 16 bits (WhiteIsZero inverted), palette, RGB and CMYK of 8 and 16 bits; an extra sample as
  alpha when ExtraSamples says so (associated alpha divided out); chunky or planar. Floating-point samples, old-style
  JPEG, CCITT, LAB and uncompressed YCbCr are `errc::unsupported`; BigTIFF too.
- **The image** is `gray8`, `gray16`, `gray_alpha8`, `gray_alpha16`, `rgb8`, `rgb16`, `rgba8`, `rgba16` or `cmyk8` (CMYK
  as ink, as JPEG's), a palette `rgb8`, or the format of [decode_options](../decode_options.md)`::want`. The
  orientation tag and the ICC profile come with it.
- **A stream** is read to its end first: TIFF's directories point anywhere in the file. A file is made in memory and
  written at once.
- **Encoding** writes every pixel format as it is (CMYK as `Separated`, alpha as unassociated), in strips of about
  64 KB, the image's orientation and ICC profile with it.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md). A chain of
  directories that loops is `errc::corrupt`.

## Member types

| Type | Definition |
|---|---|
| [compression](../tiff-compression.md) | none, LZW or Deflate |
| [options](../tiff-options.md) | the compression `encode` writes with |

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the first page of a TIFF file (static) |
| [decode_all](decode_all.md) | every page of a TIFF file (static) |
| [encode](encode.md) | a TIFF of one image or several (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(200, 100, codec::pixel_format::rgb16);
    vector<byte> lzw = codec::tiff::encode(picture);
    vector<byte> none = codec::tiff::encode(picture,
            {.compression = codec::tiff::compression::none});
    println("LZW {} bytes, none {} bytes", lzw.size(), none.size());
    codec::image back = codec::tiff::decode(lzw);
    println("rgb16 again: {}", back.format() == codec::pixel_format::rgb16);
}
```

Output:

```text
LZW 988 bytes, none 120168 bytes
rgb16 again: true
```

## See also

- [compression](../tiff-compression.md), [options](../tiff-options.md)
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [image](../image/README.md), [error](../error/README.md)
