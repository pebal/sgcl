[sgcl](../../README.md) › [codec](../README.md) › [metadata](README.md)

# sgcl::codec::metadata::read

```cpp
static expected<metadata, error> read(const slice<const byte>& file,                  // (1)
                                      const limits& l = {}) noexcept;
static expected<metadata, error> read(const io::reader& in, const limits& l = {});    // (2)
```

Reads the metadata of a file without decoding its pixels: the format told by its signature ([sniff](../sniff.md)),
its container walked to the EXIF block and the XMP packet, and their fields typed.

1. Reads the file in memory, in place.
2. Reads the stream to its end, then the file as (1): a TIFF's or a HEIF's metadata lies anywhere in it.

- JPEG: APP1 `Exif` and APP1 of XMP, the markers before the first scan.
- PNG: `eXIf` and the iTXt of `XML:com.adobe.xmp`, compressed or not.
- WebP: the `EXIF` and `XMP ` chunks.
- TIFF: the file is the EXIF structure, its IFD0, Exif IFD and GPS IFD read in place; XMP from tag 700.
- HEIF and AVIF: the meta box's item of type `Exif` and its item of type `mime` `application/rdf+xml`, found
  through `iinf` and read where `iloc` puts them (in the file or in `idat`, in one extent or several).
- JPEG XL: the container's `Exif` and `xml ` boxes, or either compressed in a `brob` box (Brotli, what `cjxl`
  writes by default); a bare codestream carries none.
- GIF, BMP, ICO, QOI and the Netpbm formats carry none: metadata of nothing, not an error.

Lenient where a file is broken, as exiftool and ImageIO are: a block that ends early, a field of a wrong type or a
value past its block is read past, and what is whole is read; a container that breaks after its metadata gives it.

## Parameters

| Parameter | Description |
|---|---|
| `file` | the bytes of the file |
| `in` | the stream the file is read from |
| `l` | the size an EXIF block or an XMP packet may have (`max_metadata`, 64 MB) and, for a stream, the size it may have (eight bytes a pixel of `max_pixels` and 64 MB); the default limits unless told |

## Return value

The metadata, or the [error](../error/README.md): `errc::unsupported` for bytes of no format the module reads,
`errc::too_large` for an EXIF block or an XMP packet past `l.max_metadata` (an XMP packet as it decompresses), and
(2) for a stream longer than its limit, or `errc::io` when the stream fails.

## Complexity

Linear in the size of the containers walked and of the blocks: a JPEG's markers before its scan, a PNG's chunk headers, a WebP's chunks, the boxes of HEIF's meta.

## Exceptions

- (1) None.
- (2) What the stream's `read` throws: `in` calls the `read` of the object it is bound to.

## Notes

The memory is the metadata's own: the blocks copied, the fields typed; a PNG's compressed packet decompressed once. (2) holds the file while it reads it.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> file = io::read_file("tests/codec/fuzz/seeds/metadata/camera.jpg");
    expected<codec::metadata, codec::error> m = codec::metadata::read(file);
    println("{} {}", m->make().value_or("none"), m->model().value_or("none"));

    io::buffer in(file);
    println("from a stream: ISO {}", codec::metadata::read(in)->iso().value_or(0));

    vector<byte> png = codec::png::encode(codec::image(4, 4, codec::pixel_format::gray8));
    println("a PNG of none: {}", codec::metadata::read(png)->make().has_value());
    println("{}", codec::metadata::read(file, {.max_metadata = 100}).error().message());
}
```

Output:

```text
Canon Canon EOS R5
from a stream: ISO 400
a PNG of none: false
offset 0: metadata: an EXIF block or an XMP packet past limits.max_metadata
```

## See also

- [load](load.md): the metadata of the file at a path
- [from_exif](from_exif.md): of a block in hand
- [limits](../limits.md)
- [sgcl::codec::metadata](README.md)
