[sgcl](../README.md) › [math](README.md)

# sgcl::math::vec4

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct vec4;
}
```

`sgcl::math::vec4` is a vector of four floats: a point or a direction in homogeneous coordinates (`w` 1 or 0), a
colour with its alpha — GLSL's and Metal's `vec4`, glm's `vec4`. The arithmetic of vectors with the operators: `+` and
`-` of two, `*` and `/` by a float, and `*` of two componentwise, as GLSL multiplies two vectors; the products and the
lengths as methods. A plain value of floats with no pointer in it, laid out as the floats one after another.

## Member objects

| Member | Description |
|---|---|
| `float x`, `float y`, `float z`, `float w` | the components; 0 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the zero vector; `vec4(float x, float y, float z, float w)`; `vec4(const vec3& v, float w)` |
| `dot` | `float dot(const vec4& v) const`: the dot product |
| `xyz` | `vec3 xyz() const`: the first three |
| `length`, `length_squared` | the length, and its square (no root) |
| `distance` | `float distance(const vec4& v) const`: the length of the difference |
| `normalized` | `vec4 normalized() const`: of length one in the same direction; a zero vector stays zero |
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
    math::vec4 p(math::vec3(1, 2, 3), 1);
    math::vec4 q = p * 2 - math::vec4(0, 0, 0, 1);
    println("{} {} {} {}", q.x, q.y, q.z, q.w);
    println("{} {}", p.xyz().length_squared(), math::vec4(1, 1, 1, 1).length());
}
```

Output:

```text
2 4 6 1
14 2
```

## See also

- [mat4](mat4.md), [mat3](mat3.md): the matrices that transform vectors
- [quaternion](quaternion.md): rotate() of a vec3
- [README: math](README.md)
