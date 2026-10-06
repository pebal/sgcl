[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::pnm

```cpp
#include "sgcl/codec/pnm.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class pnm;
}
```

`sgcl::codec::pnm` reads and writes Netpbm's formats: PBM, PGM and PPM, plain (P1, P2, P3: numbers in text) and raw
(P4, P5, P6), and PAM (P7, any channels with a header of named fields). [decode](decode.md) gives the image of a
file, [encode](encode.md) writes any [image](../image/README.md) in the [kind](../pnm-kind.md) its
[options](../pnm-options.md) ask. Every member is static. [codec::decode](../decode.md) reads them too, told by `P`
and a digit 1 to 7, and [save](../save.md) writes them for `.pbm`, `.pgm`, `.ppm`, `.pam` and `.pnm`.

## Rules

- **The samples.** A maxval of 255 or 65535 gives the samples as they are, 8 or 16 bits; another maxval is scaled to
  the full range of 8 bits (up to 255) or 16 (above), to the nearest; a sample past maxval is taken as maxval.
- **Decoding** gives `gray8` for PBM (1 black), gray, gray with alpha, RGB or RGBA by the file's channels, or the
  format of [decode_options](../decode_options.md)`::want`. Comments (`#` to the end of the line) are read past.
- **Encoding** writes PGM for gray, PPM for color and PAM for an image with alpha unless told the kind, maxval 255 or
  65535 by the image's depth; PBM is black where the gray is below half.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md).

## Member types

| Type | Definition |
|---|---|
| [kind](../pnm-kind.md) | which format `encode` writes |
| [options](../pnm-options.md) | the kind and the plain form |

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the image of a Netpbm file, from its bytes or a stream (static) |
| [encode](encode.md) | a Netpbm file of an image, as bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image gray(4, 2, codec::pixel_format::gray8);
    for (int i : range(8)) {
        gray.pixels()[i] = byte(i * 32);
    }
    vector<byte> pgm = codec::pnm::encode(gray, {.plain = true});
    println("{}", string(reinterpret_cast<const char*>(pgm.data()), pgm.size()));
    codec::image back = codec::pnm::decode(pgm);
    println("the same pixels: {}", back.pixels() == gray.pixels());
}
```

Output:

```text
P2
4 2
255
0 32 64 96
128 160 192 224

the same pixels: true
```

## See also

- [kind](../pnm-kind.md), [options](../pnm-options.md)
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [image](../image/README.md), [error](../error/README.md)
