[sgcl](../../README.md) › [codec](../README.md) › [tiff](README.md)

# sgcl::codec::tiff::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes the first page of a TIFF file: the header, the first directory (IFD), its strips or tiles decompressed,
the predictor undone and the samples put in their pixels.

1. Reads the file in memory, in place.
2. Reads the stream to its end, then the file as (1): a TIFF's directories and strips lie anywhere in it.

Without `o.want` the image is the page's own format:

- bilevel and gray of 1 to 8 bits become `gray8` (scaled to 8 bits, WhiteIsZero inverted), of 16 `gray16`;
- with an alpha sample `gray_alpha8` or `gray_alpha16`;
- RGB becomes `rgb8` or `rgb16`, with alpha `rgba8` or `rgba16`; associated (premultiplied) alpha is divided out;
- a palette, of 1 to 8 bits, becomes `rgb8`, its 16-bit color map taken to 8;
- CMYK becomes `cmyk8`, as ink (0 none) as JPEG's Adobe CMYK, 16 bits narrowed to 8;
- JPEG pages (compression 7) come as [jpeg::decode](../jpeg/decode.md) gives them, the page's JPEGTables in front.

With `o.want` the image is converted to that format. The orientation tag becomes the image's
[orientation](../image/orientation.md) and the ICC profile its [icc](../image/icc.md); the pixels are not turned. The
image's [exif](../image/exif.md) stays empty: a TIFF's EXIF is the file itself, which
[metadata::read](../metadata/read.md) reads.
16-bit samples of either byte order come in the byte order of the machine.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list; `errc::corrupt` for a header
other than `II` or `MM` and 42, no page, a chain of directories that loops, fields, strips or tiles outside the
file, a palette page without its color map, compressed data that does not decompress; `errc::unsupported` for
BigTIFF, a compression other than none, PackBits, LZW, Deflate and JPEG, samples other than unsigned integers,
a photometric interpretation or a depth the module does not read, a predictor other than horizontal;
`errc::unexpected_end` for data that ends in a directory or strips shorter than their rows; `errc::too_large` for a
page of more pixels than `o.limits.max_pixels`, and (2) a stream longer than eight bytes a pixel of that limit
and 64 MB, or `errc::io` when the stream fails.

## Complexity

Linear in the size of the page and in its pixels.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image and, for a compressed page, a strip or a tile decompressed; strips of 8 or 16 bits of
gray or RGB, the common case, are decompressed straight into the image. (2) holds the whole file while it decodes.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(300, 200, codec::pixel_format::rgba8);
    picture.set_orientation(6);
    vector<byte> file = codec::tiff::encode(picture);

    codec::image back = codec::tiff::decode(file);
    println("{}x{}, rgba8 {}, orientation {}", back.width(), back.height(),
            back.format() == codec::pixel_format::rgba8, back.orientation());

    io::buffer in(file);
    expected<codec::image, codec::error> gray = codec::tiff::decode(in,
            {.want = codec::pixel_format::gray8});
    println("from a stream: {} bytes a row", gray->stride());
}
```

Output:

```text
300x200, rgba8 true, orientation 6
from a stream: 300 bytes a row
```

## See also

- [decode_all](decode_all.md): every page
- [encode](encode.md): a TIFF of one image or several
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [sgcl::codec::tiff](README.md)
