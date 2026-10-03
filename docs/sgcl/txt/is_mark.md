[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_mark

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_mark {};   // called as is_mark(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_mark(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a mark, a category M: a nonspacing mark (Mn, the combining acute), a spacing one (Mc) or an enclosing one (Me).
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a mark, a category M: a nonspacing mark (Mn, the combining acute), a spacing one (Mc) or an enclosing one (Me).

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
    string text = "e\u0301";
    println("{} code points, {} mark", text.rune_count(), text.runes().count_of(txt::is_mark));
}
```

Output:

```text
2 code points, 1 mark
```

## See also

- [combining_class_of](combining_class_of.md)
- [category_of](category_of.md)
- [sgcl::txt](README.md)
