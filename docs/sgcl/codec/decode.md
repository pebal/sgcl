# sgcl::codec::decode, decode_frames, sniff, format

```cpp
#include "sgcl/codec/decode.h"   // or "sgcl/codec/codec.h", "sgcl/sgcl.h"

namespace sgcl::codec {
    enum class format : uint8_t { png, jpeg, gif, webp, heif, avif };

    optional<format> sniff(const slice<const byte>& head) noexcept;

    expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
    expected<image, error> decode(const io::reader& in, const decode_options& o = {});
    expected<codec::frames, error> decode_frames(const slice<const byte>& data, const decode_options& o = {});   // GIF, WebP
    expected<codec::frames, error> decode_frames(const io::reader& in, const decode_options& o = {});
}
```

**sniff** tells the format of a file by its signature, from its first bytes (12 are enough for each, but HEIF and AVIF under a general brand take their `ftyp` box, up to 64):

| format | signature |
|---|---|
| PNG | `89 50 4E 47 0D 0A 1A 0A` |
| JPEG | `FF D8 FF` |
| GIF | `GIF87a` or `GIF89a` |
| WebP | `RIFF`, 4 bytes of size, `WEBP` |
| HEIF | an `ftyp` box whose brand is `heic`, `heix`, `hevc`, `hevx`, `heim`, `heis`, `hevm` or `hevs`; under `mif1`, `msf1` or `miaf`, the first compatible brand of HEIF or AVIF |
| AVIF | the same with `avif` or `avis` |

It returns `nullopt` for anything else, or for too few bytes to tell. It says what the file claims to be: a file with a PNG signature and a broken body is `png` here, and an error of `decode`.

**decode** reads any file of the module's formats: [`png`](png.md), [`jpeg`](jpeg.md), [`webp`](webp.md), the first frame of a [`gif`](gif.md), and HEIF and AVIF through the system's codec ([`heif`](heif.md)). A file of no format it knows is `errc::unsupported`. From a stream it reads the first bytes to tell the format and hands the stream on to the format's decoder without reading it twice. [`decode_options`](README.md#limits) says the pixel format wanted, the limits and whether to keep the metadata.

| `decode_options` | the default | what it sets |
|---|---|---|
| `want` | the file's own format | the pixel format of the result, each row converted as it is decoded |
| `limits.max_pixels` | 100 million | the most pixels a file may claim (an animation's canvas); past it `errc::too_large` |
| `limits.max_metadata` | 64 MB | the most bytes of one block of EXIF or ICC |
| `metadata` | `true`: EXIF and ICC read | `false` leaves both unread: empty, orientation 1 |

A plain struct, filled by designated initializers in the order of its fields; each format's own `decode` takes the same.

**decode_frames** reads an animation, GIF or WebP (a still WebP is one frame), told by its signature, as [`gif::frames`](gif.md) and [`webp::frames`](webp.md) read it; any other format is `errc::unsupported`.

## Members

### sniff

```cpp
optional<format> sniff(const slice<const byte>& head) noexcept;
```

The format a file's first bytes claim, by the signatures in the table above; `nullopt` for none, or for too few bytes to tell.

### decode

```cpp
expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {});
expected<image, error> decode(const io::reader& in, const decode_options& o = {});
```

The image of a file of any of the module's formats, from its bytes or from a stream; of a GIF, its first frame. A format it does not know is `errc::unsupported`.

### decode_frames

```cpp
expected<codec::frames, error> decode_frames(const slice<const byte>& data, const decode_options& o = {});
expected<codec::frames, error> decode_frames(const io::reader& in, const decode_options& o = {});
```

The frames of a GIF or a WebP, read one by one; a still WebP is one frame.

### format

```cpp
enum class format : uint8_t { png, jpeg, gif, webp, heif, avif };
```

The formats `sniff` tells apart.

### decode_options

```cpp
struct decode_options {
    optional<pixel_format> want;   // nullopt: the file's own format
    codec::limits limits;          // max_pixels = 100'000'000, max_metadata = 64 MB
    bool metadata = true;          // EXIF and ICC read
};
```

The pixel format wanted, the limits and whether the metadata is read, with the defaults of the table above.

## Example

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    for (const char* path : {"tests/codec/fuzz/seeds/jpeg_decode/testorig.jpg",
                             "tests/codec/fuzz/seeds/png_decode/basn6a16.png",
                             "tests/codec/fuzz/seeds/gif_decode/treescap.gif",
                             "tests/codec/fuzz/seeds/webp_decode/lossless4.webp"}) {
        vector<byte> file = io::read_file(path);
        codec::image photo = codec::decode(file, {.want = codec::pixel_format::rgba8});
        const bool known = codec::sniff(file).has_value();
        println("{}x{} rgba8 from a file sniffed as known: {}", photo.width(), photo.height(),
                known);
    }
    vector<byte> zeros(16);
    auto unknown = codec::decode(zeros);
    if (!unknown) println(unknown.error().message());
}
```

Output:

```text
227x149 rgba8 from a file sniffed as known: true
32x32 rgba8 from a file sniffed as known: true
40x40 rgba8 from a file sniffed as known: true
256x256 rgba8 from a file sniffed as known: true
offset 0: not an image format the module reads
```

## See also

[`image`](image.md), [`error`](error.md), [`frames`](frames.md), and each format: [`png`](png.md), [`jpeg`](jpeg.md), [`gif`](gif.md), [`webp`](webp.md), [`heif`](heif.md).
