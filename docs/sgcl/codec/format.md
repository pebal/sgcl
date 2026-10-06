[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::format

```cpp
#include "sgcl/codec/format.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    enum class format : uint8_t {
        png,
        jpeg,
        gif,
        webp,
        heif,
        avif,
        bmp,
        tiff,
        ico,
        qoi,
        pnm,
        jxl
    };
}
```

The file formats the module reads, as [sniff](sniff.md) tells them apart by their first bytes. [decode](decode.md)
and [load](load.md) read a file of each, told this way; [save](save.md) writes every one but AVIF and JPEG XL, told by the
extension of the path rather than by a `format`. A format has no name as text: `{}` writes the number of the
enumerator, `2` for `gif`, and a program that prints a name writes its own.

| Value | Description |
|---|---|
| `png` | PNG, read and written ([png](png/README.md)) |
| `jpeg` | JPEG, read and written ([jpeg](jpeg/README.md)) |
| `gif` | GIF87a and GIF89a, read and written, an animation through [frames](frames/README.md) ([gif](gif/README.md)) |
| `webp` | WebP, read and written, an animation through [frames](frames/README.md) ([webp](webp/README.md)) |
| `heif` | HEIC and HEIF, read and written through the system's codec where there is one ([heif](heif/README.md)) |
| `avif` | AVIF, read through the system's codec as HEIF is ([heif](heif/README.md)) |
| `bmp` | BMP, read and written ([bmp](bmp/README.md)) |
| `tiff` | TIFF, read and written, every page through `decode_all` ([tiff](tiff/README.md)) |
| `ico` | ICO and CUR, read and written, every entry through `decode_all` ([ico](ico/README.md)) |
| `qoi` | QOI, read and written ([qoi](qoi/README.md)) |
| `pnm` | PBM, PGM, PPM and PAM, read and written ([pnm](pnm/README.md)) |
| `jxl` | JPEG XL, read through the system's codec as HEIF is ([jxl](jxl/README.md)) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

const char* name(const optional<codec::format>& f) {
    if (!f) {
        return "unknown";
    }
    switch (*f) {
        case codec::format::png: return "PNG";
        case codec::format::jpeg: return "JPEG";
        case codec::format::gif: return "GIF";
        case codec::format::webp: return "WebP";
        case codec::format::heif: return "HEIF";
        case codec::format::avif: return "AVIF";
        case codec::format::bmp: return "BMP";
        case codec::format::tiff: return "TIFF";
        case codec::format::ico: return "ICO";
        case codec::format::qoi: return "QOI";
        case codec::format::pnm: return "Netpbm";
        case codec::format::jxl: return "JPEG XL";
    }
    return "unknown";
}

int main() {
    for (const char* path : {"tests/codec/fuzz/seeds/gif_decode/fire.gif",
                             "tests/codec/fuzz/seeds/webp_decode/lossless4.webp",
                             "tests/codec/fuzz/seeds/heif_decode/basn2c08.avif",
                             "LICENSE"}) {
        vector<byte> file = io::read_file(path);
        println("{}: {}", path, name(codec::sniff(file)));
    }
}
```

Output:

```text
tests/codec/fuzz/seeds/gif_decode/fire.gif: GIF
tests/codec/fuzz/seeds/webp_decode/lossless4.webp: WebP
tests/codec/fuzz/seeds/heif_decode/basn2c08.avif: AVIF
LICENSE: unknown
```

## See also

- [sniff](sniff.md): the format of a file's first bytes
- [decode](decode.md): the image of a file of any format
- [codec](README.md)
