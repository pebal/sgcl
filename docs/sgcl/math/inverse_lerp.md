[sgcl](../README.md) › [math](README.md)

# sgcl::math::inverse_lerp

```cpp
float inverse_lerp(float a, float b, float v) noexcept;        // (1)
double inverse_lerp(double a, double b, double v) noexcept;    // (2)
```

Where `v` lies on the way from `a` to `b`, the `t` that [lerp](lerp.md) would take to give it: `(v − a)/(b − a)`, 0 at
`a`, 1 at `b`. Not clamped: a value beyond `b` gives more than 1, one before `a` less than 0. When `a == b` every value
is as far as any other, and the answer is 0 rather than a division by zero. The header is
`sgcl/math/interpolation.h`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the start, where the result is 0 |
| `b` | the end, where the result is 1 |
| `v` | the value placed |

## Return value

The fraction.

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
    // a temperature placed on a scale from 15 to 35 degrees
    println("{} {} {}", math::inverse_lerp(15.0f, 35.0f, 20.0f),
            math::inverse_lerp(15.0, 35.0, 45.0),
            math::inverse_lerp(5.0f, 5.0f, 7.0f));
}
```

Output:

```text
0.25 1.5 0
```

## See also

- [lerp](lerp.md): the value of a fraction
- [smoothstep](smoothstep.md): the fraction clamped and eased
- [README: math](README.md)
