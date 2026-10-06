[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_scientific

```cpp
string to_scientific() const;
```

The shortest decimal that reads back to this value, as [to_string](to_string.md) chooses it, always in scientific form:
the first digit, a point and the others when there are any, `e`, the sign and at least two digits of exponent —
`1e-01`, `1.5e+02`, `-3.75e+00`, `0e+00`; `+Inf` and `-Inf`. Go's `Text('e', -1)`. A number of digits after the
point is asked through [txt::format](../../txt/format.md), `{:.10e}`.

## Parameters

None.

## Return value

The text.

## Complexity

As [to_string](to_string.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_float(0.1).to_scientific(), math::big_float(150).to_scientific(),
            math::big_float(-3.75).to_scientific());
}
```

Output:

```text
1e-01 1.5e+02 -3.75e+00
```

## See also

- [to_string](to_string.md): plain while the exponent is small
- [format_value](format_value.md): `{:.10e}`
- [sgcl::math::big_float](README.md)
