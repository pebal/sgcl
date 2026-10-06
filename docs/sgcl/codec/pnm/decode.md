[sgcl](../../README.md) › [codec](../README.md) › [pnm](README.md)

# sgcl::codec::pnm::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a Netpbm file: P1 to P6, plain and raw, and P7 (PAM). The header's numbers are read past white space and
comments; PAM's header is its named lines up to `ENDHDR`, `TUPLTYPE` read past and `DEPTH` telling the channels.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

Without `o.want` the image comes as the file has it:

- PBM becomes `gray8`, 0 for a black pixel (a bit of 1) and 255 for a white one;
- PGM becomes `gray8` or `gray16` by its maxval, PPM `rgb8` or `rgb16`;
- PAM of depth 1, 2, 3 and 4 becomes gray, gray with alpha, RGB or RGBA, 8 or 16 bits by its maxval.

A maxval of 255 or 65535 gives the samples as they are; another maxval is scaled to the full range of 8 bits (up
to 255) or 16 (above), rounded to the nearest, and a sample past it is taken as maxval. 16-bit samples are
big-endian in the file and in the byte order of the machine in the image. With `o.want` each row is converted to
that format as it is decoded. One image is read: a file of several, as Netpbm allows, gives the first.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for a header the
format does not allow (a magic other than P1 to P7, a side of zero, a maxval outside 1 to 65535, a PAM depth outside
1 to 4 or a header line PAM does not have, a character where a number belongs, a PBM digit other than 0 and 1), `errc::unexpected_end` for data that ends before the last pixel, `errc::too_large` for an image of
more pixels than `o.limits.max_pixels`, and (2) `errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image and a row; (2) adds one block of the stream, not the file.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "P2\n# a comment\n3 2\n4\n0 1 2\n3 4 4\n";
    slice<const byte> file(reinterpret_cast<const byte*>(text.data()), text.size());
    codec::image gray = codec::pnm::decode(file);
    for (int y : range(2)) {
        slice<byte> row = gray.row(y);
        println("{} {} {}", int(row[0]), int(row[1]), int(row[2]));
    }

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::pnm::decode(in,
            {.want = codec::pixel_format::rgb8});
    println("from a stream: {}x{}, {} bytes a row", streamed->width(), streamed->height(),
            streamed->stride());
}
```

Output:

```text
0 64 128
191 255 255
from a stream: 3x2, 9 bytes a row
```

## See also

- [encode](encode.md): a Netpbm file of an image
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [sgcl::codec::pnm](README.md)
