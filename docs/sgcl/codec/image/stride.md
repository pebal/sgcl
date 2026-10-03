[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::stride

```cpp
size_t stride() const noexcept;
```

The bytes of a row: the [width](width.md) times the bytes of a pixel of the [format](format.md), with no padding at
the end of the row. Row `y` starts `y` × `stride()` bytes into [pixels](pixels.md), and the pixels are `stride()` ×
[height](height.md) bytes in all.

## Parameters

None.

## Return value

The bytes of a row.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(5, 2, codec::pixel_format::rgb8);
    println("{} bytes a row, {} in all", picture.stride(), picture.pixels().size());
    println("{}", picture.convert(codec::pixel_format::rgba16).stride());
    println("{}", picture.convert(codec::pixel_format::cmyk8).stride());
}
```

Output:

```text
15 bytes a row, 30 in all
40
20
```

## See also

- [row](row.md): one row, `stride()` bytes
- [pixel_format](../pixel_format.md): the bytes of a pixel of each format
- [sgcl::codec::image](README.md)
