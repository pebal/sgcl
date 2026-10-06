[sgcl](../README.md) › [math](README.md)

# sgcl::math::int_rect

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct int_rect;
}
```

`sgcl::math::int_rect` is a rectangle of whole pixels, four `int32_t` `x`, `y`, `width` and `height`: the part of an
image to copy, the damaged area of a window, a cell range of a grid. It has the members and the rules of
[rect](rect.md) — half-open, empty when a side is zero or below — over whole numbers. The right and bottom edges are
computed in 64 bits, so a rectangle reaching past `INT32_MAX` still compares and intersects right; a result that
does not fit an `int32_t` (a union of rectangles more than 2³¹ apart) is cut as a conversion cuts.
[rect::rounded_out](rect.md) gives the one that holds a rectangle of floats.

## Member objects

| Member | Description |
|---|---|
| `int32_t x` | the left edge; 0 by default |
| `int32_t y` | the top edge; 0 by default |
| `int32_t width` | the width; 0 by default |
| `int32_t height` | the height; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the empty rectangle at the origin; `int_rect(int32_t x, int32_t y, int32_t width, int32_t height)`; `int_rect(const int_point& origin, const int_size& extent)` |
| `from_points` | `static int_rect from_points(const int_point& a, const int_point& b)`: the rectangle with two opposite corners |
| `left`, `top`, `right`, `bottom` | the edges as `int64_t` |
| `origin`, `extent` | the top-left corner as an [int_point](int_point.md), the [int_size](int_size.md) |
| `is_empty` | whether it covers nothing |
| `contains` | a pixel (half-open), or a whole rectangle, neither empty |
| `intersects` | the two share a pixel |
| `intersection` | the pixels both cover, or the empty rectangle |
| `united` | the smallest rectangle covering both, an empty one ignored |
| `translated` | `int_rect translated(int32_t dx, int32_t dy) const`: moved |
| `inflated` | `int_rect inflated(int32_t dx, int32_t dy) const`: grown on each side, shrunk by negative ones |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | the four numbers equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::int_rect image(0, 0, 640, 480);
    math::int_rect dirty(600, 400, 100, 100);
    math::int_rect copy = image.intersection(dirty);
    println("{} {} {} {}", copy.x, copy.y, copy.width, copy.height);
    println("{} {}", image.contains(math::int_point(639, 479)), image.contains(math::int_point(640, 0)));
    math::int_rect far(INT32_MAX - 10, 0, 100, 1);
    println(far.right());
}
```

Output:

```text
600 400 40 80
true false
2147483737
```

## See also

- [rect](rect.md): the rectangle in floats, and its rules
- [int_point](int_point.md), [int_size](int_size.md): its corner and its extent
- [README: math](README.md)
