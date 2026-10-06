[sgcl](../../README.md) › [codec](../README.md) › [bmp](README.md)

# sgcl::codec::bmp::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a BMP file: the file header, the DIB header of any of its sizes (12, 40, 52, 56, 64, 108 or 124 bytes), the
masks and the palette, then the rows from the offset the file header gives, bottom-up or top-down by the sign of
the height.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

Without `o.want` the image is `gray8` for a palette of grays, `rgba8` where there is alpha (its masks, a V4 or V5
header at 32 bits, RLE) and `rgb8` otherwise; with `o.want` each row is converted to that format as it is decoded.

- **Palettes.** 1, 2, 4 and 8 bits, of entries of 3 bytes (BITMAPCOREHEADER) or 4; a palette shorter than its
  colors claim is taken as far as the pixels' offset lets it, and an index past it takes its first entry.
- **Bit fields.** 16 and 32 bits with masks (BI_BITFIELDS, BI_ALPHABITFIELDS, or the masks of a V2 header and
  later); 16 bits without masks are 5-5-5, 32 bits without them 8-8-8 with the high byte alpha only under a V4 or
  V5 header. A channel of fewer than 8 bits is widened by repeating its bits.
- **RLE.** RLE8 at 8 bits and RLE4 at 4: runs, literal runs, end of line, end of bitmap and delta, a pixel the
  stream skips left transparent black; a stream that ends before its end of bitmap leaves the rest so too.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for a header the
format does not allow (a signature other than `BM`, planes other than 1, a side of zero, a depth outside 1, 2, 4, 8,
16, 24 and 32, a compression its depth does not take, top-down RLE, the pixels' offset inside the headers),
`errc::unsupported` for a DIB header of another size or JPEG and PNG inside, `errc::unexpected_end` for data that
ends before the last pixel, `errc::too_large` for an image of more pixels than `o.limits.max_pixels`, and (2)
`errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image and a row; a bottom-up file is read in the order of its rows, each put in its place,
with no second pass. (2) adds one block of the stream, not the file; a stream is read forward only, the bytes
between the headers and the pixels skipped.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image gray(10, 4, codec::pixel_format::gray8);
    vector<byte> file = codec::bmp::encode(gray);
    codec::image back = codec::bmp::decode(file);
    println("gray8 again: {}", back.format() == codec::pixel_format::gray8);

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::bmp::decode(in,
            {.want = codec::pixel_format::rgba8});
    println("from a stream: {}x{}, {} bytes a row", streamed->width(), streamed->height(),
            streamed->stride());

    file[0] = byte('X');
    println("{}", codec::bmp::decode(file).error().message());
}
```

Output:

```text
gray8 again: true
from a stream: 10x4, 40 bytes a row
offset 0: bmp: not a BMP signature
```

## See also

- [encode](encode.md): a BMP of an image
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [sgcl::codec::bmp](README.md)
