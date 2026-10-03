[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_printable

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_printable {};   // called as is_printable(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_printable(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is something a terminal can show, Go's `unicode.IsPrint`: a letter, a mark, a number, a punctuation mark or a symbol, and the space U+0020. The other separators (the no-break space too), the controls, the formatting code points and the unassigned are not.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is something a terminal can show, Go's `unicode.IsPrint`: a letter, a mark, a number, a punctuation mark or a symbol, and the space U+0020. The other separators (the no-break space too), the controls, the formatting code points and the unassigned are not.

## Complexity

Constant: none for the space, otherwise two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Notes

The text lines of [slog](../slog/README.md) quote a value that holds a code point this calls not printable.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (char32_t c : {U' ', char32_t(0x00A0), U'\t', char32_t(0x0301), char32_t(0x0378)}) {
        println("U+{:04X} {}", uint32_t(c), txt::is_printable(c));
    }
}
```

Output:

```text
U+0020 true
U+00A0 false
U+0009 false
U+0301 true
U+0378 false
```

## See also

- [is_control](is_control.md)
- [slog](../slog/README.md): what it quotes
- [sgcl::txt](README.md)
