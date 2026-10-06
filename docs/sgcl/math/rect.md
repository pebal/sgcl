[sgcl](../README.md) › [math](README.md)

# sgcl::math::rect

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct rect;
}
```

`sgcl::math::rect` is a rectangle of the plane with its sides along the axes, four floats `x`, `y`, `width` and
`height` — as CSS, SVG and a `DOMRect` write one, and as Core Graphics' `CGRect` holds it. The type does not care
whether `y` grows downwards (a screen) or upwards (a drawing): `top` is the smaller `y`. [int_rect](int_rect.md) is
the form for pixels, and [rounded_out](#member-functions) takes one to the other.

## Rules

- **Half-open.** A rectangle holds the points with `left <= x < right` and `top <= y < bottom`: two tiles that share
  an edge do not overlap, and a point on the edge belongs to one of them.
- **Empty when it covers nothing.** A width or a height of zero or below (or NaN) is empty: it contains no point and
  no rectangle, intersects nothing, and `united` ignores it. `intersection` of two that do not meet is the empty
  rectangle of all zeros.
- **A value.** Four floats, no pointer: it lives anywhere and is copied as it is.

## Member objects

| Member | Description |
|---|---|
| `float x` | the left edge; 0 by default |
| `float y` | the top edge; 0 by default |
| `float width` | the width; 0 by default |
| `float height` | the height; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the empty rectangle at the origin; `rect(float x, float y, float width, float height)`; `rect(const point& origin, const size& extent)`; `explicit rect(const int_rect&)` |
| `from_points` | `static rect from_points(const point& a, const point& b)`: the rectangle with two opposite corners, in any order |
| `left`, `top`, `right`, `bottom` | the edges: `x`, `y`, `x + width`, `y + height` |
| `origin`, `extent`, `center` | the top-left corner as a [point](point.md), the [size](size.md), the middle |
| `is_empty` | whether it covers nothing |
| `contains` | `bool contains(const point&) const`: half-open, as above; `bool contains(const rect&) const`: the other lies wholly inside, neither empty |
| `intersects` | `bool intersects(const rect&) const`: the two share a point, neither empty |
| `intersection` | `rect intersection(const rect&) const`: the part both cover, or the empty rectangle |
| `united` | `rect united(const rect&) const`: the smallest rectangle covering both, an empty one ignored |
| `translated` | `rect translated(float dx, float dy) const`: moved |
| `inflated` | `rect inflated(float dx, float dy) const`: grown by `dx` on the left and the right and `dy` on the top and the bottom; shrunk by negative ones |
| `rounded_out` | `int_rect rounded_out() const`: the smallest [int_rect](int_rect.md) holding it, the edges rounded outwards and held to `int32_t` |

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
    math::rect a(0, 0, 100, 50);
    math::rect b(80, 40, 50, 50);
    math::rect both = a.intersection(b);
    math::rect all = a.united(b);
    println("{} {} {} {}", both.x, both.y, both.width, both.height);
    println("{} {} {} {}", all.x, all.y, all.width, all.height);
    println("{} {}", a.contains(math::point(0, 0)), a.contains(math::point(100, 0)));
    println("{} {}", a.intersects(math::rect(100, 0, 10, 10)), math::rect(5, 5, 0, 5).is_empty());
    math::int_rect pixels = math::rect(0.5f, 0.25f, 10, 10).rounded_out();
    println("{} {} {} {}", pixels.x, pixels.y, pixels.width, pixels.height);
}
```

Output:

```text
80 40 20 10
0 0 130 90
true false
false true
0 0 11 11
```

## See also

- [point](point.md), [size](size.md): its corner and its extent
- [int_rect](int_rect.md): the rectangle of pixels
- [affine](affine.md): `apply(rect)`, the box of a transformed rectangle
- [README: math](README.md)
