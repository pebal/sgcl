[sgcl](../../README.md) › [codec](../README.md) › [jpeg](../jpeg.md)

# sgcl::codec::jpeg::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a JPEG file: baseline, extended sequential of 8 bits or progressive, with Huffman coding, every sampling
factor of 1 to 4 that divides the largest of the frame, restart intervals, and gray, YCbCr, RGB, CMYK or YCCK
components. The blocks are dequantized and transformed by libjpeg-turbo's integer IDCT, the components upsampled
and converted as `djpeg -dct int` does it, row by row as the rows complete.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

Without `o.want` the image comes in one of three formats:

- one component (gray) becomes `gray8`;
- three, YCbCr (JFIF, or no Adobe marker saying otherwise) or RGB (Adobe's transform 0, or components named R, G
  and B), become `rgb8`;
- four, CMYK or YCCK, become `cmyk8`, the ink of each channel: a file with an Adobe marker has its inverted values
  turned back, as Go does.

With `o.want` each row is converted to that format as it is decoded. A file with no Huffman tables of its own (a
Motion-JPEG frame) takes the typical ones of Annex K, as libjpeg-turbo does.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for data the
format does not allow, `errc::unexpected_end` for a file that ends before EOI, `errc::unsupported` for what the
class's [rules](../jpeg.md#rules) refuse, for a frame of 2 or more than 4 components and for a sampling factor that
does not divide the largest, `errc::too_large` for an image of more pixels than `o.limits.max_pixels` or an EXIF
block or ICC profile past `o.limits.max_metadata`, and (2) `errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image and three rows of MCUs of each component: the one being decoded and two for the rows above
and below a row being upsampled, all of it made when the first scan starts and none per MCU. A file of one scan per
component keeps its components whole until the last scan, and a progressive file the coefficients of the whole
image (2 bytes a sample of each component) until its end. (2) adds one block of the stream, not the file.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = codec::jpeg::encode(codec::image(40, 30, codec::pixel_format::rgb8));

    codec::image color = codec::jpeg::decode(file);
    codec::image gray = codec::jpeg::decode(file, {.want = codec::pixel_format::gray8});
    println("{} and {} bytes a row", color.stride(), gray.stride());

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::jpeg::decode(in);
    println("from a stream: {}x{}", streamed->width(), streamed->height());

    expected<codec::image, codec::error> cut = codec::jpeg::decode(file.as_slice(0, 400));
    println("{}", cut.error().message());
}
```

Output:

```text
120 and 40 bytes a row
from a stream: 40x30
offset 400: jpeg: the data ends in the middle
```

## See also

- [encode](encode.md): a baseline JPEG of an image
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [image::oriented](../image/oriented.md): the image turned as its EXIF says
- [sgcl::codec::jpeg](../jpeg.md)
