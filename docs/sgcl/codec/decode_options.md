[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::decode_options

```cpp
#include "sgcl/codec/options.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    struct decode_options {
        optional<pixel_format> want;
        codec::limits limits;
        bool metadata = true;
    };
}
```

`sgcl::codec::decode_options` is what differs from the defaults when a file is decoded: the pixel format wanted, the
[limits](limits.md) a file may not pass, and whether EXIF and ICC are read. [decode](decode.md), [load](load.md),
[decode_frames](decode_frames.md) and each format's own `decode` take it, but for [heif::decode](heif/decode.md),
which takes none; [decode](decode.md) of a HEIF or AVIF file applies them all the same. A plain struct, filled by
designated initializers in the order of its fields:
`codec::load(path, {.want = codec::pixel_format::rgba8, .metadata = false})`.

With `want`, a gray PNG, a CMYK JPEG and a GIF all come as the one format a program works in, each row converted as
it is decoded, with no second pass over the image ([image::convert](image/convert.md) says how).

## Member objects

| Member | Description |
|---|---|
| `want` | the [pixel format](pixel_format.md) of the result; `nullopt` by default, the file's own: a gray PNG stays `gray8` or `gray16`, a JPEG is `rgb8`, `gray8` or `cmyk8`, a GIF frame `rgba8`, a WebP `rgba8` or `rgb8` as it says it has alpha or not. One outside the list is `errc::invalid_argument` |
| `limits` | the most pixels a file may claim and the most bytes of metadata, 100 million and 64 MB by default ([limits](limits.md)) |
| `metadata` | `true` by default: EXIF and the ICC profile read; `false` leaves both unread, the image's [exif](image/exif.md) and [icc](image/icc.md) empty and its [orientation](image/orientation.md) 1 |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::gray16);
    vector<byte> png = codec::png::encode(picture);
    println("{}", codec::decode(png)->format() == codec::pixel_format::gray16);
    codec::image rgba = codec::decode(png, {.want = codec::pixel_format::rgba8});
    println("{}", rgba.format() == codec::pixel_format::rgba8);

    vector<byte> jpeg = io::read_file("tests/codec/fuzz/seeds/jpeg_decode/exif_9x7.jpg");
    codec::image photo = codec::decode(jpeg);
    println("EXIF {} bytes, orientation {}", photo.exif().size(), photo.orientation());
    codec::image bare = codec::decode(jpeg, {.metadata = false});
    println("EXIF {} bytes, orientation {}", bare.exif().size(), bare.orientation());
}
```

Output:

```text
true
true
EXIF 26 bytes, orientation 6
EXIF 0 bytes, orientation 1
```

## See also

- [limits](limits.md): the sizes a file may claim
- [pixel_format](pixel_format.md): the formats `want` names
- [decode](decode.md), [load](load.md): what takes them
- [codec](README.md)
