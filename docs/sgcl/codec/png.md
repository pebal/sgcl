# sgcl::codec::png

```cpp
#include "sgcl/codec/png.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    class png {
    public:
        struct options {
            compress::level level = 7;   // compress's: 0 stores, 1 fastest, 9 smallest; 7 the default here
        };

        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {});

        static vector<byte> encode(const image& im, const options& o = {});
        static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {});
    };
}
```

PNG as the W3C's third edition has it: every color type and bit depth, Adam7 interlacing, tRNS, and the CRC-32 of every chunk and the Adler-32 of the image data checked.

**Decoding.** Without [`want`](README.md#limits), the image comes in the file's own format:

- gray of 1 to 8 bits becomes `gray8` (scaled to 8 bits), 16 bits `gray16`;
- gray with alpha becomes `gray_alpha8` or `gray_alpha16`;
- truecolor becomes `rgb8` or `rgb16`, and with alpha `rgba8` or `rgba16`;
- a palette always becomes `rgba8`;
- a tRNS chunk adds alpha to gray and truecolor.

Gamma, chromaticities and sRGB (gAMA, cHRM, sRGB) are not applied, as in Go. EXIF (eXIf) and the ICC profile (iCCP) are kept as bytes. An APNG decodes to its default image.

The decoder is strict where the format is and lenient where libpng is:

- These are errors: a critical chunk it does not know or out of place, a bad CRC or Adler-32, image data that ends short, a missing IEND.
- These are passed over: an ancillary chunk it does not know or out of place, a palette index past the palette (opaque black), stream bytes past the image.

**Encoding.** Any image is written as the PNG type that holds it, at 8 or 16 bits a channel. `cmyk8` is written as truecolor; nothing is interlaced and nothing gets a palette.

- **Filters.** Each row gets the filter whose output has the smallest sum of absolute values, as libpng chooses: the same filter as libpng on every row.
- **Compression.** The zlib stream uses DEFLATE's filtered strategy at `options.level`, 7 unless asked: the first of DEFLATE's chain levels, which the filtered strategy is for ([level](../compress/README.md#level); levels 1 to 6 are a faster encoder that gains nothing from it). The files come within ±0.3 % of libpng's size at the same level, and at 7 a little smaller than libpng's default.
- **Metadata.** The image's EXIF and ICC profile become eXIf and iCCP.
- **Failure.** `encode` to bytes never fails for a valid image. To a writer, it fails only with the stream (`errc::io`).

## Members

### options

```cpp
struct options {
    compress::level level = 7;
};
```

The encoder's DEFLATE level, compress's: 0 stores, 1 is the fastest, 9 the smallest; 7 unless told.

### decode

```cpp
static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});
```

The image of a PNG file, from its bytes or from a stream, in the file's own format unless [`want`](README.md#limits) asks for another.

### encode

```cpp
static vector<byte> encode(const image& im, const options& o = {});
static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {});
```

A PNG of the image, as bytes or into a stream; to bytes it never fails for a valid image.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"

#include <algorithm>

using namespace sgcl;

int main() {
    // a gradient, 64 × 32, with a transparent left half
    codec::image picture(64, 32, codec::pixel_format::rgba8);
    for (uint32_t y : range(picture.height())) {
        slice<byte> row = picture.row(y);
        for (uint32_t x : range(picture.width())) {
            row[4 * x] = byte(x * 4);
            row[4 * x + 1] = byte(y * 8);
            row[4 * x + 2] = byte(128);
            row[4 * x + 3] = byte(x < 32 ? 0 : 255);
        }
    }
    vector<byte> file = codec::png::encode(picture, {.level = 9});
    codec::image back = codec::png::decode(file);
    const bool same =
        std::equal(back.pixels().begin(), back.pixels().end(), picture.pixels().begin());
    println("{} bytes; {}x{}; the same pixels: {}", file.size(), back.width(), back.height(), same);
}
```

Output:

```text
130 bytes; 64x32; the same pixels: true
```

## See also

[`image`](image.md), [`decode`](decode.md), [`error`](error.md); [`compress::zlib`](../compress/zlib.md), which it writes and reads through.
