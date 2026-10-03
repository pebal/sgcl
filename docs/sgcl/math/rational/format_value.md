[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::format_value (sgcl::math::rational)

```cpp
void format_value(txt::format_sink& out, const rational& v, const txt::format_spec& spec);
```

Writes a fraction for [txt::format](../../txt/format.md) and the functions built on it (`println`, `print`), which
find it beside the class. The specification:

- `{}` writes [to_string](to_string.md): `"3/4"`, `"-5"`;
- `{:f}`, `{:.5f}` or `{:.5}` write [to_decimal](to_decimal.md) with that many places, six when none is given, as
  for a `double`: rounded half away from zero;
- `+` or a space writes that sign before a value not below zero;
- the width, the fill and the alignment of any number, right by default; a `0` before the width puts the zeros
  after the sign, `{:08.3f}` of `-1/8` is `"-000.125"`.

Any other type, `{:x}` or `{:d}`, is an error: of the compiler in a literal pattern, and `nullopt` from a runtime
one ([txt::runtime](../../txt/format.md)). Which specifications a fraction takes is said by the specialization
`txt::formatter<rational>`, for the pattern checked where it is compiled.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the text is written |
| `v` | the fraction written |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

As [to_string](to_string.md), or [to_decimal](to_decimal.md) with a precision or `f`.

## Exceptions

`length_error` when the text has more digits than a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    math::rational r(3, 4);
    println("[{}] [{:f}] [{:.2f}] [{:.0f}]", r, r, r, r);
    println("[{:+}] [{:>8}] [{:*<8}] [{:08.3f}]", r, r, r, math::rational(-1, 8));
    println("{}", txt::format(txt::runtime("{:x}"), r).has_value());
}
```

Output:

```text
[3/4] [0.750000] [0.75] [1]
[+3/4] [     3/4] [3/4*****] [-000.125]
false
```

## See also

- [to_string](to_string.md), [to_decimal](to_decimal.md): the texts written
- [txt::format](../../txt/format.md): the patterns and the fields
- [sgcl::math::rational](../rational.md)
