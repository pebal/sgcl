[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::image

```cpp
image(uint32_t width, uint32_t height, pixel_format f);
```

An image of `width` × `height` pixels of the format `f`, every byte zero: black, transparent where the format has
alpha, and white for `cmyk8`, whose zero is no ink. The pixels are one managed block of `height` ×
[stride](stride.md) bytes, the exact size.

The sides and the format are a contract of the program, not a condition of the data: a decoder checks the size a
file claims against its [limits](../limits.md) before it makes the image, and a file that claims too much is an
[errc](../errc.md)`::too_large`, not an exception.

## Parameters

| Parameter | Description |
|---|---|
| `width` | the width in pixels, at least 1 |
| `height` | the height in pixels, at least 1 |
| `f` | the [pixel format](../pixel_format.md) |

## Complexity

Linear in the number of bytes: they are zeroed.

## Exceptions

- `invalid_argument` when a side is zero or `f` is outside the list of [pixel_format](../pixel_format.md).
- `length_error` when the pixels would take more bytes than an address holds.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(640, 480, codec::pixel_format::rgba8);
    bool zero = picture.pixels().all([](byte b) { return b == byte(0); });
    println("{}x{}, {} bytes, all zero: {}", picture.width(), picture.height(),
            picture.pixels().size(), zero);
    try {
        codec::image line(0, 480, codec::pixel_format::rgba8);
    } catch (const std::invalid_argument& e) {
        println(e.what());
    }
}
```

Output:

```text
640x480, 1228800 bytes, all zero: true
sgcl::codec::image: a side of zero pixels
```

## See also

- [pixel_format](../pixel_format.md): the formats
- [clone](clone.md): a new image of the pixels of another
- [sgcl::codec::image](README.md)
