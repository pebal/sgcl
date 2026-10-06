[sgcl](../../README.md) › [codec](../README.md) › [ico](README.md)

# sgcl::codec::ico::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;    // (1)
static expected<vector<byte>, error> encode(const slice<const image>& images,                    // (2)
                                            const options& o = {}) noexcept;
static expected<void, error> encode(const image& im, const io::writer& out,                      // (3)
                                    const options& o = {});
static expected<void, error> encode(const slice<const image>& images, const io::writer& out,     // (4)
                                    const options& o = {});
```

Encodes one image or several as an ICO file, or a CUR when `o.cursor` is set, one entry an image in the order
given.

- (1, 3) One entry.
- (2, 4) One entry an image.
- (1–2) Return the file as bytes.
- (3–4) Write the file into a stream.

An image of a side of 256 is written as PNG inside, as Windows writes its large icons; a smaller one as a 32-bit
BMP of BGRA with its AND mask (a bit of 1 where alpha is zero), which every reader since Windows 3 takes. Any
pixel format is written through `rgba8` by [convert](../image/convert.md)'s rules. A cursor's entries carry the
hotspot of `o`.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `images` | the images, one entry each |
| `out` | the stream the file is written into |
| `o` | an icon or a cursor, and its hotspot; the default is an icon |

## Return value

- (1–2) The bytes of the file, or the [error](../error/README.md) `errc::invalid_argument` for an image of a side
  past 256, no image, or more than the 65535 entries a directory holds.
- (3–4) Nothing, or the error: `errc::invalid_argument` as (1–2), before anything is written; `errc::io` when the
  stream fails, at the offset of the bytes written before, the stream's own error in
  [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the images.

## Exceptions

- (1–2) None.
- (3–4) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The entries are made in memory first, since the directory in front gives their sizes.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image arrow(32, 32, codec::pixel_format::rgba8);
    vector<byte> cursor = codec::ico::encode(arrow, {.cursor = true, .hotspot_x = 3,
            .hotspot_y = 1});
    println("CUR: type {}, {} bytes", int(cursor[2]), cursor.size());

    expected<vector<byte>, codec::error> big = codec::ico::encode(codec::image(512, 512,
            codec::pixel_format::rgba8));
    println("{}", big.error().message());
}
```

Output:

```text
CUR: type 2, 4286 bytes
offset 0: ico: a side past 256 pixels, more than an entry holds
```

## See also

- [decode](decode.md), [decode_all](decode_all.md): the images of an ICO or CUR file
- [options](../ico-options.md): an icon or a cursor
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::ico](README.md)
