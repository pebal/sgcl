[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_alnum

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_alnum {};   // called as is_alnum(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_alnum(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a letter or a decimal digit: [is_alpha](is_alpha.md) or [is_digit](is_digit.md).
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a letter or a decimal digit: [is_alpha](is_alpha.md) or [is_digit](is_digit.md).

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
    string id = "user_42";
    println("{} of {}", id.runes().count_of(txt::is_alnum), id.rune_count());
}
```

Output:

```text
6 of 7
```

## See also

- [is_alpha](is_alpha.md), [is_digit](is_digit.md)
- [sgcl::txt](README.md)
