[sgcl](../../README.md) › [codec](../README.md) › [tiff](README.md)

# sgcl::codec::tiff::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;    // (1)
static expected<vector<byte>, error> encode(const slice<const image>& pages,                     // (2)
                                            const options& o = {}) noexcept;
static expected<void, error> encode(const image& im, const io::writer& out,                      // (3)
                                    const options& o = {});
static expected<void, error> encode(const slice<const image>& pages, const io::writer& out,      // (4)
                                    const options& o = {});
```

Encodes one image or several as a TIFF file, little-endian, one page an image in the order given.

- (1, 3) One page.
- (2, 4) One page an image.
- (1–2) Return the file as bytes.
- (3–4) Write the file into a stream.

Every pixel format is written as it is: gray, gray with alpha, RGB and RGBA of 8 or 16 bits (alpha as
unassociated, ExtraSamples 2), `cmyk8` as Separated ink. The rows go in strips of about 64 KB, compressed as
`o.compression` says: LZW unless told, which every TIFF reader takes; Deflate, smaller and slower, which libtiff,
Go and every recent reader take; or none. LZW and Deflate get the horizontal predictor, as libtiff's tools write
them. The image's orientation and its ICC profile are written with its page.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `pages` | the images, one page each |
| `out` | the stream the file is written into |
| `o` | the compression; the default is LZW |

## Return value

- (1–2) The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for no page, a
  compression outside the list, or a file past the 4 GB that TIFF's offsets hold.
- (3–4) Nothing, or the error: `errc::invalid_argument` as (1–2), before anything is written; `errc::io` when the
  stream fails, at the offset of the bytes written before, the stream's own error in
  [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the images.

## Exceptions

- (1–2) None.
- (3–4) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The file is made in memory first, since each directory gives the offsets of what follows it.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(256, 256, codec::pixel_format::rgb8);
    for (int y : range(256)) {
        for (int x : range(256)) {
            picture.row(y)[3 * x] = byte(x);
            picture.row(y)[3 * x + 1] = byte(y);
        }
    }
    for (codec::tiff::compression c : {codec::tiff::compression::none,
            codec::tiff::compression::lzw,
                                       codec::tiff::compression::deflate}) {
        println("{} bytes", codec::tiff::encode(picture, {.compression = c})->size());
    }
}
```

Output:

```text
196792 bytes
3312 bytes
1418 bytes
```

## See also

- [decode](decode.md), [decode_all](decode_all.md): the pages of a TIFF file
- [options](../tiff-options.md), [compression](../tiff-compression.md)
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::tiff](README.md)
