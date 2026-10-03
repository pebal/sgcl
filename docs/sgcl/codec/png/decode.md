[sgcl](../../README.md) › [codec](../README.md) › [png](README.md)

# sgcl::codec::png::decode

```cpp
static expected<image, error> decode(const slice<const byte>& data,                          // (1)
                                     const decode_options& o = {}) noexcept;
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});    // (2)
```

Decodes a PNG file: its chunks in order, each one's CRC-32 checked, the image data through one zlib stream as it
comes, each row unfiltered and put in its place (an interlaced file's seven passes of Adam7 to their pixels).

1. Reads the file in memory, in place.
2. Reads the file from a stream, a block at a time.

Without `o.want` the image comes in the file's own pixel format:

- gray of 1 to 8 bits becomes `gray8`, scaled to 8 bits, and of 16 bits `gray16`;
- gray with alpha becomes `gray_alpha8` or `gray_alpha16`;
- truecolor becomes `rgb8` or `rgb16`, and with alpha `rgba8` or `rgba16`;
- a palette always becomes `rgba8`;
- a tRNS chunk adds alpha to gray and truecolor: `gray_alpha8` or `gray_alpha16`, `rgba8` or `rgba16`.

16-bit samples are big-endian in the file and in the byte order of the machine in the image. With `o.want` each
row is converted to that format as it is decoded, with no second pass over the image.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the file |
| `in` | the stream the file is read from |
| `o` | the pixel format wanted, the limits and whether to keep the metadata; the default is the file's own format, 100 million pixels and the metadata kept |

## Return value

The image, or the error: `errc::invalid_argument` for a `want` outside the list, `errc::corrupt` for data the
format does not allow, `errc::checksum` for a chunk's CRC-32 or the image data's Adler-32, `errc::unexpected_end`
for a file that ends in the middle, `errc::unsupported` for a critical chunk the decoder does not know,
`errc::too_large` for an image of more pixels than `o.limits.max_pixels`, or than an address holds whatever the
limit says (sides of up to 2^31 − 1 can claim that), or an eXIf chunk or an ICC profile past
`o.limits.max_metadata` (the profile as it decompresses: an iCCP chunk may be larger by its name and the zlib
stream's own bytes), and (2) `errc::io` when the stream fails.

## Complexity

Linear in the size of the file and in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the image, a window of the zlib stream (its 32 KB of history and room for a row) and three rows, all
of it made when the first IDAT chunk comes and none per row; (2) adds one block of the stream, not the file.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = codec::png::encode(codec::image(3, 2, codec::pixel_format::gray8));

    codec::image gray = codec::png::decode(file);
    codec::image color = codec::png::decode(file, {.want = codec::pixel_format::rgba8});
    println("{} and {} bytes a row", gray.stride(), color.stride());

    io::buffer in(file);
    expected<codec::image, codec::error> streamed = codec::png::decode(in);
    println("from a stream: {}x{}", streamed->width(), streamed->height());

    file[20] ^= byte(1);  // a bit of the width in IHDR
    expected<codec::image, codec::error> broken = codec::png::decode(file);
    println("{}", broken.error().message());
}
```

Output:

```text
3 and 12 bytes a row
from a stream: 3x2
offset 8: png: CRC-32 of chunk IHDR
```

## See also

- [encode](encode.md): a PNG of an image
- [codec::decode](../decode.md): any format, told by its signature
- [decode_options](../decode_options.md), [limits](../limits.md), [pixel_format](../pixel_format.md)
- [sgcl::codec::png](README.md)
