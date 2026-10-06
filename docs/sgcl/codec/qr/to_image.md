[sgcl](../../README.md) › [codec](../README.md) › [qr](README.md)

# sgcl::codec::qr::to_image

```cpp
image to_image(uint32_t scale = 8, uint32_t border = 4) const;
```

The symbol as a `gray8` [image](../image/README.md): each module a square of `scale` pixels, dark 0 and light 255,
inside a light border of `border` modules, the quiet zone the standard asks to be 4 modules wide. The image saves as
any format the module writes; PNG keeps it exact and small.

## Parameters

| Parameter | Description |
|---|---|
| `scale` | pixels a module, 1 or more; 8 unless told |
| `border` | the quiet zone in modules; 4 unless told |

## Return value

The image, `(size() + 2 · border) · scale` pixels a side.

## Complexity

Linear in the pixels of the image.

## Exceptions

`invalid_argument` for a scale of 0; `length_error` for an image past 2^31 − 1 pixels a side.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::qr code = codec::qr::encode("https://example.com");
    codec::image picture = code.to_image(4);
    println("{}x{}", picture.width(), picture.height());
    vector<byte> png = codec::png::encode(picture);
    println("PNG: {} bytes", png.size());
}
```

Output:

```text
132x132
PNG: 527 bytes
```

## See also

- [to_svg](to_svg.md): the symbol as SVG
- [png::encode](../png/encode.md), [save](../save.md)
- [sgcl::codec::qr](README.md)
