[sgcl](../README.md) › [math](README.md)

# sgcl::math::mat3

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct mat3;
}
```

`sgcl::math::mat3` is a 3×3 matrix of floats, column-major — GLSL's `mat3`: a rotation of 3D space (the normal
matrix of a mesh), or a transform of the plane in homogeneous coordinates, which an [affine](affine.md) converts to
(`mat3(t)`, with a last row of 0 0 1). The identity by default. Columns are vectors, so `m * v` applies the matrix
to a column vector and `a * b` applies `b` first; `data()` gives the nine floats column after column, as a shader
takes them.

## Member objects

| Member | Description |
|---|---|
| `vec3 columns[3]` | the columns; those of the identity by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the identity; `mat3(const vec3& c0, const vec3& c1, const vec3& c2)`, the columns; `explicit mat3(const affine& t)`, the transform with the row 0 0 1 below |
| `identity` | `static mat3 identity()` |
| `operator()` | `float operator()(int row, int column) const`: one number |
| `operator*=` | `mat3& operator*=(const mat3& b)`: `*this = *this * b` |
| `transposed` | rows and columns exchanged |
| `determinant` | `float determinant() const`: the triple product of the columns |
| `inverse` | `optional<mat3> inverse() const`: from the cofactors; nothing for a determinant of zero or one that is not finite |
| `data` | `const float* data() const`: the nine floats, column after column |

## Non-member functions

| Function | Description |
|---|---|
| `operator*` | `vec3 operator*(const mat3&, const vec3&)`: the matrix applied; `mat3 operator*(const mat3& a, const mat3& b)`: `b` first, then `a` |
| `operator==` | every number equal |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::mat3 m(math::affine::translation(10, 20) * math::affine::scaling(2, 3));
    math::vec3 p = m * math::vec3(1, 1, 1);
    println("{} {} {} {}", p.x, p.y, p.z, m.determinant());
    math::vec3 back = *m.inverse() * p;
    println("{:.3f} {:.3f} {}", back.x, back.y, m(0, 2));
}
```

Output:

```text
12 23 1 6
1.000 1.000 10
```

## See also

- [mat4](mat4.md): the 4×4 matrix of 3D space
- [affine](affine.md): the transform of the plane in six numbers
- [quaternion](quaternion.md): to_mat3() of a rotation
- [README: math](README.md)
