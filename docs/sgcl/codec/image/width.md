[sgcl](../../README.md) › [codec](../README.md) › [image](../image.md)

# sgcl::codec::image::width

```cpp
uint32_t width() const noexcept;
```

The width of the image in pixels, at least 1: the number of pixels in a row. It is the width the rows are stored
in; an image whose [orientation](orientation.md) is 5 to 8 is shown with its sides swapped, as
[oriented](oriented.md) makes it.

## Parameters

None.

## Return value

The width in pixels.

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
    codec::image banner(728, 90, codec::pixel_format::rgb8);
    codec::image read = codec::png::decode(codec::png::encode(banner));
    println("{} pixels wide, {} a row", read.width(), read.stride());
}
```

Output:

```text
728 pixels wide, 2184 a row
```

## See also

- [height](height.md): the other side
- [stride](stride.md): the bytes of a row
- [sgcl::codec::image](../image.md)
