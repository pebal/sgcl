[sgcl](../../README.md) › [codec](../README.md) › [heif](README.md)

# sgcl::codec::heif::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data) noexcept;    // (1)
static expected<image, error> decode(const io::reader& in);                      // (2)
```

Decodes the first image of a HEIC, HEIF or AVIF file through the system's codec, ImageIO on macOS; on other systems
it is `errc::unsupported`.

1. Reads the file in memory, in place.
2. Reads the stream to its end first, then the file: the items of a HEIF file lie wherever its `iloc` box says, and
   ImageIO reads them so.

It decodes with the defaults of [decode_options](../decode_options.md): the file's own pixel format, at most 100
million pixels and 64 MB of profile, the metadata read. [codec::decode](../decode.md) takes the options for anything
else: `codec::decode(bytes, {.want = codec::pixel_format::rgba8})` for 8 bits,
`{.limits = {.max_pixels = 20'000'000}}` for a smaller limit.

The pixel format is gray or RGB as the file has it, with alpha when it has any, 16 bits a channel when its components
have more than 8. A 10-bit HDR photo comes as `rgb16` or `rgba16`, its values scaled to 16 bits, its profile (PQ,
HLG, Display P3…) in `icc()`, and no tone mapping. A color model other than gray and RGB comes in sRGB.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |

## Return value

The image, or the error: `errc::unsupported` on a system without the codec, the errors of the checks the notes list,
and (2) `errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image, as the system's decoder is.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

What decoding checks: the size is checked against `max_pixels` from the file's properties before a pixel is decoded,
and the profile against `max_metadata` (`errc::too_large`). A file ImageIO does not read in full is
`errc::unexpected_end`, one it refuses or takes for another format is `errc::corrupt`, and a kind it does not know is
`errc::unsupported`.

What is refused before ImageIO sees the file (macOS only): an HEVC slice whose entry points lie at or past the end of
its data is `errc::corrupt`, at the offset of the slice's NAL unit. VideoToolbox, which decodes ImageIO's HEVC, waits
for ever on such a slice (seen on macOS 26.5), so a decoding would never return; the module reads the file's boxes
and the slices' headers first, in time linear in the file and with no thread of its own. Items whose data overlap
past four times the file's size, which no encoder writes, are `errc::unsupported`.

Alpha comes straight, not premultiplied, as every image of the module. ImageIO gives some files' alpha
premultiplied; the module divides it back out, which cannot restore what premultiplying rounded away in faint pixels.

The metadata: [icc](../image/icc.md) is the profile of the pixels' color space. [orientation](../image/orientation.md)
is ImageIO's reading of the file's rotation and mirror (`irot`, `imir`); the image is not turned, as with every
format ([oriented](../image/oriented.md)). [exif](../image/exif.md) is empty: ImageIO gives the tags, not the block,
and the module does not parse the container itself.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::rgba16);
    vector<byte> file = codec::heif::encode(picture);

    codec::image photo = codec::heif::decode(file);
    println("{}x{}, rgba16: {}", photo.width(), photo.height(),
            photo.format() == codec::pixel_format::rgba16);
    println("a profile: {}, EXIF: {}, orientation {}", !photo.icc().empty(), !photo.exif().empty(),
            photo.orientation());

    io::buffer in(file);
    codec::image streamed = codec::heif::decode(in);
    println("from a stream: {}x{}", streamed.width(), streamed.height());
}
```

Output:

```text
64x48, rgba16: true
a profile: true, EXIF: false, orientation 1
from a stream: 64x48
```

## See also

- [encode](encode.md): the image as HEIC
- [codec::decode](../decode.md): any format, with options
- [sgcl::codec::heif](README.md)
