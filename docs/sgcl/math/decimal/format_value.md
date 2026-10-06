[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::format_value (sgcl::math::decimal)

```cpp
void format_value(txt::format_sink& out, const decimal& v, const txt::format_spec& spec);
```

Writes a decimal for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which find
it beside the class. The specification:

- `{}` and `{:f}` write [to_string](to_string.md): `"1.50"`, `"1500"`;
- `{:.2f}` or `{:.2}` write the value at that many places, [rescaled](rescale.md) half-even: `{:.2f}` of `2.675` is
  `"2.68"`;
- `{:e}` writes [to_scientific](to_scientific.md), every digit; `{:.3e}` rounds to that many digits after the point,
  half-even, and pads with zeros to them: `{:.3e}` of `1.5` is `"1.500e+0"` (Python's `format`);
- `+` or a space writes that sign before a value not below zero;
- the width, the fill and the alignment of any number, right by default; a `0` before the width puts the zeros after
  the sign, `{:08.2f}` of `-3.14159` is `"-0003.14"`.

NaN and the infinities write `NaN`, `Infinity` and `-Infinity` whatever the type and precision. Any other type,
`{:x}` or `{:d}`, is an error: of the compiler in a literal pattern, and `nullopt` from a runtime one
([txt::runtime](../../txt/format.md)). Which specifications a decimal takes is said by the specialization
`txt::formatter<decimal>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `v` | the decimal written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

As [to_string](to_string.md), with a [rescale](rescale.md) or a [round_precision](round_precision.md) before it
when a precision is given.

## Exceptions

`length_error` when the text has more characters than a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    math::decimal d("2.675");
    println("[{}] [{:.2f}] [{:.0f}] [{:e}] [{:.1e}]", d, d, d, d, d);
    println("[{:+}] [{:>8}] [{:*<8}] [{:08.2f}]", d, d, d, math::decimal("-3.14159"));
    println("{}", txt::format(txt::runtime("{:x}"), d).has_value());
}
```

Output:

```text
[2.675] [2.68] [3] [2.675e+0] [2.7e+0]
[+2.675] [   2.675] [2.675***] [-0003.14]
false
```

## See also

- [to_string](to_string.md), [to_scientific](to_scientific.md): the texts written
- [rescale](rescale.md): the places of `{:.2f}`
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::math::decimal](README.md)
