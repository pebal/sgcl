[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_string, sgcl::math::operator\<\< (sgcl::math::decimal)

```cpp
string to_string() const;                                        // (1)
std::ostream& operator<<(std::ostream& os, const decimal& v);    // (2)
```

1. The value in plain form, never with an exponent: every digit of the unscaled part, a point before the last
   `scale()` of them (with zeros in front when there are fewer), zeros after them when the scale is negative —
   `"1.50"`, `"-0.0012"`, `"1500"` for `1.5e3`. PostgreSQL's form of a `NUMERIC`, and Python's `format(d, 'f')`.
   `"NaN"`, `"Infinity"` and `"-Infinity"` for the three that are not numbers. [to_scientific](to_scientific.md)
   writes the exponent.
2. Writes `to_string()` to the stream.

## Parameters

| Parameter | Description |
|---|---|
| `os` | the stream written to |
| `v` | the decimal written |

## Return value

1. The text.
2. `os`.

## Complexity

The conversion of the unscaled part to decimal, and the length of the text.

## Exceptions

- `length_error` when the text has more characters than a string holds (a scale of −4·10⁹ asks for that many zeros).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

#include <iostream>

using namespace sgcl;

int main() {
    for (const char* text : {"1.50", "-0.0012", "1.5e3", "0.000", "-Infinity"}) {
        println(math::decimal(text).to_string());
    }
    std::cout << math::decimal("12.5e-3") << '\n';
}
```

Output:

```text
1.50
-0.0012
1500
0.000
-Infinity
0.0125
```

## See also

- [to_scientific](to_scientific.md): the text with an exponent
- [format_value](format_value.md): `{}` and `{:.2f}` in a field
- [parse](parse.md): the text read back
- [sgcl::math::decimal](README.md)
