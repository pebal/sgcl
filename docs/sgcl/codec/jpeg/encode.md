[sgcl](../../README.md) › [codec](../README.md) › [jpeg](../jpeg.md)

# sgcl::codec::jpeg::encode

```cpp
static expected<vector<byte>, error> encode(const image& im) noexcept;             // (1)
static expected<vector<byte>, error> encode(const image& im, const options& o);    // (2)
static expected<void, error> encode(const image& im, const io::writer& out);       // (3)
static expected<void, error> encode(const image& im, const io::writer& out,        // (4)
                                    const options& o);
```

Encodes an image as a baseline JPEG file, byte for byte what libjpeg-turbo's `cjpeg -dct int -baseline` writes
with the same quality, sampling and `-optimize`.

1. Returns the file as bytes, at quality 85 and 4:2:0.
2. Returns the file as bytes, as `o` asks.
3. Writes the file into a stream, at quality 85 and 4:2:0.
4. Writes the file into a stream, as `o` asks.

- **Format.** A gray image (`gray8`, `gray16`, gray with alpha) becomes a gray JPEG; any other becomes YCbCr, with
  alpha dropped, 16 bits taken to 8 and CMYK converted through RGB.
- **Coding.** The quantization tables of Annex K scaled by the IJG's quality, 8-bit at every quality (a baseline
  file); the integer FDCT; the typical Huffman tables of Annex K, or with `o.optimize` tables made for the image in
  a second pass. The file starts with JFIF.
- **Metadata.** The image's EXIF and ICC profile become APP1 and APP2, the profile in chunks.
- **The two forms.** `jpeg::encode(photo)` and `jpeg::encode(photo, {.quality = 90})` are two overloads rather
  than a default argument: a nested struct with member initializers cannot be a default argument inside its class.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `out` | the stream the file is written into |
| `o` | the quality, the subsampling and whether to optimize the Huffman tables |

## Return value

- (1–2) The bytes of the file, or the [error](../error.md) `errc::invalid_argument` for an image with a side past
  65 535 pixels, which JPEG's SOF cannot hold; any other image encodes.
- (3–4) Nothing, or the error: `errc::invalid_argument` for a side past 65 535 pixels, before anything is written;
  `errc::io` when the stream fails, at the offset of the bytes written before, the stream's own error in
  [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image; with `o.optimize` the image is read twice.

## Exceptions

- (1) None.
- (2) `invalid_argument` when `o.quality` is outside 1 to 100 or `o.subsampling` outside the list of
  [subsampling](../jpeg-subsampling.md): a contract of the program. Nothing is written.
- (3) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.
- (4) The same, and `invalid_argument` as (2).

## Notes

The memory is one row of MCUs and a block of output, none per MCU; (1–2) add the file. An EXIF block longer than
one segment holds (65 527 bytes) is not written, nor an ICC profile of more than 255 chunks.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(32, 32, codec::pixel_format::rgba8);
    vector<byte> file = codec::jpeg::encode(picture);
    codec::image back = codec::jpeg::decode(file);
    println("{} bytes, rgb8 without alpha: {}", file.size(),
            back.format() == codec::pixel_format::rgb8);

    io::buffer out;
    expected<void, codec::error> written = codec::jpeg::encode(picture, out);
    println("into the stream: {}, the same bytes: {}", written.has_value(), out.release() == file);

    codec::image deep(32, 32, codec::pixel_format::gray16);
    codec::image gray = codec::jpeg::decode(codec::jpeg::encode(deep));
    println("gray16 as a gray JPEG: {}", gray.format() == codec::pixel_format::gray8);

    try {
        codec::jpeg::encode(picture, {.quality = 0});
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
643 bytes, rgb8 without alpha: true
into the stream: true, the same bytes: true
gray16 as a gray JPEG: true
sgcl::codec::jpeg::encode: quality outside 1..100
```

## See also

- [decode](decode.md): the image of a JPEG file
- [options](../jpeg-options.md), [subsampling](../jpeg-subsampling.md): the quality and the chroma
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::jpeg](../jpeg.md)
