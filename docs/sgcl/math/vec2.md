[sgcl](../README.md) › [math](README.md)

# sgcl::math::vec2

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct vec2;
}
```

`sgcl::math::vec2` is a vector of two floats — GLSL's and Metal's `vec2`, glm's `vec2`. The arithmetic of vectors with
the operators: `+` and `-` of two, `*` and `/` by a float, and `*` of two componentwise, as GLSL multiplies two
vectors; the products and the lengths as methods. A plain value of floats with no pointer in it, laid out as the
floats one after another.

## Member objects

| Member | Description |
|---|---|
| `float x`, `float y` | the components; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the zero vector; `vec2(float x, float y)`; `explicit vec2(const point&)`, a point as a vector |
| `operator point` | `explicit operator point() const`: the vector as a [point](point.md), `math::point(v)` |
| `dot` | `float dot(const vec2& v) const`: the dot product |
| `cross` | `float cross(const vec2& v) const`: the z of the cross product of the two in 3D, the signed area of the parallelogram they span, positive when `v` turns left of this one |
| `length`, `length_squared` | the length, and its square (no root) |
| `distance` | `float distance(const vec2& v) const`: the length of the difference |
| `normalized` | `vec2 normalized() const`: of length one in the same direction; a zero vector stays zero |
| `operator+=`, `operator-=`, `operator*=`, `operator/=` | `+=` and `-=` by a vector, `*=` and `/=` by a float |
| `operator-` | the vector reversed |

## Non-member functions

| Function | Description |
|---|---|
| `operator+`, `operator-` | the sum and the difference of two vectors |
| `operator*` | componentwise of two vectors; by a float on either side |
| `operator/` | by a float |
| `operator==` | every component equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::vec2 a(3, 4);
    math::vec2 b(1, 0);
    println("{} {} {} {}", a.length(), a.dot(b), b.cross(a), a.normalized().x);
    math::vec2 c = a * b + 2 * b;
    println("{} {} {}", c.x, c.y, math::vec2().normalized() == math::vec2());
}
```

Output:

```text
5 3 4 0.6
5 0 true
```

## See also

- [mat4](mat4.md), [mat3](mat3.md): the matrices that transform vectors
- [quaternion](quaternion.md): rotate() of a vec3
- [README: math](README.md)
