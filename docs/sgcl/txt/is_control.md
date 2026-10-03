[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_control

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_control {};   // called as is_control(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::is_control(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a control, the category Cc: C0 (U+0000 to U+001F), DEL and C1 (U+0080 to U+009F). A formatting code point, such as the zero-width joiner, is not one ([is_format](is_format.md)).
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a control, the category Cc: C0 (U+0000 to U+001F), DEL and C1 (U+0080 to U+009F). A formatting code point, such as the zero-width joiner, is not one ([is_format](is_format.md)).

## Complexity

Constant: two comparisons, no table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::is_control(U'\n'), txt::is_control(char32_t(0x85)),
            txt::is_control(char32_t(0x200D)));
}
```

Output:

```text
true true false
```

## See also

- [is_format](is_format.md), [is_printable](is_printable.md)
- [sgcl::txt](README.md)
