[sgcl](../README.md) › [math](README.md)

# sgcl::math::int_point

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct int_point;
}
```

`sgcl::math::int_point` is a point of whole numbers, two `int32_t`: a pixel of an image, a cell of a grid. It is
[point](point.md) for what is counted rather than measured, as Go's `image.Point` is; a `point` is made from one
explicitly, `math::point(p)`. Its arithmetic is `int32_t`'s, an overflow included.

## Member objects

| Member | Description |
|---|---|
| `int32_t x` | the column; 0 by default |
| `int32_t y` | the row; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the origin; `int_point(int32_t x, int32_t y)` |
| `operator+=`, `operator-=` | moves the point by a step |
| `operator-` | the point mirrored through the origin |

## Non-member functions

| Function | Description |
|---|---|
| `operator+`, `operator-` | the sum and the difference of two points |
| `operator==` | both coordinates equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::int_point pixel(10, 20);
    pixel += math::int_point(1, -1);
    math::point at(pixel);
    println("{} {} {}", pixel.x, pixel.y, at.x / 2);
}
```

Output:

```text
11 19 5.5
```

## See also

- [point](point.md): the point in floats
- [int_rect](int_rect.md): a rectangle of pixels
- [README: math](README.md)
