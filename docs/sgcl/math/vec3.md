[sgcl](../README.md) › [math](README.md)

# sgcl::math::vec3

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct vec3;
}
```

`sgcl::math::vec3` is a vector of three floats: a position, a direction, a normal, a colour — GLSL's and Metal's
`vec3`, glm's `vec3`. The arithmetic of vectors with the operators: `+` and `-` of two, `*` and `/` by a float, and
`*` of two componentwise, as GLSL multiplies two vectors; the products and the lengths as methods. A plain value of
floats with no pointer in it, laid out as the floats one after another.

## Member objects

| Member | Description |
|---|---|
| `float x`, `float y`, `float z` | the components; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the zero vector; `vec3(float x, float y, float z)`; `vec3(const vec2& v, float z)` |
| `dot` | `float dot(const vec3& v) const`: the dot product |
| `cross` | `vec3 cross(const vec3& v) const`: perpendicular to both by the right-hand rule, `x × y` is `z` |
| `length`, `length_squared` | the length, and its square (no root) |
| `distance` | `float distance(const vec3& v) const`: the length of the difference |
| `normalized` | `vec3 normalized() const`: of length one in the same direction; a zero vector stays zero |
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
    math::vec3 x(1, 0, 0);
    math::vec3 y(0, 1, 0);
    math::vec3 z = x.cross(y);
    println("{} {} {}", z.x, z.y, z.z);
    math::vec3 v(2, 3, 6);
    println("{} {} {}", v.length(), v.dot(x), v.distance(math::vec3()));
}
```

Output:

```text
0 0 1
7 2 7
```

## See also

- [mat4](mat4.md), [mat3](mat3.md): the matrices that transform vectors
- [quaternion](quaternion.md): rotate() of a vec3
- [README: math](README.md)
