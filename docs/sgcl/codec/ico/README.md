[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::ico

```cpp
#include "sgcl/codec/ico.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class ico;
}
```

`sgcl::codec::ico` reads and writes ICO and CUR, Windows' icons and cursors: a directory of entries, each a PNG or
a BMP with an AND mask, several sizes of one picture. [decode](decode.md) gives the largest entry,
[decode_all](decode_all.md) every entry, [encode](encode.md) writes one image or several, as an icon or a cursor
by its [options](../ico-options.md). Every member is static. [codec::decode](../decode.md) reads them too, and
[save](../save.md) writes them for `.ico` and `.cur`.

## Rules

- **The entries.** An entry whose data begins with PNG's signature is read as PNG, any other as a BMP without its
  file header: its height doubled, an AND mask after the pixels that makes a pixel transparent where the pixels
  carry no alpha of their own (32-bit entries whose alpha is all zero, and every entry of fewer bits).
- **The largest** is the entry of the most pixels, then of the most bits; the directory's sizes are not trusted
  further: the image is the size its data says.
- **A stream** is read to its end before the directory is read, since the entries lie wherever it points.
- **Encoding** writes PNG inside for a side of 256 and a 32-bit BMP with its mask below; a side past 256 is
  `errc::invalid_argument`. A cursor's entries carry the hotspot of its options.
- **Errors are values**: an [expected](../../core/expected/README.md) with an [error](../error/README.md).

## Member types

| Type | Definition |
|---|---|
| [options](../ico-options.md) | an icon or a cursor, and its hotspot |

## Member functions

| Function | Description |
|---|---|
| [decode](decode.md) | the largest entry of an ICO or CUR file (static) |
| [decode_all](decode_all.md) | every entry of an ICO or CUR file (static) |
| [encode](encode.md) | an ICO or CUR of one image or several (static) |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<codec::image> sizes;
    for (int side : {16, 32, 256}) {
        sizes.push_back(codec::image(side, side, codec::pixel_format::rgba8));
    }
    vector<byte> file = codec::ico::encode(sizes);
    vector<codec::image> back = codec::ico::decode_all(file);
    println("{} entries; the largest {}x{}", back.size(), codec::ico::decode(file)->width(),
            codec::ico::decode(file)->height());
}
```

Output:

```text
3 entries; the largest 256x256
```

## See also

- [bmp](../bmp/README.md), [png](../png/README.md): the formats of the entries
- [decode](../decode.md), [save](../save.md): any format, told by the signature or by the extension
- [image](../image/README.md), [error](../error/README.md)
