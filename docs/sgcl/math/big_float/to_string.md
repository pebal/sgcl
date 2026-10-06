[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_string, sgcl::math::operator\<\< (sgcl::math::big_float)

```cpp
string to_string() const;                                          // (1)
std::ostream& operator<<(std::ostream& os, const big_float& v);    // (2)
```

1. The shortest decimal that reads back to this value at its precision: `0.1` for the double 0.1, `1e+100`,
   `0.3333333333333333333333333333335` for a third at 100 bits; plain but for an exponent below −4 or of 21 and more,
   which is written in scientific form (Go's `String` changes at 6 and writes a million `1e+06`). The digits are chosen
   from the interval of the values that round to this one — half a unit of the last place either side, a quarter below
   a power of two, where the next value down is nearer (Go's interval has a half there too, and its digits for 2^k
   may read back as the value below; these never do). `+Inf`, `-Inf`, `0`, `-0`.
2. Writes `to_string()` to the stream.

A number of digits is asked through [txt::format](../../txt/format.md): `{:.30f}`, `{:.10e}` ([format_value](format_value.md)).

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `v` | the value written |

## Return value

- (1) The text.
- (2) `os`.

## Complexity

The exact decimal ends of the value's interval (a power of five of its exponent) and a few divisions.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <iostream>

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_float(0.1), math::big_float(1234567), math::big_float(0.00001));
    math::big_float third(math::rational(1, 3), 100);
    println(third.to_string());
    println(math::big_float::parse("1e100").value());
    std::cout << -math::big_float::infinity() << '\n';
}
```

Output:

```text
0.1 1234567 1e-05
0.3333333333333333333333333333335
1e+100
-Inf
```

## See also

- [to_scientific](to_scientific.md): always with an exponent
- [to_hex](to_hex.md): the exact value in hexadecimal
- [format_value](format_value.md): `{:.30f}` and `{:e}` with a number of digits
- [parse](parse.md): the text read back
- [sgcl::math::big_float](README.md)
