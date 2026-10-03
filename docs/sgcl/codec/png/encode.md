[sgcl](../../README.md) › [codec](../README.md) › [png](../png.md)

# sgcl::codec::png::encode

```cpp
static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept;    // (1)
static expected<void, error> encode(const image& im, const io::writer& out,                      // (2)
                                    const options& o = {});
```

Encodes an image as a PNG file.

1. Returns the file as bytes.
2. Writes the file into a stream.

Any image is written as the PNG type that holds its format, at 8 or 16 bits a channel: gray, gray with alpha,
truecolor or truecolor with alpha, `cmyk8` as truecolor through [convert](../image/convert.md)'s conversion.
Nothing is interlaced and nothing gets a palette.

- **Filters.** Each row gets the one of the five filters whose output has the smallest sum of absolute values, its
  bytes taken as signed, as libpng chooses: the same filter as libpng on every row. At level 0 every row is left
  unfiltered.
- **Compression.** The rows go through one zlib stream with DEFLATE's filtered strategy at `o.level`, 7 unless
  asked: the first of DEFLATE's chain levels, which the filtered strategy is for; levels 1 to 6 are a faster
  encoder that gains nothing from it ([level](../../compress/level.md)). The stream is cut into IDAT
  chunks of 64 KB.
- **Metadata.** The image's EXIF and ICC profile become eXIf and iCCP. A chunk holds at most 2^31 − 1 bytes: an EXIF
  block, or a compressed profile with its name, past that is left out and the image written without it, as
  [jpeg::encode](../jpeg/encode.md) leaves out what a segment does not hold.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `out` | the stream the file is written into |
| `o` | the DEFLATE level; the default is 7 |

## Return value

1. The bytes of the file, or the [error](../error.md) `errc::invalid_argument` for an image with a side past
   2^31 − 1 pixels, which PNG's IHDR cannot hold; any other image encodes.
2. Nothing, or the error: `errc::invalid_argument` for a side past 2^31 − 1 pixels, before anything is written;
   `errc::io` when the stream fails, at the offset of the bytes written before, the stream's own error in
   [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The memory is the compressor's state, five rows and a block of output, made once and none per row; (1) adds the
file, (2) does not hold it. The sides are checked before any pixel is read or any byte written.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image deep(16, 16, codec::pixel_format::rgb16);
    vector<byte> file = codec::png::encode(deep);

    io::buffer out;
    expected<void, codec::error> written = codec::png::encode(deep, out, {.level = 9});
    println("{} bytes; into the stream {}, {} bytes", file.size(), written.has_value(), out.size());

    codec::image back = codec::png::decode(file);
    println("rgb16 again: {}", back.format() == codec::pixel_format::rgb16);

    codec::image ink(16, 16, codec::pixel_format::cmyk8);
    codec::image truecolor = codec::png::decode(codec::png::encode(ink));
    println("cmyk8 as rgb8: {}", truecolor.format() == codec::pixel_format::rgb8);
}
```

Output:

```text
78 bytes; into the stream true, 78 bytes
rgb16 again: true
cmyk8 as rgb8: true
```

## See also

- [decode](decode.md): the image of a PNG file
- [options](../png-options.md): the DEFLATE level
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::png](../png.md)
