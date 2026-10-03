[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_emoji

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ is_emoji {};   // called as is_emoji(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::is_emoji(c)`, and passed as a predicate. It takes a `char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Checks whether the code point `c` is an emoji, the Extended_Pictographic property: `😀`, `©`. Not the Emoji property, whose members are also `#`, `*` and the ten digits; Extended_Pictographic is the property the segmentation needs, so the two agree on what one emoji is.
2. Every other argument is refused: a `char` (a byte of UTF-8), an `int` (a multi-character literal), a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when the code point `c` is an emoji, the Extended_Pictographic property: `😀`, `©`. Not the Emoji property, whose members are also `#`, `*` and the ten digits; Extended_Pictographic is the property the segmentation needs, so the two agree on what one emoji is.

## Complexity

Constant: none below U+00A9, otherwise two reads of a table below U+10000, a binary search over sorted ranges above.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::is_emoji(U'😀'), txt::is_emoji(U'©'), txt::is_emoji(U'#'));
}
```

Output:

```text
true true false
```

## See also

- [graphemes](graphemes/README.md): an emoji and its modifiers as one character
- [sgcl::txt](README.md)
