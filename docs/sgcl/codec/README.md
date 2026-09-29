# sgcl::codec

Images: what Go has in `image/png`, `image/jpeg` and `image/gif`, and what libpng, libjpeg-turbo, giflib and libwebp decode and encode in C. `#include "sgcl/codec/codec.h"` brings the module in. It depends on:

- [`core`](../core/README.md);
- [`io`](../io/README.md) (the streams read and written);
- [`compress`](../compress/README.md) (zlib's DEFLATE for PNG, LZW for GIF);
- [`hash`](../hash/README.md) (CRC-32 and Adler-32 for PNG).

The index of the whole interface is [`docs/sgcl/`](../README.md).

Every format is written from its specification: PNG 3rd ed. (W3C), T.81 with JFIF 1.02 and Adobe's APP14, GIF89a, RFC 9649 and RFC 6386 for WebP, CIPA DC-008 for the orientation of EXIF. The reference libraries (libpng, libjpeg-turbo, giflib, libwebp) and Go's image packages are the oracles of the tests and nothing more:

- PNG decodes pixel for pixel as libpng and Go do, and the PNG encoder's filters are libpng's row for row.
- JPEG decodes bit for bit as `djpeg -dct int`, and the JPEG encoder writes what `cjpeg -dct int -baseline` writes, byte for byte.
- GIF's frames are giflib's and Go's pixel for pixel.
- Lossless WebP decodes to libwebp's and Go's pixels byte for byte. The container is taken and refused as libwebp's demuxer takes and refuses it.
- HEIF and AVIF are not decoded by the module: HEVC and AV1 come from the system (ImageIO on macOS), and the module's wrapping is held against ImageIO's own reading of each file.

The time each format takes against libpng, libjpeg-turbo, libwebp, giflib and Go is on the page of [benchmarks](benchmarks.md). The decoders are fuzzed (libFuzzer with ASan and UBSan, a harness for each). Their code has not been audited on its own: take files from outside as the input they are, with the [limits](#limits) set to what a program expects.

## One call for any file

```cpp
#include "sgcl/codec/codec.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    codec::image picture(320, 200, codec::pixel_format::rgb8);
    picture.save("picture.png");
    codec::image loaded = codec::load("picture.png");
    loaded.save("picture.jpg");
    println("{}x{}", loaded.width(), loaded.height());
}
```

Output:

```text
320x200
```

`codec::load` reads a file of any of the module's formats and tells the format from its first bytes ([`sniff`](decode.md)). `save` writes the format the path's extension names. Both return [`expected`](error.md): `loaded` above takes the image, and a file that does not load throws. A `save` whose result is not looked at, as above, fails quietly. [Files](#files) has the rules. For a file already in memory, `codec::decode` does what `load` does:

```cpp
vector<byte> file = io::read_file("photo.jpg");
codec::image photo = codec::decode(file);
```

[`decode_options`](decode.md) set what differs from the defaults, for `load` and `decode` alike. `want` asks for the pixels in one format whatever the file holds, so a gray PNG, a CMYK JPEG and a GIF all come as RGBA:

```cpp
codec::image rgba = codec::load("photo.png", {.want = codec::pixel_format::rgba8, .limits = {.max_pixels = 20'000'000}});
```

Without `want`, the file's own format is kept: a gray PNG stays `gray8` or `gray16`, a JPEG is `rgb8`, `gray8` or `cmyk8`, a GIF frame is `rgba8`, and a WebP is `rgba8` or `rgb8` as it says it has alpha or not. Each format also has a type of its own with `decode` and, where the module writes the format, `encode`:

```cpp
codec::image picture = codec::png::decode(file);
vector<byte> smaller = codec::jpeg::encode(picture, {.quality = 90});
```

## The formats

| format | read | write | notes |
|---|---|---|---|
| PNG | yes | yes | every type and depth, Adam7 |
| JPEG | yes | yes | baseline, extended and progressive read; baseline written |
| GIF | yes | no | animations through `frames` |
| WebP | yes | no | lossless, lossy, alpha, animations |
| HEIC | macOS | macOS | through the platform's ImageIO |
| AVIF | macOS | no | through the platform's ImageIO |

`image.save` and `codec::save` on a format without an encoder give `errc::unsupported`; `codec::load` and `codec::decode` read every format of the table.

## The types

| type | what | page |
|---|---|---|
| `codec::image` | an image in memory: its size, one of nine pixel formats, its rows, EXIF and ICC; `convert`, `clone`, `oriented`, `save` | [image](image.md) |
| `codec::load`, `codec::save`, `codec::save_options` | a file of any format read, told by its signature; an image written in the format its extension names | [below](#files) |
| `codec::decode`, `codec::decode_frames`, `codec::sniff`, `codec::format` | any file of the module's formats, told by its signature; the frames of an animation | [decode](decode.md) |
| `codec::png` | PNG: every type and depth, Adam7, tRNS, eXIf, iCCP; the encoder with adaptive filters | [png](png.md) |
| `codec::jpeg` | JPEG: baseline, extended and progressive, every sampling, CMYK and YCCK; the baseline encoder | [jpeg](jpeg.md) |
| `codec::gif` | GIF87a and GIF89a: the first frame, or all of them through `frames` | [gif](gif.md) |
| `codec::frames`, `codec::frame` | the frames of an animation, each the whole canvas, read one by one | [frames](frames.md) |
| `codec::error`, `codec::errc` | what went wrong in a file, and at which byte | [error](error.md) |
| `codec::limits`, `codec::decode_options` | how large a file may be, the format asked for, metadata or not | [below](#limits) |
| `codec::webp` | WebP: the container, animation, lossless and lossy images, alpha | [webp](webp.md) |
| `codec::heif` | HEIC, HEIF and AVIF through the system's codec (ImageIO on macOS): the first image, HEIC written | [heif](heif.md) |

## Files

```cpp
namespace sgcl::codec {
    struct save_options {
        compress::level level = 7;                                   // PNG: 0 stores, 1 fastest, 9 smallest
        int quality = 85;                                            // JPEG and HEIC: 1..100
        jpeg::subsampling subsampling = jpeg::subsampling::s420;     // JPEG
    };

    expected<image, error> load(const string& path, const decode_options& o = {});
    expected<void, error> save(const image& im, const string& path, const save_options& o = {});
    // image::save(path[, options]), the same as codec::save(image, path[, options]);
    // async_load, async_save and image::async_save for a task, on the blocking pool
}
```

`load` reads the whole file and decodes it as `decode` does: the format by the file's first bytes, whatever its name says. A file that does not read is `errc::io`, with io's error inside (`error().io_error()`); a file of no format the module reads is `errc::unsupported`.

`save` takes the format from the extension, in either case: `.png`, `.jpg` or `.jpeg`, and `.heic` or `.heif` where the system writes HEIC (macOS). `.gif`, `.webp` and `.avif` are read but not written: `errc::unsupported`, and so is any other extension. Nothing is written then. The file is written as `path + ".part"` and renamed over `path` when whole. A failure of the stream or of the encoder leaves no `.part`, and `path` as it was. Each field of `save_options` is for the formats it names:

```cpp
photo.save("small.jpg", {.quality = 70, .subsampling = codec::jpeg::subsampling::s420});
photo.save("fast.png", {.level = 1});
```

## Streams

Every `decode` also takes an [`io::reader`](../io/stream.md) and reads the file as it comes, so that memory is the image and a constant, not the file. The exception is a progressive JPEG, whose coefficients are kept whole until its last scan. Every `encode` also writes into an [`io::writer`](../io/stream.md) and returns `expected<void, codec::error>` (`errc::io` when the stream fails).

## limits

```cpp
struct limits {
    uint64_t max_pixels = 100'000'000;   // the pixels a file may claim
    size_t max_metadata = 64u << 20;      // the bytes of EXIF and ICC it may carry
};

struct decode_options {
    optional<pixel_format> want;         // the pixel format of the result; the file's own when not given
    codec::limits limits;
    bool metadata = true;                 // EXIF and ICC; false leaves both empty
};
```

A small file can claim a huge image. Every decoder checks the size a file declares against `max_pixels` before it allocates anything, and the metadata against `max_metadata` as it reads it. Past either it returns `errc::too_large`. Pillow warns from about 89 million pixels and refuses from about 179 million; Go has no limit.

## Examples

Each page ends with a program that runs as it is written, under `using namespace sgcl;` with every module named. A result used further has its type named (`codec::image photo = …`), and a variable is never named like a type. Some read files of the repository's tests, by their path from its root.
