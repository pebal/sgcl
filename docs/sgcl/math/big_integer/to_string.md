[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::to_string, sgcl::math::operator\<\< (sgcl::math::big_integer)

```cpp
/*(1)*/ string to_string(int base = 10) const;
/*(2)*/ std::ostream& operator<<(std::ostream& os, const big_integer& v);
```

1. The digits of the number in `base`, 2 to 36, the letters small, a minus in front of a negative number and
   nothing else: no prefix, no plus. Zero is `"0"`. Go's `Text(base)` takes bases up to 62; here the bases are the
   ones [parse](parse.md) reads, which reads back every text `to_string` writes.
2. Writes `v` as the stream writes an `int64_t`: in decimal, or in the base its basefield asks for (`std::hex`,
   `std::oct`). `showbase` puts `0x` (`0X` with `uppercase`) or a leading `0` in front of a value that is not zero;
   `uppercase` writes the digits of hexadecimal in capitals; `showpos` puts a plus before a positive number in
   decimal. The width is padded with the fill on the side `adjustfield` says, `std::internal` padding between the
   sign and `0x` and the digits (the `0` of octal counts as a digit, as the stream counts it); the width is used up
   by the one value, as for any number. One thing is not as for an `int64_t`: a negative number in hexadecimal or
   octal is its magnitude after a minus (`-ff`), a number of no fixed width having no two's complement to print.

## Parameters

| Parameter | Description |
|---|---|
| `base` | the base of the digits, 2 to 36 |
| `os` | the stream written to |
| `v` | the number written |

## Return value

- (1) The text.
- (2) `os`.

## Complexity

- (1–2) Linear in the length of the number for a base that is a power of two. In another base, decimal above all,
  a number of fewer than 32 limbs is written a chunk of digits at a time (19 in decimal, the largest power of the
  base a limb holds), each the remainder of a division by one word, which is quadratic in the length; a longer one
  by divide and conquer over the powers `10^(19·2^i)` (of the base's chunk, for another base), so that a million
  digits take the time of a few long multiplications and never the square of the length. The threshold is set by
  measurement.

## Exceptions

- (1) `invalid_argument` when `base` is outside 2 to 36; `length_error` when the number has more digits than a
  string holds (4 G characters: a number of 512 MB of limbs, in binary).
- (2) What the stream throws when its exceptions are on.

## Notes

[txt::format](../../txt/format.md) writes a number with the specifications of an `int`, `{:#x}` and `{:>40}`
among them: [format_value](format_value.md). There a negative number in hexadecimal is `-ff` as well, as
`std::format` writes an `int`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <iomanip>
#include <iostream>

using namespace sgcl;

int main() {
    math::big_integer a = 255;
    println("{} {} {} {}", a.to_string(), a.to_string(16), (-a).to_string(2), a.to_string(36));
    math::big_integer text("hello", 36);
    println("{} {}", text, text.to_string(36));

    std::cout << std::hex << -a << ' ' << std::showbase << a << ' ' << std::uppercase << a << '\n';
    std::cout << std::dec << std::setw(8) << std::setfill('0') << std::internal << -a << '\n';
    std::cout << std::oct << a << '\n';
}
```

Output:

```text
255 ff -11111111 73
29234652 hello
-ff 0xff 0XFF
-0000255
0377
```

## See also

- [parse](parse.md): reads the text back
- [format_value](format_value.md): what [txt::format](../../txt/format.md) writes
- [to_bytes](to_bytes.md): the magnitude as bytes
- [benchmarks](../benchmarks.md#big_integer): the time against Go's `math/big`
- [sgcl::math::big_integer](../big_integer.md)
