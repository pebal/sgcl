[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_scientific

```cpp
string to_scientific() const;
```

The value in scientific form with every digit of the unscaled part: the first digit, a point and the others when
there are any, `e`, the sign and the exponent — `"1.5e+3"`, `"-1.2e-3"`, `"1.50e+0"`, `"5e+0"`, `"0e-2"` for
`0.00`. Python's `format(d, 'e')`. [parse](parse.md) reads it back to the same value at the same scale, which the
plain [to_string](to_string.md) of a negative scale does not keep. `"NaN"`, `"Infinity"` and `"-Infinity"` for
the three that are not numbers.

## Parameters

None.

## Return value

The text.

## Complexity

The conversion of the unscaled part to decimal.

## Exceptions

- `length_error` when the text has more characters than a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    for (const char* text : {"1500", "1.5e3", "-0.0012", "1.50", "0.00"}) {
        println(math::decimal(text).to_scientific());
    }
    math::decimal d("1.5e3");
    println(math::decimal(d.to_scientific()).identical(d));
}
```

Output:

```text
1.500e+3
1.5e+3
-1.2e-3
1.50e+0
0e-2
true
```

## See also

- [to_string](to_string.md): the plain text
- [format_value](format_value.md): `{:e}` and `{:.3e}`
- [sgcl::math::decimal](README.md)
