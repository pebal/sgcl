[sgcl](../README.md) › [math](README.md)

# sgcl::math::mat4

```cpp
#include "sgcl/math/algebra.h"   // or "sgcl/math.h"

namespace sgcl::math {
    struct mat4;
}
```

`sgcl::math::mat4` is a 4×4 matrix of floats, column-major — GLSL's and Metal's `float4x4`, glm's `mat4`: the
transforms of 3D space in homogeneous coordinates (a move, a turn, a scaling, a camera) and the projections onto the
screen. The identity by default. Columns are vectors, so `m * v` applies the matrix to a column vector, `a * b`
applies `b` first, and a model-view-projection is written `projection * view * model`; `data()` gives the sixteen
floats column after column, as a shader takes them.

## Rules

- **Right-handed, depth 0 to 1.** [look_at](#member-functions) makes a camera that looks down its −z; the
  projections map the visible depth to 0 (`z_near`) … 1 (`z_far`) in clip space, as Metal, Vulkan, Direct3D and WebGPU clip.
  OpenGL's −1 … 1 is the exception and takes a fixed matrix in front (`z' = 2z − w`). Vulkan's y of clip space points
  down; a scene for it flips y in the projection (`columns[1].y = -columns[1].y`).
- **Angles in radians**, a turn counter-clockwise looking from the tip of its axis towards the origin.
- **A singular matrix has no inverse.** `inverse()` is `nullopt` for a determinant of zero or one that is not finite;
  the cofactors and the determinant are computed in double.
- **On the processor's vectors.** The product of two matrices and `apply()` over a batch of vectors use NEON on
  arm64 (SSE2 on x86-64, its minimum), as two chains of multiply-adds by a lane summed at the end; they measured
  faster than the loop the compiler makes of the plain arithmetic ([benchmarks](benchmarks.md#algebra)). The results
  may differ from the plain arithmetic in the last bit, by the fused multiply-add.

## Member objects

| Member | Description |
|---|---|
| `vec4 columns[4]` | the columns; those of the identity by default |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the identity; `mat4(const vec4& c0, const vec4& c1, const vec4& c2, const vec4& c3)`, the columns |
| `identity` | `static mat4 identity()` |
| `translation` | `static mat4 translation(const vec3& offset)`: a move |
| `scaling` | `static mat4 scaling(const vec3& factors)`: a scaling about the origin |
| `rotation` | `static mat4 rotation(const vec3& axis, float radians)`: a turn about the axis (normalized here; a zero axis gives the identity), by Rodrigues' formula |
| `look_at` | `static mat4 look_at(const vec3& eye, const vec3& target, const vec3& up)`: the view of a camera at `eye` looking at `target`, `up` the direction that shows up |
| `perspective` | `static mat4 perspective(float fov_y, float aspect, float z_near, float z_far)`: `fov_y` the vertical angle of view, `aspect` the width over the height, `z_near` and `z_far` the distances of the planes, above zero; a `z_far` of infinity is the infinite projection (not `near` and `far`, which `windows.h` defines as macros) |
| `orthographic` | `static mat4 orthographic(float left, float right, float bottom, float top, float z_near, float z_far)`: the box onto clip space, depth 0 at −`z_near` and 1 at −`z_far` |
| `operator()` | `float operator()(int row, int column) const`: one number |
| `operator*=` | `mat4& operator*=(const mat4& b)`: `*this = *this * b` |
| `transform_point` | `vec3 transform_point(const vec3& p) const`: the point with w = 1, the result divided by its w (the perspective divide) |
| `transform_vector` | `vec3 transform_vector(const vec3& v) const`: a direction, w = 0, no move |
| `apply` | `void apply(const slice<vec4>& vectors) const`: every vector of the slice replaced by the matrix times it, the vertices of a mesh at once |
| `transposed` | rows and columns exchanged |
| `determinant` | `float determinant() const`: Laplace's expansion over the minors of the top and the bottom two rows |
| `inverse` | `optional<mat4> inverse() const`: the adjugate over the determinant; nothing when it is singular |
| `data` | `const float* data() const`: the sixteen floats, column after column |

## Non-member functions

| Function | Description |
|---|---|
| `operator*` | `vec4 operator*(const mat4&, const vec4&)`: the matrix applied; `mat4 operator*(const mat4& a, const mat4& b)`: `b` first, then `a` |
| `operator==` | every number equal |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::mat4 view = math::mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    math::mat4 projection = math::mat4::perspective(1.0f, 16.0f / 9, 0.1f, 100);
    math::mat4 mvp = projection * view * math::mat4::rotation({0, 1, 0}, 0.5f);
    math::vec3 center = mvp.transform_point({0, 0, 0});
    println("{:.3f} {:.3f} {:.4f}", center.x, center.y, center.z);

    vector<math::vec4> corners = {{1, 0, 0, 1}, {0, 1, 0, 1}};
    math::mat4::translation({10, 0, 0}).apply(corners);
    println("{} {}", corners[0].x, corners[1].x);
    println("{}", math::mat4::scaling({2, 2, 2}).determinant());
}
```

Output:

```text
0.000 0.000 0.9810
11 10
8
```

## See also

- [vec4](vec4.md), [vec3](vec3.md): what the matrix transforms
- [quaternion](quaternion.md): a rotation, to_mat4()
- [mat3](mat3.md): the 3×3 matrix
- [README: math](README.md)
