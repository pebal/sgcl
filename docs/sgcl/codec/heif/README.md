[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::heif

```cpp
#include "sgcl/codec/heif.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class heif;
}
```

`sgcl::codec::heif` reads HEIC, HEIF and AVIF photos and writes HEIC, through the system's codec. One line reads a
HEIC photo, `codec::image photo = codec::heif::decode(bytes);`, and one writes one, `codec::heif::encode(photo)`; the
quality of the encoding is an [option](../heif-options.md), `codec::heif::encode(photo, out, {.quality = 60})`. Every
member is static. [codec::decode](../decode.md) reads HEIF and AVIF too, told by their signature, its
[decode_options](../decode_options.md) setting anything else.

HEIC holds HEVC and AVIF holds AV1: video codecs, which the module takes from the platform as it would take video,
and does not write itself. Go's standard library reads neither.

## Rules

- **Through the system's codec.** On macOS the calls go to ImageIO. On other systems every function is
  `errc::unsupported` (Windows through WIC belongs to the platform step of 1.0.0). An encoder may be missing even on
  macOS (some virtual machines): [encode](encode.md) is then `errc::unsupported`, which is why it returns an
  `expected` where PNG's and JPEG's return the bytes.
- **What it reads.** HEIF and HEIC still images, and AVIF where the system reads it (macOS 13 and later): the first
  image of the file. Sequences, thumbnails and auxiliary images (depth, alpha planes of their own) are not read as
  such.
- **A file the system's decoder hangs on is refused.** ImageIO hands HEVC to VideoToolbox, which waits for ever,
  without an error and without using the processor (seen on macOS 26.5), on an HEVC slice whose entry points (where
  its tiles or its rows of wavefront coding start) lie at or past the end of the slice's data. Before ImageIO sees
  a file, the module reads its boxes and, of every HEVC item, the parameter sets and the slice headers, and refuses
  such a file with `errc::corrupt`. What it cannot read (a P or B slice, an extension of HEVC it does not parse) is
  left to ImageIO, as before; AVIF is not checked, no such hang of AV1 being known. The check is a reading of the
  boxes and the headers, linear in the file: no thread, no time limit. Items whose data overlap past four times the
  file's size, which no encoder writes, are `errc::unsupported`. This is the Apple path's behaviour; elsewhere every
  call is `errc::unsupported` anyway.
- **What it writes.** HEIC. AVIF is not written in 1.0.0; writing it is an addition planned for 1.x.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md), whose code is an
  [errc](../errc.md). A program that takes the image directly, `codec::image photo = codec::heif::decode(file);`, gets a
  [bad_expected_access](../../core/bad_expected_access/README.md) thrown on an error, and on a system without the codec on every
  call.
- **Linking.** On Apple's systems the library links ImageIO, CoreGraphics, CoreFoundation and Accelerate (CMake does
  it). A program that never calls `heif` still lists them; `-Wl,-dead_strip_dylibs` drops them. No header of these
  frameworks is included by the library: `sgcl/codec/detail/apple_imageio.h` declares what it calls under names of
  its own, so a program's own `Point` or `Size` never meets MacTypes'.
- **Tested** against ImageIO's own reading of each file, drawn by CoreGraphics: files made by `sips` from PngSuite
  (gray, gray with alpha, RGB, RGBA, 16-bit, tRNS) decode to the same pixels, with 0 difference also for alpha after
  premultiplying; AVIF made by ImageIO the same; the module's own HEIC read back above 35 dB. The wrapping is fuzzed
  (the limits, the signature, the conversions, cut files).

## Member types

| Type | Definition |
|---|---|
| [options](../heif-options.md) | what an encoding takes: the quality |

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the first image of a HEIC, HEIF or AVIF file (static) |
| [encode](encode.md) | the image as HEIC, its bytes or into a stream (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::rgb8);
    vector<byte> file = codec::heif::encode(picture);
    println("HEIC: {}", codec::sniff(file) == codec::format::heif);

    codec::image back = codec::heif::decode(file);
    println("{}x{}, rgb8: {}", back.width(), back.height(),
            back.format() == codec::pixel_format::rgb8);
}
```

Output:

```text
HEIC: true
64x48, rgb8: true
```

## See also

- [decode](../decode.md), [sniff](../sniff.md): any format, told by its signature
- [png](../png/README.md), [jpeg](../jpeg/README.md): the formats the module writes itself
- [image](../image/README.md), [error](../error/README.md)
