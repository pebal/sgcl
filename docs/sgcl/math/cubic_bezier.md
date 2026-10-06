[sgcl](../README.md) › [math](README.md)

# sgcl::math::cubic_bezier

```cpp
#include "sgcl/math/bezier.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct cubic_bezier;
}
```

`sgcl::math::cubic_bezier` is a cubic Bézier curve of the plane: from `p0`, leaving towards `p1`, arriving from the
direction of `p2`, at `p3` — the `C` of an SVG path, the curve of a PostScript or CFF glyph, of a vector drawing.
What a renderer asks of one is here: the point and the tangent at a parameter `t` from 0 to 1, a split into two
cubics, the tight bounding box, a polyline within a tolerance and the length. A value of four [point](point.md)s;
[quadratic_bezier](quadratic_bezier.md) is the curve of one control point.

## Rules

- **Flattened by Wang's formula.** The polyline is the curve at evenly spaced parameters, as many as Wang's formula
  asks: with `M` the longest of the second differences `p0 − 2p1 + p2` and `p1 − 2p2 + p3`, `n` segments keep every
  chord within `3M / (4n²)` of the curve, so `n = ceil(sqrt(3M / (4·tolerance)))`. The count is known before a point
  is computed (`segments`) and the bound holds for every curve. It is held to 2¹⁶ segments; a tolerance of zero, below
  or NaN is taken as a ten-thousandth of the curve's extent.
- **Appended for a path.** `flatten(out, tolerance)` appends all but `p0`, which a path has already as the end of the
  segment before, so a path's segments make one polyline.
- **The length** is the integral of the speed `|B'(t)|` by Gauss–Legendre's rule of five points, on halves of halves
  until two levels agree within the tolerance, at most twenty levels down.
- **The bounds** are of the curve, not of the control points: its ends and the parameters where the derivative of a
  coordinate is zero, the roots of a quadratic.

## Member objects

| Member | Description |
|---|---|
| `point p0` | the start |
| `point p1` | the first control point: the curve leaves `p0` towards it |
| `point p2` | the second control point: the curve arrives at `p3` from its direction |
| `point p3` | the end |

## Member functions

| Function | Description |
|---|---|
| `at` | `point at(float t) const`: the point at `t` (0: `p0`, 1: `p3`), by de Casteljau's construction |
| `derivative` | `point derivative(float t) const`: the derivative, the tangent's direction and the speed |
| `split` | `pair<cubic_bezier, cubic_bezier> split(float t) const`: the two halves at `t`, the first from `p0` to `at(t)` and the second from there to `p3` |
| `bounds` | `rect bounds() const`: the smallest rectangle holding the curve |
| `segments` | `size_t segments(float tolerance = 0.25f) const`: how many segments `flatten` makes |
| `flatten` | `vector<point> flatten(float tolerance = 0.25f) const`: a polyline within the tolerance, `p0` to `p3`; `void flatten(vector<point>& out, float tolerance = 0.25f) const`: the same appended, but `p0` |
| `length` | `float length(float tolerance = 1e-3f) const`: the arc length |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | the same control points |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::cubic_bezier hump{{0, 0}, {0, 100}, {100, 100}, {100, 0}};
    math::rect box = hump.bounds();
    println("{} {} {} {}", box.x, box.y, box.width, box.height);
    println("{} {:.3f}", hump.segments(0.25f), hump.length());

    // two segments of a path into one polyline
    math::quadratic_bezier tail{{100, 0}, {150, -50}, {200, 0}};
    vector<math::point> path = hump.flatten(1.0f);
    tail.flatten(path, 1.0f);
    println("{} {}", path.size(), path[path.size() - 1].x);

    auto [left, right] = hump.split(0.5f);
    println("{} {}", left.p3.x, right.p0.y);
}
```

Output:

```text
0 0 100 75
21 200.000
17 200
50 75
```

## See also

- [quadratic_bezier](quadratic_bezier.md): the curve of one control point
- [easing](easing.md): CSS's cubic-bezier(), a cubic of the progress
- [point](point.md), [rect](rect.md): what the curve is made of and its bounds
- [README: math](README.md)
