[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_mirrored

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_mirrored {};   // called as is_mirrored(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::is_mirrored(c)`, and passed as a predicate:
`s.runes().count_of(txt::is_mirrored)`. It takes a `char32_t` and refuses everything else, as the
[module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is drawn the other way round in a right to left run: a parenthesis, a bracket,
   a chevron, a less-than sign, an integral. It is the `Bidi_Mirrored` property, which rule L4 of
   [UAX #9](https://www.unicode.org/reports/tr9/) asks about, and it holds of more code points than have a mirror
   of their own: **554** code points are `Bidi_Mirrored` and only **428** have a mirror
   ([mirrored_of](mirrored_of.md)); an integral sign is drawn the other way round without there being a second one
   to name it. The result is a `bool`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` is `Bidi_Mirrored`.

## Complexity

Constant: a mask for ASCII, otherwise a binary search over the 114 ranges of the property.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::is_mirrored(U'('), txt::is_mirrored(U'∫'), txt::is_mirrored(U'a'));
    string s = "f(x) = [a, b] ≤ c";
    println("{}", s.runes().count_of(txt::is_mirrored));
}
```

Output:

```text
true true false
5
```

## See also

- [mirrored_of](mirrored_of.md): the code point of the mirrored shape
- [mirrored](mirrored.md): rule L4 over a text
- [txt](README.md)
