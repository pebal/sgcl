[sgcl](../../README.md) › [txt](../README.md) › [number_format](README.md)

# sgcl::txt::number_format::format

```cpp
template<class T>
string format(T value) const;                            // (1)
optional<string> format(const string& decimal) const;    // (2)
```

Returns a number written in the format's way.

1. A value of an arithmetic type: an integer exactly, a floating-point number from its shortest decimal digits (2.675
   is 2.675 and rounds to 2.68 at two digits, as ICU rounds it, where its binary value would give 2.67). A NaN is the
   locale's NaN, an infinity its ∞ with the sign.
2. A number written as decimal text, as a [math::decimal](../../math/README.md) or a database gives it: an optional
   sign, digits with an optional point, an optional exponent after `e` or `E`. Exact to 200 significant digits,
   rounded there half to even; a number whose point stands more than 100000 digits from its first digit is refused.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number |
| `decimal` | the number as text: `"-1234.5678"`, `"1e30"` |

## Return value

The text; (2) `nullopt` when `decimal` is not a decimal number or its point stands more than 100000 digits from its
first digit.

## Complexity

Linear in the digits written.

## Exceptions

None that the program can cause; running out of memory ends it.

## Notes

(1) takes part only when `T` is an arithmetic type other than `bool`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::number_format en(txt::locale("en"), {.max_fraction = 2});
    println("{} {}", en.format(2.675), en.format(1234));
    println("{}", *en.format("123456789012345678901234567890.125"));
    println("{}", en.format("12 apples").has_value());
}
```

Output:

```text
2.68 1,234
123,456,789,012,345,678,901,234,567,890.12
false
```

## See also

- [format_to](format_to.md)
- [format_number](../format_number.md)
- [sgcl::txt::number_format](README.md)
