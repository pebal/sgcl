[sgcl](../../README.md) › [codec](../README.md)

# sgcl::codec::qr

```cpp
#include "sgcl/codec/qr.h"   // or "sgcl/codec.h"

namespace sgcl::codec {
    class qr;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::codec::qr` makes QR codes as ISO/IEC 18004:2015 has them (Model 2, versions 1 to 40, the four levels of error
correction): `codec::qr::encode("https://example.com")->to_image().save("qr.png")`. [encode](encode.md) cuts the
text into segments of the four modes — numeric, alphanumeric, byte and Kanji — for the fewest bits, takes the
smallest version that holds them at the [level](../qr-level.md) asked and the mask of least penalty; the modules come
as [dark](dark.md), as an [image](to_image.md) or as [SVG](to_svg.md). Go's standard library makes none; the symbols
are CoreImage's module for module where CoreImage makes the same segments.

## Rules

- **Generation only.** No reading, no Micro QR, no structured append, no FNC1.
- **The text is UTF-8.** Characters of Shift JIS's Kanji ranges (JIS X 0208) may take Kanji mode, 13 bits each,
  where a byte segment would take 24; a byte segment that holds what is not ASCII gets an ECI of UTF-8 (26) in
  front, as the standard asks for a character set other than its default, so that readers do not take the bytes as
  ISO-8859-1. Both are [options](../qr-options.md). Bytes that are not a text are encoded as they are, in one byte
  segment.
- **Errors are values** for data: `errc::too_large` when the largest version allowed does not hold it. Options
  outside their ranges are a contract of the program, `invalid_argument`.
- **A copy shares** the modules, which never change once made; it is a handle of one tracked word.

## Member types

| Type | Definition |
|---|---|
| [level](../qr-level.md) | the four levels of error correction |
| [options](../qr-options.md) | the level, the versions, the mask, Kanji mode, ECI |

## Member functions

| Function | Description |
|---|---|
| [encode](encode.md) | a QR code of a text or of bytes (static) |

#### The symbol

| Function | Description |
|---|---|
| [correction](correction.md) | the level of error correction written |
| [dark](dark.md) | whether a module is dark |
| [mask](mask.md) | the mask pattern, 0 to 7 |
| [size](size.md) | modules a side |
| [version](version.md) | the version, 1 to 40 |

#### Output

| Function | Description |
|---|---|
| [to_image](to_image.md) | the symbol as a gray image with its quiet zone |
| [to_svg](to_svg.md) | the symbol as SVG |

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr code = codec::qr::encode("https://example.com/sgcl");
    println("version {}, {} modules a side, mask {}", code.version(), code.size(), code.mask());
    for (uint32_t y : range(7)) {
        for (uint32_t x : range(code.size())) {
            print("{}", code.dark(x, y) ? '#' : '.');
        }
        println("");
    }
}
```

Output:

```text
version 2, 25 modules a side, mask 2
#######..#####..#.#######
#.....#...#..####.#.....#
#.###.#.#.#...#...#.###.#
#.###.#.#.#.####..#.###.#
#.###.#.#..#.#..#.#.###.#
#.....#.#.#.#.##..#.....#
#######.#.#.#.#.#.#######
```

## See also

- [options](../qr-options.md), [level](../qr-level.md)
- [image](../image/README.md), [save](../save.md): the symbol written as PNG or any format
