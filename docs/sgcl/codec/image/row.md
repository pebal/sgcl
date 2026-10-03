[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::row

```cpp
slice<byte> row(uint32_t y);                // (1)
slice<const byte> row(uint32_t y) const;    // (2)
```

Row `y` of the image, counted from the top: the [stride](stride.md) bytes of its pixels, starting `y` × `stride()`
bytes into [pixels](pixels.md).

1. The row to read and write.
2. The row to read.

The [slice](../../core/slice.md) holds the block of the pixels, as one from [pixels](pixels.md) does: it keeps the
whole image's pixels alive after the image is gone.

## Parameters

| Parameter | Description |
|---|---|
| `y` | the number of the row, from 0 at the top to [height](height.md) − 1 |

## Return value

A slice of [stride](stride.md) bytes.

## Complexity

Constant.

## Exceptions

`out_of_range` when `y` is not less than [height](height.md).

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(2, 2, codec::pixel_format::rgb8);
    slice<byte> bottom = picture.row(1);
    bottom[3] = byte(255);  // the red of the second pixel
    println("{} bytes, byte 9 of the image {}", bottom.size(),
            std::to_integer<int>(picture.pixels()[9]));
    try {
        picture.row(2);
    } catch (const std::out_of_range& e) {
        println(e.what());
    }
}
```

Output:

```text
6 bytes, byte 9 of the image 255
sgcl::codec::image::row
```

## See also

- [pixels](pixels.md): every row
- [height](height.md): the number of rows
- [sgcl::codec::image](../image.md)
