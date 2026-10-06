[sgcl](../README.md) › [math](README.md)

# sgcl::math::quaternion

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct quaternion;
}
```

`sgcl::math::quaternion` is a rotation of 3D space as a unit quaternion, four floats: `x`, `y` and `z` the vector
part, `w` the scalar — glm's `quat`, Metal's `simd_quatf`. A rotation is kept as one where a scene turns things
again and again: a product of two is sixteen multiplications, it does not drift away from a rotation as a matrix
does (a [normalized](#member-functions) call puts it back on the sphere), and `slerp` turns from one to another at an
even pace. The identity by default.

## Rules

- **`q * r` turns by `r`, then by `q`**, as the matrices compose; `to_mat4()` of the product is the product of the
  matrices.
- **The same turn as [mat4::rotation](mat4.md):** `from_axis_angle(axis, radians)` turns counter-clockwise looking
  from the tip of the axis.
- **A zero quaternion is not a rotation:** `normalized()` and `inverse()` of one give the identity, as does
  `from_axis_angle` of a zero axis.

## Member objects

| Member | Description |
|---|---|
| `float x`, `float y`, `float z` | the vector part, the axis times the sine of half the angle; 0 by default |
| `float w` | the scalar part, the cosine of half the angle; 1 by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the identity; `quaternion(float x, float y, float z, float w)` |
| `identity` | `static quaternion identity()` |
| `from_axis_angle` | `static quaternion from_axis_angle(const vec3& axis, float radians)`: the turn about the axis (normalized here) |
| `operator*=` | `quaternion& operator*=(const quaternion& r)`: `*this = *this * r` |
| `conjugate` | the vector part negated: the turn back, for a unit quaternion |
| `inverse` | the conjugate over the squared length: the turn back of any nonzero one |
| `dot`, `length` | the dot product of the four numbers, and the length |
| `normalized` | of length one |
| `rotate` | `vec3 rotate(const vec3& v) const`: the vector turned, `v + 2w(u × v) + 2u × (u × v)` with `u` the vector part |
| `slerp` | `quaternion slerp(const quaternion& to, float t) const`: the turn a fraction `t` of the way to `to`, along the shorter arc at an even pace; a normalized linear blend when the two are within a hair |
| `to_mat3`, `to_mat4` | the rotation matrix of a unit quaternion |

## Non-member functions

| Function | Description |
|---|---|
| `operator*` | `quaternion operator*(const quaternion& l, const quaternion& r)`: Hamilton's product, `r` first |
| `operator==` | the four numbers equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::quaternion quarter = math::quaternion::from_axis_angle({0, 0, 1}, 1.5707964f);
    math::vec3 v = quarter.rotate({1, 0, 0});
    println("{:.3f} {:.3f} {:.3f}", v.x, v.y, v.z);
    math::quaternion half = quarter * quarter;
    println("{:.3f}", half.rotate({1, 0, 0}).x);
    math::quaternion eighth = math::quaternion().slerp(quarter, 0.5f);
    math::vec3 w = eighth.rotate({1, 0, 0});
    println("{:.3f} {:.3f}", w.x, w.y);
    math::vec3 m = quarter.to_mat3() * math::vec3(1, 0, 0);
    println("{:.3f} {:.3f}", m.x, m.y);
}
```

Output:

```text
0.000 1.000 0.000
-1.000
0.707 0.707
0.000 1.000
```

## See also

- [mat4](mat4.md): the rotation as a matrix, and its other transforms
- [vec3](vec3.md): what a quaternion turns
- [README: math](README.md)
