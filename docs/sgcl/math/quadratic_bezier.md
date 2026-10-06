[sgcl](../README.md) › [math](README.md)

# sgcl::math::quadratic_bezier

```cpp
#include "sgcl/math/bezier.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct quadratic_bezier;
}
```

`sgcl::math::quadratic_bezier` is a quadratic Bézier curve of the plane: from `p0` towards `p1` and on to `p2`, the
`Q` of an SVG path and the curve of a TrueType glyph. It has the members of [cubic_bezier](cubic_bezier.md), whose
page says how they work — a point and the tangent at a parameter, a split, the tight bounding box, a polyline within a
tolerance and the length — and one more, `to_cubic`, the same curve as a cubic. A value of three [point](point.md)s.

## Member objects

| Member | Description |
|---|---|
| `point p0` | the start |
| `point p1` | the control point, which the curve leans towards |
| `point p2` | the end |

## Member functions

| Function | Description |
|---|---|
| `at` | `point at(float t) const`: the point at `t` (0: `p0`, 1: `p2`), by de Casteljau's construction |
| `derivative` | `point derivative(float t) const`: the derivative, the tangent's direction and the speed |
| `split` | `pair<quadratic_bezier, quadratic_bezier> split(float t) const`: the two halves at `t` |
| `bounds` | `rect bounds() const`: the smallest rectangle holding the curve, from its ends and its extremes |
| `segments` | `size_t segments(float tolerance = 0.25f) const`: how many segments `flatten` makes, by Wang's formula with `d(d − 1)/8 = 1/4` |
| `flatten` | `vector<point> flatten(float tolerance = 0.25f) const`: a polyline within the tolerance, `p0` to `p2`; `void flatten(vector<point>& out, float tolerance = 0.25f) const`: the same appended, but `p0` |
| `length` | `float length(float tolerance = 1e-3f) const`: the arc length |
| `to_cubic` | `cubic_bezier to_cubic() const`: the same curve as a cubic, its control points two thirds of the way from the ends to `p1` |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | the same control points |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::quadratic_bezier arch{{0, 0}, {50, 100}, {100, 0}};
    math::point top = arch.at(0.5f);
    math::rect box = arch.bounds();
    println("{} {} {} {}", top.x, top.y, box.width, box.height);
    println("{:.3f} {}", arch.length(), arch.flatten(0.5f).size());
    math::cubic_bezier same = arch.to_cubic();
    println("{:.3f} {:.3f}", same.at(0.5f).y, same.p1.y);
}
```

Output:

```text
50 50 100 50
147.894 11
50.000 66.667
```

## See also

- [cubic_bezier](cubic_bezier.md): the cubic, and how flattening and lengths work
- [point](point.md), [rect](rect.md): what the curve is made of and its bounds
- [README: math](README.md)
