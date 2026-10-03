[sgcl](../../README.md) › [codec](../README.md) › [image](README.md)

# sgcl::codec::image::pixels

```cpp
slice<byte> pixels() noexcept;                // (1)
slice<const byte> pixels() const noexcept;    // (2)
```

Every row of the image, from the top, [stride](stride.md) bytes each with no gap between them: the pixels as the
[format](format.md) lays them out.

1. The pixels to read and write.
2. The pixels to read.

The [slice](../../core/slice/README.md) holds the block of the pixels: it keeps them alive after the image and every copy
of it are gone, as a Go slice keeps its array. A write through it is seen through every copy of the image, since
copies share the pixels.

## Parameters

None.

## Return value

A slice of [stride](stride.md) × [height](height.md) bytes.

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

slice<byte> gray_ramp() {
    codec::image ramp(4, 1, codec::pixel_format::gray8);
    slice<byte> levels = ramp.pixels();
    for (int x : range(4)) {
        levels[x] = byte(x * 85);
    }
    return levels;
}

int main() {
    slice<byte> levels = gray_ramp();
    vector<int> values;
    for (byte b : levels) {
        values.push_back(std::to_integer<int>(b));
    }
    println("{}", values);
}
```

Output:

```text
[0, 85, 170, 255]
```

## See also

- [row](row.md): one row
- [stride](stride.md): the bytes of a row
- [sgcl::codec::image](README.md)
