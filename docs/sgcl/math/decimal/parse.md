[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::parse

```cpp
static expected<decimal, parse_error> parse(const string& text) noexcept;
```

Reads a decimal, and nothing else:

- a number, `"-12.50"`, `".5"`, `"7."`, `"1.5e3"`, `"-0.0012E-4"`: an optional sign, digits with an optional point
  (either side of it may be empty, not both), and an optional exponent, `e` or `E` with an optional sign and
  digits;
- `"NaN"`, `"Infinity"` or `"Inf"` in any case, the infinities with an optional sign — PostgreSQL's spellings and
  Python's.

No space, no `_` or `,`, no base prefix, no fraction with a slash. The scale is the text's: `"1.50"` is 150 at
scale 2, `"1.5e3"` is 15 at scale −2 and `"00012.3400"` is 123400 at scale 4, as Java's `BigDecimal` and Python's
`Decimal` read them; [trim_scale](trim_scale.md) takes the zeros off. `"-0"` is zero.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The decimal, or a [parse_error](../parse_error/README.md) whose offset is the byte where the reading stopped:

- `"empty text"`: an empty text;
- `"no digits after the sign"`: a sign alone, a point alone, an `e` with no digits after it;
- `"not a digit in base 10"`: any other byte where a digit was wanted — a space, a second point, a `/`, a sign
  before NaN, a word that is not one of the three;
- `"an exponent past a million"`: at the first byte after the `e` (at the first digit when there is no `e`), when
  the value's exponent in scientific form — the place of its first digit, the exponent
  [to_scientific](to_scientific.md) writes — is past a million either way: `"1e1000000"` is read, `"12345e999997"`
  is not. A dozen bytes of text would otherwise ask for megabytes when the value meets another; and the limit being
  on what `to_scientific` writes, every value read is written and read back.

## Complexity

Linear in the digits up to eighteen of them; past that, the reading of the digits as a
[big_integer](../big_integer/README.md).

## Exceptions

None.

## Notes

A text from outside the program (a column, a setting, the user) is parsed, and its error is a value; a decimal the
program itself writes is constructed, `math::decimal price("19.99")` ([constructor](decimal.md)), and a wrong one
throws `bad_expected_access<parse_error>` with the same message.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (const char* text : {"-12.50", "1.5e3", ".5", "00012.3400", "-Infinity", "nan"}) {
        math::decimal d = math::decimal::parse(text).value();
        println("{} {} {}", d, d.unscaled(), d.scale());
    }
    for (const char* text : {"", "1.2.3", "1e", "12 ", "1e9999999"}) {
        println(math::decimal::parse(text).error().message());
    }
}
```

Output:

```text
-12.50 -1250 2
1500 15 -2
0.5 5 1
12.3400 123400 4
-Infinity 0 0
NaN 0 0
empty text at byte 0
not a digit in base 10 at byte 3
no digits after the sign at byte 2
not a digit in base 10 at byte 2
an exponent past a million at byte 2
```

## See also

- [(constructor)](decimal.md): a literal of the program
- [to_string](to_string.md), [to_scientific](to_scientific.md): the text written back
- [parse_error](../parse_error/README.md): why a text is not a decimal
- [sgcl::math::decimal](README.md)
