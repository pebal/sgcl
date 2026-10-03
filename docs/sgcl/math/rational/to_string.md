[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::rational::to_string, sgcl::math::operator\<\< (sgcl::math::rational)

```cpp
string to_string() const;                                         // (1)
std::ostream& operator<<(std::ostream& os, const rational& v);    // (2)
```

1. The fraction in lowest terms as the numerator, a slash and the denominator, in decimal: `"3/4"`, `"-3/2"`. A
   whole number is written without the slash, `"-5"`, `"0"`.
2. Writes `v.to_string()` to `os`.

[parse](parse.md) reads back every text `to_string` writes. [txt::format](../../txt/format.md) writes `{}` of a
fraction as `to_string` does ([format_value](format_value.md)).

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `v` | the fraction written |

## Return value

- (1) The text.
- (2) `os`.

## Complexity

The conversion of the two parts to decimal, as [big_integer::to_string](../big_integer/to_string.md).

## Exceptions

- (1) `length_error` when a part has more digits than a string holds.
- (2) The same, and what the stream throws when its exceptions are on.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <iostream>

using namespace sgcl;

int main() {
    math::rational whole = -5;
    println("{} {} {}", math::rational(3, 4).to_string(), math::rational(9, -6), whole);
    std::cout << math::rational(1, 3) * 2 << '\n';
}
```

Output:

```text
3/4 -3/2 -5
2/3
```

## See also

- [to_decimal](to_decimal.md): the decimal with a number of places
- [parse](parse.md): reads the text back
- [sgcl::math::rational](README.md)
