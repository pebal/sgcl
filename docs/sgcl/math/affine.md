[sgcl](../README.md) › [math](README.md)

# sgcl::math::affine

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct affine;
}
```

`sgcl::math::affine` is an affine transform of the plane — a turn, a scaling, a skew and a move, in any combination —
as six floats: `x' = a·x + c·y + e`, `y' = b·x + d·y + f`, the numbers of SVG's `matrix(a, b, c, d, e, f)` and of a
canvas's `setTransform` in that order. It is Core Graphics' `CGAffineTransform`, Skia's `SkMatrix` without the
perspective and Qt's `QTransform` of the plane. The identity by default.

## Rules

- **Composed as matrices are.** `l * r` applies `r` first, then `l`: `(l * r).apply(p)` is `l.apply(r.apply(p))`. So
  SVG's `transform="translate(100, 50) rotate(90)"` is `translation(100, 50) * rotation(π/2)`, read in the order it is
  written, and `t *= r` puts `r` before what `t` did, as a canvas's `rotate()` after `translate()` does.
- **Angles in radians**, positive counter-clockwise when `y` grows upwards — clockwise on a screen, whose `y` grows
  downwards, which is SVG's `rotate`.
- **A singular transform has no inverse.** `inverse()` is `nullopt` for a determinant of zero (everything onto a line
  or a point) or one that is not finite; it is computed in double, so a transform of large and small parts loses no
  more than its own rounding.
- **A value.** Six floats, no pointer.

## Member objects

| Member | Description |
|---|---|
| `float a`, `float b` | the image of the unit step along x: `(a, b)`; 1 and 0 by default |
| `float c`, `float d` | the image of the unit step along y: `(c, d)`; 0 and 1 by default |
| `float e`, `float f` | the move: where the origin goes; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `identity` | `static affine identity()`: the transform that moves nothing |
| `translation` | `static affine translation(float dx, float dy)`: a move |
| `scaling` | `static affine scaling(float sx, float sy)`: a scaling about the origin |
| `rotation` | `static affine rotation(float radians)`: a turn about the origin; `static affine rotation(float radians, const point& center)`: about a point, SVG's `rotate(angle, cx, cy)` |
| `skew_x`, `skew_y` | `static affine skew_x(float radians)`: SVG's `skewX`, `x' = x + tan(angle)·y`; `skew_y` likewise for `y` |
| `operator*=` | `affine& operator*=(const affine& r)`: `*this = *this * r`, `r` applied first |
| `determinant` | `float determinant() const`: `a·d − b·c`, the factor by which areas grow (negative: mirrored) |
| `inverse` | `optional<affine> inverse() const`: the transform undoing it, or nothing when it is singular |
| `apply` | `point apply(const point&) const`: the point transformed; `rect apply(const rect&) const`: the smallest rectangle holding the rectangle transformed, the box of its four corners |
| `apply_vector` | `point apply_vector(const point&) const`: a step rather than a place, the move left out |
| `is_identity` | whether it is the identity, exactly |

## Non-member functions

| Function | Description |
|---|---|
| `operator*` | `affine operator*(const affine& l, const affine& r)`: `r` first, then `l` |
| `operator==` | the six numbers equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    // SVG's transform="translate(100, 50) rotate(90)"
    math::affine t = math::affine::translation(100, 50) * math::affine::rotation(1.5707964f);
    math::point p = t.apply({10, 0});
    println("{:.1f} {:.1f}", p.x, p.y);
    math::rect box = t.apply(math::rect(0, 0, 20, 10));
    println("{:.1f} {:.1f} {:.1f} {:.1f}", box.x, box.y, box.width, box.height);
    math::point back = t.inverse()->apply(p);
    println("{:.1f} {:.1f}", back.x, back.y);
    println("{}", math::affine::scaling(0, 1).inverse().has_value());
}
```

Output:

```text
100.0 60.0
90.0 50.0 10.0 20.0
10.0 0.0
false
```

## See also

- [point](point.md), [rect](rect.md): what is transformed
- [mat3](mat3.md): the same transform as a 3×3 matrix, `mat3(t)`
- [README: math](README.md)
