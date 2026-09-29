# sgcl::codec::jpeg

```cpp
#include "sgcl/codec/jpeg.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    class jpeg {
    public:
        enum class subsampling : uint8_t { s444, s422, s420 };   // chroma whole, half across, half both ways

        struct options {
            int quality = 85;                                   // 1..100, the IJG's scale
            jpeg::subsampling subsampling = jpeg::subsampling::s420;
            bool optimize = false;                              // Huffman tables made for the image
        };

        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
        static expected<image, error> decode(const io::reader& in, const decode_options& o = {});

        static vector<byte> encode(const image& im);
        static vector<byte> encode(const image& im, const options& o);
        static expected<void, error> encode(const image& im, const io::writer& out);
        static expected<void, error> encode(const image& im, const io::writer& out, const options& o);
    };
}
```

JPEG as T.81 has it, with JFIF 1.02 and Adobe's APP14.

**Decoding.** The decoder reads the sequential modes with Huffman coding (baseline and extended, 8 bits) and the progressive mode, every sampling factor, restart intervals, gray, YCbCr, RGB, CMYK and YCCK.

- **Pixels.** They are libjpeg-turbo's (`djpeg -dct int`) bit for bit: its integer IDCT, its "fancy" upsampling and its YCbCr conversion.
- **Format.** The image comes `gray8`, `rgb8` or `cmyk8`. A CMYK file with an Adobe marker has its inverted values turned back, as Go does.
- **Metadata.** EXIF gives `orientation()` (the image is not turned) and is kept with the ICC profile, whose APP2 chunks are joined in their order.
- **Tables.** A file with no Huffman tables (a Motion-JPEG frame) takes the typical ones of Annex K, as libjpeg-turbo does.
- **Block smoothing.** libjpeg-turbo smooths the blocks of a progressive file whose progression stops before its low coefficients are whole. The module does not: such a file's pixels are libjpeg's with smoothing off.
- **Refused.** Arithmetic coding, 12-bit samples, lossless and hierarchical files and DNL are `errc::unsupported`.
- **Errors.** Data that ends early is `errc::unexpected_end`, a scan cut by a marker or a restart marker out of turn `errc::corrupt`, even where libjpeg only warns.

**Encoding.** Baseline JPEG, byte for byte what libjpeg-turbo's `cjpeg -dct int -baseline` writes with the same quality, sampling and `-optimize`.

- **What it does.** It uses the quantization tables of Annex K scaled by the IJG's quality, the integer FDCT, JFIF, and the image's EXIF and ICC profile as APP1 and APP2.
- **Format.** A gray image (`gray8`, `gray16`, gray with alpha) becomes a gray JPEG; any other becomes YCbCr, with alpha dropped, 16 bits taken to 8 and CMYK converted through RGB.
- **Tables.** They are 8-bit (a baseline file) at every quality.
- **The two forms.** `jpeg::encode(photo)` and `jpeg::encode(photo, {.quality = 90})` are the two forms. They are two overloads rather than a default argument, which C++ does not allow for a nested struct with member initializers.
- **Contract.** A quality outside 1..100 is `invalid_argument`.

## Members

### subsampling, options

```cpp
enum class subsampling : uint8_t { s444, s422, s420 };

struct options {
    int quality = 85;
    jpeg::subsampling subsampling = jpeg::subsampling::s420;
    bool optimize = false;
};
```

The encoder's settings: the quality on the IJG's scale, 1 to 100 (85 unless told); the chroma kept whole, halved across or halved both ways (4:2:0 unless told); and Huffman tables made for the image in place of the typical ones (`-optimize`).

### decode

```cpp
static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
static expected<image, error> decode(const io::reader& in, const decode_options& o = {});
```

The image of a JPEG file, from its bytes or from a stream: `gray8`, `rgb8` or `cmyk8` unless [`want`](README.md#limits) asks for another format.

### encode

```cpp
static vector<byte> encode(const image& im);
static vector<byte> encode(const image& im, const options& o);
static expected<void, error> encode(const image& im, const io::writer& out);
static expected<void, error> encode(const image& im, const io::writer& out, const options& o);
```

A baseline JPEG of the image, as bytes or into a stream; with no options, quality 85 and 4:2:0.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/jpeg_decode/testorig.jpg");
    codec::image photo = codec::jpeg::decode(file);
    println("{}x{}, {} bytes", photo.width(), photo.height(), file.size());
    for (int quality : {50, 90}) {
        vector<byte> smaller = codec::jpeg::encode(photo, {.quality = quality, .optimize = true});
        println("quality {}: {} bytes", quality, smaller.size());
    }
    vector<byte> standard = codec::jpeg::encode(photo);
    println("quality 85, 4:2:0: {} bytes", standard.size());
}
```

Output:

```text
227x149, 5770 bytes
quality 50: 4058 bytes
quality 90: 6738 bytes
quality 85, 4:2:0: 7070 bytes
```

## See also

[`image`](image.md), [`decode`](decode.md), [`error`](error.md).
