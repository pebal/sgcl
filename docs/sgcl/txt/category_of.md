[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::category_of

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ category_of {};   // called as category_of(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::category_of(c)`, and passed as a projection. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the general category of the code point `c`, a [category](category.md): `category::unassigned` when it has none, which is two thirds of the code space.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The general category of `c`.

## Complexity

Constant: two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U'A', U'ж', U'7', U'!', U' ', char32_t(0x0378)}) {
        println("U+{:04X} {}", uint32_t(c), int(txt::category_of(c)));
    }
    println("{}", txt::category_of(U'€') == txt::category::currency_symbol);
}
```

Output:

```text
U+0041 1
U+0436 2
U+0037 9
U+0021 18
U+0020 23
U+0378 0
true
```

## See also

- [category](category.md)
- [is_alpha](is_alpha.md), [is_punct](is_punct.md): questions over it
- [sgcl::txt](README.md)
