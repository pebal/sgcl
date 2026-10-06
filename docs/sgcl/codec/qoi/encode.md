[sgcl](../../README.md) › [codec](../README.md) › [qoi](README.md)

# sgcl::codec::qoi::encode

```cpp
static expected<vector<byte>, error> encode(const image& im) noexcept;          // (1)
static expected<void, error> encode(const image& im, const io::writer& out);    // (2)
```

Encodes an image as a QOI file.

1. Returns the file as bytes.
2. Writes the file into a stream.

An image of a format with alpha is written with 4 channels, any other with 3; 16-bit channels are narrowed to 8,
gray written as RGB and `cmyk8` through [convert](../image/convert.md)'s conversion. The color space byte says
sRGB. The coder is QOI's one way: a pixel equal to the one before extends a run, one found at its hash in the 64
colors seen last is its index, one close to the pixel before is a difference of 1 or 2 bytes, any other the color
in full; the file ends with QOI's 8 bytes of end marker.

## Parameters

| Parameter | Description |
|---|---|
| `im` | the image to encode |
| `out` | the stream the file is written into |

## Return value

1. The bytes of the file; every image encodes.
2. Nothing, or the [error](../error/README.md) `errc::io` when the stream fails, at the offset of the bytes
   written before, the stream's own error in [io_error](../error/io_error.md).

## Complexity

Linear in the pixels of the image.

## Exceptions

- (1) None.
- (2) What the stream's `write` throws: `out` calls the `write` of the object it is bound to.

## Notes

The memory is a row converted and a row of output, made once; (1) adds the file, (2) writes a row at a time.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image flat(100, 100, codec::pixel_format::rgba8);
    vector<byte> file = codec::qoi::encode(flat);
    println("{} bytes for 10000 equal pixels", file.size());

    io::buffer out;
    expected<void, codec::error> written = codec::qoi::encode(flat, out);
    println("into the stream {}, {} bytes", written.has_value(), out.size());
}
```

Output:

```text
185 bytes for 10000 equal pixels
into the stream true, 185 bytes
```

## See also

- [decode](decode.md): the image of a QOI file
- [save](../save.md): an image into a file, in the format its extension names
- [sgcl::codec::qoi](README.md)
