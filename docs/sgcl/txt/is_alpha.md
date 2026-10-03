[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_alpha

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_alpha {};   // called as is_alpha(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_alpha(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is a letter: the categories Lu, Ll, Lt, Lm and Lo, Go's `unicode.IsLetter`. Not the Alphabetic property, which also holds `letter_number` and the marks that spell a vowel, and would make `is_alpha` true of a combining sign.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is a letter: the categories Lu, Ll, Lt, Lm and Lo, Go's `unicode.IsLetter`. Not the Alphabetic property, which also holds `letter_number` and the marks that spell a vowel, and would make `is_alpha` true of a combining sign.

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
    println("{} {} {}", txt::is_alpha(U'ż'), txt::is_alpha(U'Ⅻ'), txt::is_alpha(char32_t(0x0301)));
    string word = "naïve 2";
    println("{} letters", word.runes().count_of(txt::is_alpha));
}
```

Output:

```text
true false false
5 letters
```

## See also

- [is_alnum](is_alnum.md), [is_digit](is_digit.md)
- [category_of](category_of.md)
- [sgcl::txt](README.md)
