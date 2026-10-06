[sgcl](../../README.md) › [codec](../README.md) › [ico](README.md)

# sgcl::codec::ico::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes the largest entry of an ICO or CUR file: the directory read, the entry of the most pixels chosen (of
the most bits among equals), its data decoded as PNG when it begins with PNG's signature and as a BMP without its
file header otherwise.

1. Reads the file in memory, in place.
2. Reads the stream to its end, then the file as (1).

A PNG entry comes as [png::decode](../png/decode.md) gives it. A BMP entry comes as [bmp::decode](../bmp/decode.md)
gives it, its height halved, and its AND mask applied: a pixel is transparent where the mask has a bit of 1, unless
the entry carries an alpha of its own (32 bits with any alpha other than zero), which is kept. The size the
directory claims is not trusted: the image is the size its data says. With `o.want` the image is converted to
that format.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for a header other
than an icon's or a cursor's, a directory of no entry or an entry outside the data, `errc::unexpected_end` for
data that ends in the header or the directory, any error of the entry's own decoding, `errc::too_large` for an
image of more pixels than `o.limits.max_pixels`, and (2) a stream longer than four bytes a pixel of that limit and
64 KB, or `errc::io` when the stream fails.

## Complexity

Linear in the size of the directory and of the entry decoded.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

Only the largest entry is decoded; the others are read past. (2) holds the whole file while it decodes.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<codec::image> sizes;
    for (int side : {16, 48, 32}) {
        sizes.push_back(codec::image(side, side, codec::pixel_format::rgba8));
    }
    vector<byte> file = codec::ico::encode(sizes);
    codec::image largest = codec::ico::decode(file);
    println("the largest: {}x{}", largest.width(), largest.height());

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::ico::decode(in);
    println("from a stream: {}x{}", streamed->width(), streamed->height());
}
```

Output:

```text
the largest: 48x48
from a stream: 48x48
```

## See also

- [decode_all](decode_all.md): every entry
- [encode](encode.md): an ICO or CUR of one image or several
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md)
- [sgcl::codec::ico](README.md)
