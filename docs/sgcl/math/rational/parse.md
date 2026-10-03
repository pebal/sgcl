[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::parse

```cpp
static expected<rational, parse_error> parse(const string& text) noexcept;
```

Reads a fraction or a decimal, and nothing else:

- a fraction, `"3/4"`, `"-5"`, `"+0/7"`: an optional sign, digits, and a slash and digits or none; no sign in the
  denominator;
- a decimal, `"-0.125"`, `".5"`, `"7."`, `"1.5e-3"`, `"2E10"`: an optional sign, digits with a point (either side
  of it may be empty, not both), and an exponent, `e` or `E` with an optional sign and digits, of at most a
  million either way.

No space, no `_`, no `inf` or `nan`, no base prefix. The value is the exact one, in lowest terms: `"0.1"` is a
tenth, `"6/4"` is `3/2`, `"1.5e-3"` is `3/2000`.

This is what Go's `big.Rat.SetString` is for; where Go answers `false`, `parse` says why and at which byte.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |

## Return value

The fraction, or a [parse_error](../parse_error.md) whose offset is the byte where the reading stopped:

- `"empty text"`: an empty text;
- `"no digits after the sign"`: a sign alone, a slash with no digits before or after it, a point alone, an `e`
  with no digits after it;
- `"not a digit in base 10"`: any other byte where a digit was wanted — a space, a `_`, a sign in the
  denominator, a slash after a point;
- `"a denominator of zero"`: at the first digit of the denominator;
- `"an exponent past a million"`: at the first byte after the `e`. An exponent is held to a million because a
  dozen bytes of text would otherwise ask for megabytes.

## Complexity

The reading of the digits as a [big_integer](../big_integer.md), then a power of ten for a point or an exponent
(at most a million digits) and the gcd of the reduction.

## Exceptions

None.

## Notes

A text from outside the program (a setting, the user) is parsed, and its error is a value; a fraction the program
itself writes is constructed, `math::rational rate("0.075")` ([constructor](rational.md)), and a wrong one throws
`bad_expected_access<parse_error>` with the same message.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (const char* text : {"3/4", "-5", "6/4", "-0.125", ".5", "1.5e-3", "2E10"}) {
        println("{} -> {}", text, math::rational::parse(text).value());
    }
    for (const char* text : {"", "-", "1/-4", "3/0", "1 /2", "1e9999999"}) {
        auto r = math::rational::parse(text);
        println("\"{}\": {}", text, r.error().message());
    }
}
```

Output:

```text
3/4 -> 3/4
-5 -> -5
6/4 -> 3/2
-0.125 -> -1/8
.5 -> 1/2
1.5e-3 -> 3/2000
2E10 -> 20000000000
"": empty text at byte 0
"-": no digits after the sign at byte 1
"1/-4": not a digit in base 10 at byte 2
"3/0": a denominator of zero at byte 2
"1 /2": not a digit in base 10 at byte 1
"1e9999999": an exponent past a million at byte 2
```

## See also

- [to_string](to_string.md), [to_decimal](to_decimal.md): write the text
- [(constructor)](rational.md): a fraction from a literal text
- [parse_error](../parse_error.md): why a text is not a fraction
- [sgcl::math::rational](../rational.md)
