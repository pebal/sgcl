[sgcl](../README.md) › [math](README.md)

# sgcl::math::lerp

```cpp
float lerp(float a, float b, float t) noexcept;                  // (1)
double lerp(double a, double b, double t) noexcept;              // (2)
point lerp(const point& a, const point& b, float t) noexcept;    // (3)
vec2 lerp(const vec2& a, const vec2& b, float t) noexcept;       // (4)
vec3 lerp(const vec3& a, const vec3& b, float t) noexcept;       // (5)
vec4 lerp(const vec4& a, const vec4& b, float t) noexcept;       // (6)
```

The value a fraction `t` of the way from `a` to `b`, `a + (b − a)·t`: `a` at 0, `b` at 1, the middle at 0.5, and past
the ends for `t` outside [0, 1].

- (1–2) The numbers by `std::lerp`, which promises what the plain formula does not: exactly `a` at `t = 0` and
  exactly `b` at `t = 1`, a result monotonic in `t`, and `a` again for every `t` when `a == b`.
- (3–6) Each coordinate by (1): a [point](point.md) and the vectors [vec2](vec2.md), [vec3](vec3.md),
  [vec4](vec4.md). A rotation is interpolated by [quaternion](quaternion.md)'s `slerp`, not by its numbers.

The header is `sgcl/math/interpolation.h`, brought by `sgcl/math.h`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the value at `t = 0` |
| `b` | the value at `t = 1` |
| `t` | how far from `a` to `b` |

## Return value

The value between, or beyond.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::lerp(10.0f, 20.0f, 0.25f), math::lerp(10.0, 20.0, 1.5),
            math::lerp(0.1f, 0.7f, 1.0f));
    math::point p = math::lerp(math::point(0, 100), math::point(100, 0), 0.5f);
    println("{} {}", p.x, p.y);
}
```

Output:

```text
12.5 25 0.7
50 50
```

## See also

- [inverse_lerp](inverse_lerp.md): the fraction of a value
- [smoothstep](smoothstep.md): an eased fraction
- [easing](easing.md): the timing functions of animations
- [README: math](README.md)
