[sgcl](../../README.md) › [codec](../README.md) › [webp](README.md)

# sgcl::codec::webp::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a still WebP image, or an animation's first frame on its canvas.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time: the memory is the image, its words and a block of the stream.

With the default options a file is read in its own pixel format, at most 100 million pixels and 64 MB of metadata,
with its EXIF and ICC profile; `o` sets anything else: the pixel format wanted, the limits, the metadata.

A still image comes as `rgba8` when the file says it has alpha (VP8X's flag, or VP8L's `alpha_is_used` in the simple
format) and as `rgb8` when it does not; a lossy image of the simple format is `rgb8`. An animation's first frame comes
as `rgba8`. `o.want` asks for another format, each row converted as it is decoded.

After the image the rest of the file is walked to its end, its frames' headers read and their data passed over, so
that a file libwebp refuses is refused here too.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether the metadata is read |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt`,
`errc::unexpected_end` and `errc::too_large` as the class's [rules](README.md#rules) say, and (2) `errc::io` when the
stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The metadata is the extended format's first ICCP and EXIF chunks: [icc](../image/icc.md) the profile,
[exif](../image/exif.md) the EXIF block without the `Exif\0\0` some writers put before its TIFF header, and
[orientation](../image/orientation.md) the EXIF's. `o.metadata = false` leaves them empty and the orientation 1.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> still = io::read_file("tests/codec/fuzz/seeds/webp_decode/lossless4.webp");
    codec::image gray = codec::webp::decode(still, {.want = codec::pixel_format::gray8});
    println("as gray8: {} bytes a row", gray.stride());

    auto refused = codec::webp::decode(still, {.limits = {.max_pixels = 1000}});
    if (!refused) println(refused.error().message());

    io::file in = io::open("tests/codec/fuzz/seeds/webp_decode/made_extended.webp");
    codec::image photo = codec::webp::decode(in);
    println("from a stream: {}x{}, ICC {} bytes, EXIF {} bytes", photo.width(), photo.height(),
            photo.icc().size(), photo.exif().size());
}
```

Output:

```text
as gray8: 256 bytes a row
offset 21: size limit exceeded
from a stream: 7x5, ICC 20 bytes, EXIF 10 bytes
```

## See also

- [frames](frames.md): every frame, one by one
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md)
- [sgcl::codec::webp](README.md)
