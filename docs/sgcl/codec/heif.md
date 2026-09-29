# sgcl::codec::heif

```cpp
#include "sgcl/codec/heif.h"   // or "sgcl/codec/codec.h"

namespace sgcl::codec {
    class heif {
    public:
        // the first image of a HEIC, HEIF or AVIF file
        static expected<image, error> decode(const slice<const byte>& data);
        static expected<image, error> decode(const io::reader& in);

        struct options {
            int quality = 85;   // 1..100
        };

        // the image as HEIC
        static expected<vector<byte>, error> encode(const image& im);
        static expected<vector<byte>, error> encode(const image& im, const options& o);
        static expected<void, error> encode(const image& im, const io::writer& out);
        static expected<void, error> encode(const image& im, const io::writer& out, const options& o);
    };
}
```

One line reads a HEIC photo, `codec::image photo = codec::heif::decode(bytes)`, and one writes one, `codec::heif::encode(photo)`. [`codec::decode`](decode.md) reads HEIF and AVIF too, told by their signature, its `decode_options` setting anything else (`{.want = codec::pixel_format::rgb8, .limits = {.max_pixels = 20'000'000}}`). The quality of the encoding is an option: `codec::heif::encode(photo, out, {.quality = 60})`.

**Through the system's codec.** HEIC holds HEVC and AVIF holds AV1: video codecs, which the module takes from the platform as it would take video, and does not write itself. On macOS the calls go to ImageIO. On other systems every function is `errc::unsupported` (Windows through WIC belongs to the platform step of 1.0.0). An encoder may be missing even on macOS (some virtual machines): `encode` is then `errc::unsupported`, which is why it returns `expected` where PNG's and JPEG's do not.

**What it reads.** HEIF and HEIC still images, and AVIF where the system reads it (macOS 13 and later), the first image of the file. Sequences, thumbnails and auxiliary images (depth, alpha planes of their own) are not read as such.

**The pixel format.** Gray or RGB as the file has it, with alpha when it has any, 16 bits a channel when its components have more than 8. A 10-bit HDR photo comes as `rgb16` or `rgba16`, its values scaled to 16 bits, its profile (PQ, HLG, Display P3…) in `icc()`, and no tone mapping. A program that wants 8 bits asks `codec::decode(bytes, {.want = codec::pixel_format::rgba8})`. A color model other than gray and RGB comes in sRGB.

**Alpha** comes straight, not premultiplied, as every image of the module. ImageIO gives some files' alpha premultiplied; the module divides it back out, which cannot restore what premultiplying rounded away in faint pixels.

**Metadata.** `icc()` is the profile of the pixels' color space. `orientation()` is ImageIO's reading of the file's rotation and mirror (`irot`, `imir`); the image is not turned, as with every format ([`oriented`](image.md)). `exif()` is empty: ImageIO gives the tags, not the block, and the module does not parse the container itself.

**Writing.** HEIC at a quality of 1 to 100, 85 by default; a quality outside is `errc::invalid_argument`. The image's ICC profile and orientation go with it. 16-bit images are written as the system's encoder writes them (10 bits). A CMYK image goes through RGB. AVIF is not written in 1.0.0; writing it is an addition planned for 1.x.

**Streams.** `decode` from an [`io::reader`](../io/stream.md) reads the stream to its end first: the items of a HEIF file lie wherever its `iloc` box says, and ImageIO reads them so. `encode` into an [`io::writer`](../io/stream.md) writes the bytes as ImageIO produces them, without holding the whole file.

**What decoding checks.** The size is checked against `max_pixels` from the file's properties before a pixel is decoded, and the profile against `max_metadata` (`errc::too_large`). A file ImageIO does not read in full is `errc::unexpected_end`, one it refuses or takes for another format is `errc::corrupt`, and a kind it does not know is `errc::unsupported`.

**Linking.** On Apple's systems the library links ImageIO, CoreGraphics, CoreFoundation and Accelerate (CMake does it). A program that never calls `heif` still lists them; `-Wl,-dead_strip_dylibs` drops them. No header of these frameworks is included by the library: `detail/apple_imageio.h` declares what it calls under names of its own, so a program's own `Point` or `Size` never meets MacTypes'.

**Tested** against ImageIO's own reading of each file, drawn by CoreGraphics: files made by `sips` from PngSuite (gray, gray with alpha, RGB, RGBA, 16-bit, tRNS) decode to the same pixels, with 0 difference also for alpha after premultiplying; AVIF made by ImageIO the same; the module's own HEIC read back above 35 dB. The wrapping is fuzzed (the limits, the signature, the conversions, cut files).

## Members

### decode

```cpp
static expected<image, error> decode(const slice<const byte>& data);
static expected<image, error> decode(const io::reader& in);
```

The first image of a HEIC, HEIF or AVIF file, through the system's codec; `errc::unsupported` where there is none. From a reader, the stream is read to its end first.

### options

```cpp
struct options {
    int quality = 85;   // 1..100
};
```

`quality` is the quality of the encoding, 1 to 100, 85 by default; a value outside is `errc::invalid_argument`.

### encode

```cpp
static expected<vector<byte>, error> encode(const image& im);
static expected<vector<byte>, error> encode(const image& im, const options& o);
static expected<void, error> encode(const image& im, const io::writer& out);
static expected<void, error> encode(const image& im, const io::writer& out, const options& o);
```

The image as HEIC, with its ICC profile and orientation: its bytes, or written into `out` as the system's encoder produces them. `errc::unsupported` where the system has no encoder.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/heif_decode/tbrn2c08.heic");
    codec::image photo = codec::heif::decode(file);
    vector<byte> again = codec::heif::encode(photo);
    println("{}x{}, written back as HEIC: {}", photo.width(), photo.height(),
            codec::sniff(again) == codec::format::heif);
}
```

Output:

```text
32x32, written back as HEIC: true
```

On macOS; on other systems `decode` is `errc::unsupported`, which the conversion to `codec::image` throws.

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/heif_decode/tbrn2c08.heic");
    auto photo = codec::heif::decode(file);
    if (!photo) {
        println("{}", photo.error().message());  // on a system without ImageIO
        return 0;
    }
    println("{}x{}, alpha: {}, profile: {} bytes", photo->width(), photo->height(),
            photo->format() == codec::pixel_format::rgba8, photo->icc().size());

    auto small = codec::heif::encode(*photo, {.quality = 40});
    auto large = codec::heif::encode(*photo);
    if (small && large) {
        println("quality 40 is smaller than 85: {}", small->size() < large->size());
    }

    auto refused = codec::heif::encode(*photo, {.quality = 0});
    println(refused.error().message());
}
```

On macOS; on other systems the program prints the message of `errc::unsupported` and ends.

Output:

```text
32x32, alpha: true, profile: 572 bytes
quality 40 is smaller than 85: true
offset 0: heif: quality outside 1..100
```

## See also

[`image`](image.md), [`decode`](decode.md), [`error`](error.md), [`webp`](webp.md).
