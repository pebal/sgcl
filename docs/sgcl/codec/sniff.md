[sgcl](../README.md) › [codec](README.md)

# sgcl::codec::sniff

```cpp
#include "sgcl/codec/format.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    optional<format> sniff(const slice<const byte>& head) noexcept;
}
```

Tells the [format](format.md) of a file by its signature, from its first bytes: what [decode](decode.md),
[decode_frames](decode_frames.md) and [load](load.md) look at before they choose a decoder. Twelve bytes are enough
for each format but HEIF and AVIF under a general brand, whose `ftyp` box is read for its compatible brands (`decode`
of a stream reads up to 64 bytes for it).

| Format | Signature |
|---|---|
| `png` | `89 50 4E 47 0D 0A 1A 0A` |
| `jpeg` | `FF D8 FF`: SOI and the first byte of the next marker |
| `gif` | `GIF87a` or `GIF89a` |
| `webp` | `RIFF`, 4 bytes of size, `WEBP` |
| `heif` | an `ftyp` box first whose major brand is `heic`, `heix`, `hevc`, `hevx`, `heim`, `heis`, `hevm` or `hevs`; under any other major brand (`mif1`, `msf1`, `miaf`, `isom`, …), the first compatible brand of HEIF or AVIF within the box, when it is one of these |
| `avif` | the same with the brand `avif` or `avis` |

It says what the file claims to be, not what it is: a file with the signature of PNG and a broken body is `png` here,
and an error of `decode`.

## Parameters

| Parameter | Description |
|---|---|
| `head` | the first bytes of the file, or all of it |

## Return value

The format the bytes claim; `nullopt` for any other bytes, or for too few to tell.

## Complexity

Constant for a signature; for an `ftyp` box under a general brand, linear in its compatible brands within `head`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(64, 48, codec::pixel_format::rgb8);
    vector<byte> png = codec::png::encode(picture);
    vector<byte> jpeg = codec::jpeg::encode(picture);
    vector<byte> heic = io::read_file("tests/codec/fuzz/seeds/heif_decode/own.heic");
    println("{}", codec::sniff(png) == codec::format::png);
    println("{}", codec::sniff(jpeg) == codec::format::jpeg);
    println("{}", codec::sniff(heic) == codec::format::heif);
    println("{}", codec::sniff(png.as_slice(0, 4)).has_value());  // too few bytes to tell

    vector<byte> broken = png;
    broken.resize(20);  // the signature and half of IHDR
    println("{}", codec::sniff(broken) == codec::format::png);
    println(codec::decode(broken).error().message());
}
```

Output:

```text
true
true
true
false
true
offset 20: png: the data ends in the middle
```

## See also

- [format](format.md): what it returns
- [decode](decode.md): the image of a file of any format, told by `sniff`
- [codec](README.md)
