[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_format

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_format {};   // called as is_format(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_format(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a formatting code point, the category Cf: the zero-width joiner and non-joiner, the marks of direction, the soft hyphen, the byte order mark.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a formatting code point, the category Cf: the zero-width joiner and non-joiner, the marks of direction, the soft hyphen, the byte order mark.

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
    for (char32_t c : {char32_t(0x00AD), char32_t(0x200D), char32_t(0x200F), U'a'}) {
        println("U+{:04X} {}", uint32_t(c), txt::is_format(c));
    }
}
```

Output:

```text
U+00AD true
U+200D true
U+200F true
U+0061 false
```

## See also

- [is_control](is_control.md)
- [sgcl::txt](README.md)
