[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::jxl

```cpp
#include "sgcl/codec/jxl.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class jxl;
}
```

`sgcl::codec::jxl` reads JPEG XL (ISO/IEC 18181), through the system's codec as [heif](../heif/README.md) reads
HEIF: one line, `codec::image picture = codec::jxl::decode(bytes);`, gives the first frame of a file. The module has
no decoder of its own and writes no JPEG XL. Every member is static. [codec::decode](../decode.md) reads JPEG XL too,
told by its signature (a bare codestream or the container), its [decode_options](../decode_options.md) setting
anything else, and [metadata](../metadata/README.md) reads the EXIF and XMP of the container. Go's standard library
reads no JPEG XL.

## Rules

- **Through the system's codec.** On macOS (14 and later) the calls go to ImageIO, which decodes with libjxl. On
  other systems every call is `errc::unsupported`.
- **What it reads.** Lossless and lossy files, bare codestreams and the container, a JPEG recompressed without loss:
  the first frame of an animation, in the file's own pixel format (gray or RGB, alpha when it has any, 16 bits when
  its samples have more than 8), its orientation and color profile with it.
- **Gray with alpha** is refused where the system decodes it wrong: macOS 26's ImageIO gives such a file's pixels as
  0 and 255 whatever it holds, and draws it transparent. The module asks the system once, on a file of two known
  pixels, and gives `errc::unsupported` for gray with alpha where the answer is wrong; a system that reads them
  right is taken as it is.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md), whose code is
  an [errc](../errc.md). On a system without the codec every call is an error.
- **Linking** is heif's: ImageIO, CoreGraphics, CoreFoundation and Accelerate on Apple's systems, no header of them
  included by the library.
- **Tested** against libjxl's own tools: files that `cjxl` makes without loss from PngSuite (gray, RGB and RGBA of 8
  and 16 bits) decode to the source's pixels and to `djxl`'s exactly; lossy ones within 2 of `djxl`'s (two builds of
  libjxl); an animated GIF's first frame exactly; the wrapping is fuzzed.

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the first frame of a JPEG XL file (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/jxl_decode/basn2c16.jxl");
    println("JPEG XL: {}", codec::sniff(file) == codec::format::jxl);
    expected<codec::image, codec::error> picture = codec::jxl::decode(file);
    if (picture) {
        println("{}x{}, rgb16: {}", picture->width(), picture->height(),
                picture->format() == codec::pixel_format::rgb16);
    } else {
        println("{}", picture.error().message());
    }
}
```

Output:

```text
JPEG XL: true
32x32, rgb16: true
```

## See also

- [heif](../heif/README.md): HEIC, HEIF and AVIF through the same codec
- [decode](../decode.md), [sniff](../sniff.md): any format, told by its signature
- [metadata](../metadata/README.md): the EXIF and XMP of the container
- [image](../image/README.md), [error](../error/README.md)
