[sgcl](../../README.md) › [codec](../README.md) › [jxl](README.md)

# sgcl::codec::jxl::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data) noexcept;    // (1)
static expected<image, error> decode(const io::reader& in);                      // (2)
```

Decodes the first frame of a JPEG XL file through the system's codec, ImageIO on macOS; on other systems it is
`errc::unsupported`.

1. Reads the file in memory, in place.
2. Reads the stream to its end first, then the file, as ImageIO reads one whole.

It decodes with the defaults of [decode_options](../decode_options.md), as [heif::decode](../heif/decode.md) does:
the file's own pixel format, at most 100 million pixels and 64 MB of profile, the metadata read.
[codec::decode](../decode.md) takes the options for anything else: `codec::decode(bytes, {.want =
codec::pixel_format::rgba8})`.

The pixel format is gray or RGB as the file has it, with alpha when it has any, 16 bits a channel when its samples
have more than 8; the orientation of the file's header comes as the image's [orientation](../image/orientation.md),
the pixels not turned, and its color profile as [icc](../image/icc.md). A JPEG recompressed without loss decodes to
the pixels libjxl reconstructs, which differ from a JPEG decoder's by the rounding of two inverse DCTs.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |

## Return value

The image, or the error: `errc::unsupported` on a system without the codec, for gray with alpha where the system
decodes it wrong, and for a kind of file the system does not read; `errc::corrupt` for a file the system refuses or
takes for another format; `errc::unexpected_end` for one that ends before its image; `errc::too_large` for an image
of more than 100 million pixels or a profile past 64 MB; and (2) `errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image, as the system's decoder is.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The time is the system's: the module converts ImageIO's pixels into the image with vImage, in the image's own color
space, and adds nothing measurable to ImageIO's own decoding. libjxl run directly over all the cores, as `djxl` runs
it, decodes faster than ImageIO does ([benchmarks](../benchmarks.md)).

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/jxl_decode/photo.jxl");
    expected<codec::image, codec::error> photo = codec::jxl::decode(file);
    if (photo) {
        println("{}x{}, orientation {}", photo->width(), photo->height(), photo->orientation());
    }
    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::jxl::decode(in);
    println("from a stream: {}", streamed.has_value() == photo.has_value());
    file.resize(20);
    println("cut: {}", codec::jxl::decode(file).has_value());
}
```

Output:

```text
48x32, orientation 6
from a stream: true
cut: false
```

## See also

- [codec::decode](../decode.md): any format, told by its signature, with options
- [metadata::read](../metadata/read.md): the file's EXIF and XMP
- [heif::decode](../heif/decode.md): HEIF through the same codec
- [sgcl::codec::jxl](README.md)
