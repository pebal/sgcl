[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::parse

```cpp
static expected<big_integer, parse_error> parse(const string& text, int base = 10);
```

Reads the number `text` writes in `base`: an optional `+` or `-`, then the digits of the base, letters in either
case, and nothing else — no prefix, no space, no separator: `parse("ff", 16)`, never a base guessed from `0x`.
Leading zeros are allowed, and `-0` is zero.

A text from outside the program (a file, a request, the user) is parsed, and its error is a value; a text the
program itself writes is constructed, `math::big_integer mask("ffff0000", 16)` ([constructor](big_integer.md)),
and a wrong one throws.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text to read |
| `base` | the base of the digits, 2 to 36 |

## Return value

The number, or a [parse_error](../parse_error.md) whose `offset()` is the byte where the reading stopped and whose
`message()` says why, ending with `at byte` and the offset:

- `"empty text"`: an empty text, at byte 0;
- `"no digits after the sign"`: a sign alone, at the end of the text;
- `"not a digit in base N"`: at the first byte that is not a digit of the base; for a character past ASCII, the
  byte its encoding starts at.

## Complexity

Linear in the length of `text` in a base that is a power of two, which is a matter of placing bits. In any other,
decimal above all, the digits are read a limb's worth at a time (19 digits in decimal), which is quadratic, and
past a threshold by divide and conquer over the powers `10^(19·2^i)` (in decimal; in another base the largest
power of it a limb holds, squared again and again): a million digits take the time of a few long multiplications,
`O(M(n) log n)`, and never the square of the length, so that `parse` is safe on text from outside.

## Exceptions

`invalid_argument` when `base` is outside 2 to 36. A text that is not a number is not an exception: it is the
error of the result.

## Notes

Go's `SetString` reads bases up to 62, with a base of 0 guesses the base from a prefix, and answers a text that is
not a number with `nil, false`. Here the base is the caller's, 2 to 36, a prefix is written only in the literal
[_big](literals.md), and the error says where and why.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (const char* text : {"123456789012345678901234567890", "-0", "+007"}) {
        println("{}", math::big_integer::parse(text).value());
    }
    println("{}", math::big_integer::parse("FFff", 16).value());

    for (const char* text : {"", "-", "12x4", "0x10", "1 000"}) {
        auto n = math::big_integer::parse(text);
        println("\"{}\": {}", text, n.error().message());
    }
}
```

Output:

```text
123456789012345678901234567890
0
7
65535
"": empty text at byte 0
"-": no digits after the sign at byte 1
"12x4": not a digit in base 10 at byte 2
"0x10": not a digit in base 10 at byte 1
"1 000": not a digit in base 10 at byte 1
```

## See also

- [(constructor)](big_integer.md): a number from a text the program writes
- [to_string](to_string.md): writes the text
- [parse_error](../parse_error.md): why a text is not a number
- [sgcl::math::big_integer](../big_integer.md)
