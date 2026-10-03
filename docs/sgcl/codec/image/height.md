[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::height

```cpp
uint32_t height() const noexcept;
```

The height of the image in pixels, at least 1: the number of rows, which [row](row.md) takes from 0 to `height()`
− 1. It is the height the rows are stored in; an image whose [orientation](orientation.md) is 5 to 8 is shown with
its sides swapped, as [oriented](oriented.md) makes it.

## Parameters

None.

## Return value

The height in pixels.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    codec::image picture(4, 3, codec::pixel_format::gray8);
    for (int y : range(picture.height())) {
        picture.row(y)[0] = byte(y * 100);
    }
    println("{} rows, {} bytes", picture.height(), picture.pixels().size());
    println("{}", std::to_integer<int>(picture.row(picture.height() - 1)[0]));
}
```

Output:

```text
3 rows, 12 bytes
200
```

## See also

- [width](width.md): the other side
- [row](row.md): a row by its number
- [sgcl::codec::image](README.md)
