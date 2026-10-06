[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::parse

```cpp
static expected<big_float, parse_error> parse(const string& text, uint32_t precision = 64,
                                              rounding mode = rounding::half_even) noexcept;
```

Reads a number and rounds it once to `precision` bits by `mode`:

- a decimal, `"-1.5"`, `"2.5e-300"`, `".5"`, `"7."`, read exactly (as [rational](../rational/README.md)'s
  [parse](../rational/parse.md) reads it) and then rounded — correctly, where Go's `ParseFloat` scales by powers of
  ten at a working precision and may round twice;
- a hexadecimal mantissa with a binary exponent, `"0x1.8p3"`, `"0x.cp-1"`, `"-0x1p-1074"` — Go's `'p'` and `'x'`
  formats, read exactly;
- `"Inf"` or `"Infinity"`, any case, with a sign.

No space, no `_`. A decimal exponent past a million either way is an error of the data, as for rational.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |
| `precision` | the bits of the result |
| `mode` | how it is rounded to them |

## Return value

The value, or a [parse_error](../parse_error/README.md) with the offset where the reading stopped and the reason.

## Complexity

The reading of the digits as a whole number, a power of ten of the decimal exponent and one division.

## Exceptions

None. A precision of 0 for a text that is not zero is `invalid_argument` thrown from the rounding, as for the
[constructor](big_float.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float x = math::big_float::parse("3.14159265358979323846264338327950288",
                                               120).value();
    println("{} {}", x, x.precision());
    println(math::big_float::parse("0x1.8p3").value());
    println(math::big_float::parse("1.5", 1, rounding::down).value());
    println(math::big_float::parse("1.2.3").error().message());
}
```

Output:

```text
3.14159265358979323846264338327950288 120
12
1
not a digit in base 10 at byte 3
```

## See also

- [to_string](to_string.md): the text written back
- [(constructor)](big_float.md): a literal of the program
- [sgcl::math::big_float](README.md)
