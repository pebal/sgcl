[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::numeric_value_of

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ numeric_value_of {};   // called as numeric_value_of(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::numeric_value_of(c)`, and passed as a projection. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the value of the decimal digit `c`, an `int`, in any script: `numeric_value_of(U'٣')` is 3, the Arabic-Indic three; -1 when `c` is not a decimal digit (a Roman numeral, a superscript, a letter).
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The value, 0 to 9, or -1.

## Complexity

Constant: none for ASCII, otherwise a binary search over the sorted ranges of the decimal digits, a table of 16-bit bounds below U+10000 and one of wide fields above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string digits = "٣ ١ ٤";
    int sum = 0;
    for (char32_t c : digits.runes()) {
        sum += std::max(0, txt::numeric_value_of(c));
    }
    println("{} sums to {}; {}", digits, sum, txt::numeric_value_of(U'Ⅻ'));
}
```

Output:

```text
٣ ١ ٤ sums to 8; -1
```

## See also

- [is_digit](is_digit.md)
- [sgcl::txt](README.md)
