[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::format_value (sgcl::math::big_float)

```cpp
void format_value(txt::format_sink& out, const big_float& v, const txt::format_spec& spec);
```

Writes a value for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`). The
specification:

- `{}` writes [to_string](to_string.md), the shortest decimal that reads back;
- `{:e}`, `{:f}`, `{:g}` write the fewest digits that read back in those forms (`{:e}` is
  [to_scientific](to_scientific.md)), `{:.30e}`, `{:.30f}`, `{:.30g}` that many digits after the point (significant
  ones for `g`, which takes `e` for an exponent below −4 or of at least the digits), rounded half-even from the exact
  value — Go's formats; `{:.30}` as `{:.30g}`;
- `{:a}` writes [to_hex](to_hex.md), `{:.5a}` with five hexadecimal digits after the point, rounded half-even;
- `+` or a space writes that sign before a number not below zero;
- the width, the fill and the alignment of any number, right by default.

The infinities are written `+Inf` and `-Inf` whatever is asked. Any other type is an error of the compiler in a
literal pattern.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `v` | the value written |
| `spec` | the specification of the field |

## Return value

None.

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
    math::big_float pi = math::big_float::parse("3.14159265358979323846264338327950288",
                                                120).value();
    println("[{}] [{:.10f}] [{:.3e}] [{:+.4g}]", pi, pi, pi, pi);
    println("[{:>12.5f}] [{}] [{:a}]", -pi, math::big_float::infinity(), math::big_float(12));
}
```

Output:

```text
[3.14159265358979323846264338327950288] [3.1415926536] [3.142e+00] [+3.142]
[    -3.14159] [+Inf] [0x1.8p+3]
```

## See also

- [to_string](to_string.md): the texts written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::math::big_float](README.md)
