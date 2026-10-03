[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_digit

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_digit {};   // called as is_digit(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::is_digit(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a decimal digit, the category Nd, in any script: `7`, the Arabic-Indic `٣`, the Devanagari `७`. Not a superscript or a fraction, which are `other_number`.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a decimal digit, the category Nd, in any script: `7`, the Arabic-Indic `٣`, the Devanagari `७`. Not a superscript or a fraction, which are `other_number`.

## Complexity

Constant: none for ASCII, otherwise two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::is_digit(U'7'), txt::is_digit(U'٣'), txt::is_digit(U'²'));
}
```

Output:

```text
true true false
```

## See also

- [numeric_value_of](numeric_value_of.md): its value
- [is_alnum](is_alnum.md)
- [sgcl::txt](README.md)
