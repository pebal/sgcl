[sgcl](../README.md) › [math](README.md)

# sgcl::math::smoothstep

```cpp
float smoothstep(float edge0, float edge1, float x) noexcept;        // (1)
double smoothstep(double edge0, double edge1, double x) noexcept;    // (2)
```

GLSL's `smoothstep`: 0 at and below `edge0`, 1 at and above `edge1`, and between them `3t² − 2t³` of the fraction
`t` of the way — a step whose ends have no slope, for fading and blending. Edges that meet are a plain step at them
(0 below, 1 at and above), where GLSL leaves the result undefined; `edge0` above `edge1` runs the other way, 1 below
`edge1` and 0 above `edge0`. The header is `sgcl/math/interpolation.h`.

## Parameters

| Parameter | Description |
|---|---|
| `edge0` | where the step starts, 0 |
| `edge1` | where it ends, 1 |
| `x` | the value |

## Return value

The eased fraction, in [0, 1].

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
    println("{} {} {} {}", math::smoothstep(0.0f, 1.0f, -1.0f), math::smoothstep(0.0f, 1.0f, 0.25f),
            math::smoothstep(0.0f, 1.0f, 0.5f), math::smoothstep(0.0f, 1.0f, 2.0f));
    println("{} {}", math::smoothstep(1.0f, 1.0f, 0.5f), math::smoothstep(1.0f, 1.0f, 1.0f));
}
```

Output:

```text
0 0.15625 0.5 1
0 1
```

## See also

- [lerp](lerp.md), [inverse_lerp](inverse_lerp.md): the straight forms
- [easing](easing.md): the timing functions of animations
- [README: math](README.md)
