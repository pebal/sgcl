[sgcl](../../README.md) › [codec](../README.md) › [qoi](README.md)

# sgcl::codec::qoi::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a QOI file: its header of 14 bytes, then the chunks a pixel at a time, each one an index into the 64
colors seen last, a difference from the pixel before, a run of it or a color in full.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

Without `o.want` a file of 3 channels becomes `rgb8` and one of 4 `rgba8`; with `o.want` each row is converted to
that format as it is decoded. The color space byte (sRGB or linear) is checked and not applied, as the reference
decoder leaves it. The 8 bytes of the end marker are not required.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for a signature
other than `qoif`, a side of zero, a header of other than 3 or 4 channels or a color space byte past 1, `errc::unexpected_end` for data that ends before the
last pixel, `errc::too_large` for an image of more pixels than `o.limits.max_pixels`, and (2) `errc::io` when the
stream fails.

## Complexity

Linear in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image and, with `o.want`, a row; (2) adds one block of the stream, not the file.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(8, 8, codec::pixel_format::rgb8);
    vector<byte> file = codec::qoi::encode(picture);

    codec::image color = codec::qoi::decode(file);
    codec::image gray = codec::qoi::decode(file, {.want = codec::pixel_format::gray8});
    println("{} and {} bytes a row", color.stride(), gray.stride());

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::qoi::decode(in);
    println("from a stream: {}x{}", streamed->width(), streamed->height());

    file.resize(15);  // the header and one run of 62 pixels
    println("{}", codec::qoi::decode(file).error().message());
}
```

Output:

```text
24 and 8 bytes a row
from a stream: 8x8
offset 15: qoi: the data ends before the last pixel
```

## See also

- [encode](encode.md): a QOI of an image
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [sgcl::codec::qoi](README.md)
