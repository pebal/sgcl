[sgcl](../../README.md) › [codec](../README.md) › [gif](../gif.md)

# sgcl::codec::gif::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes the first frame of a GIF: the whole canvas with the frame drawn on it, as it is shown.

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

The header, the logical screen, its color table and the blocks up to the end of the first image are decoded, and
nothing after: the rest of the file, its other frames and its trailer, is not checked, so a file cut after its first
image decodes.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted and the limits; the default is `rgba8` and 100 million pixels |

## Return value

The image, `rgba8` unless `o.want` asks for another format, or the error: `errc::invalid_argument` for a `want`
outside the list, `errc::corrupt`, `errc::unexpected_end` and `errc::too_large` as the class's
[rules](../gif.md#rules) say, `errc::corrupt` for a file that ends at its trailer with no image, and (2)
`errc::io` when the stream fails.

## Complexity

Linear in the bytes up to the end of the first image and in the pixels of the canvas.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

A GIF has no EXIF and no ICC profile: the image's [exif](../image/exif.md) and [icc](../image/icc.md) are empty and
its [orientation](../image/orientation.md) is 1, whatever `o.metadata` says.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const char* path = "tests/codec/fuzz/seeds/gif_decode/fire.gif";
    vector<byte> file = io::read_file(path);
    codec::image first = codec::gif::decode(file);
    println("{}x{}, rgba8: {}", first.width(), first.height(),
            first.format() == codec::pixel_format::rgba8);

    codec::image gray = codec::gif::decode(file, {.want = codec::pixel_format::gray8});
    println("as gray8: {} bytes a row", gray.stride());

    io::file in = io::open(path);
    codec::image streamed = codec::gif::decode(in);
    println("from a stream: {}x{}", streamed.width(), streamed.height());

    auto refused = codec::gif::decode(file, {.limits = {.max_pixels = 1000}});
    if (!refused) println(refused.error().message());
}
```

Output:

```text
30x60, rgba8: true
as gray8: 30 bytes a row
from a stream: 30x60
offset 6: size limit exceeded
```

## See also

- [frames](frames.md): every frame, one by one
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md)
- [sgcl::codec::gif](../gif.md)
