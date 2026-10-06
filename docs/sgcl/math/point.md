[sgcl](../README.md) › [math](README.md)

# sgcl::math::point

```cpp
#include "sgcl/math/geometry.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct point;
}
```

`sgcl::math::point` is a point of the plane, two floats `x` and `y`, or a step across it: the difference of two
points is a point too, as a `CGPoint` or Skia's `SkPoint` is used. The coordinates of a user interface and of SVG are
floats, and so are these; [int_point](int_point.md) is the form for pixels. A plain value with no pointer in it,
copied as two floats, with the arithmetic of a vector.

## Member objects

| Member | Description |
|---|---|
| `float x` | the horizontal coordinate; 0 by default |
| `float y` | the vertical coordinate; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the origin; `point(float x, float y)`; `explicit point(const int_point&)`, the pixel's coordinates; a [vec2](vec2.md) converts explicitly, `math::point(v)` |
| `distance` | `float distance(const point& other) const`: the distance between the two (`std::hypot`) |
| `operator+=`, `operator-=` | moves the point by a step |
| `operator*=`, `operator/=` | scales both coordinates by a float |
| `operator-` | the point mirrored through the origin |

## Non-member functions

| Function | Description |
|---|---|
| `operator+`, `operator-` | the sum and the difference of two points |
| `operator*`, `operator/` | both coordinates times or over a float, the float on either side of `*` |
| `operator==` | both coordinates equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::point a(1, 2);
    math::point b(4, 6);
    math::point step = b - a;
    println("{} {} {}", step.x, step.y, a.distance(b));
    math::point middle = (a + b) / 2;
    println("{} {} {}", middle.x, middle.y, middle == math::point(2.5f, 4));
}
```

Output:

```text
3 4 5
2.5 4 true
```

## See also

- [size](size.md), [rect](rect.md): the other shapes of the plane
- [affine](affine.md): a transform of points
- [vec2](vec2.md): the vector of the algebra
- [README: math](README.md)
