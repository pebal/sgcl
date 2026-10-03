[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::compose

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ compose {};   // called as compose(a, b)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::compose(a, b)`. It takes two `char32_t` and refuses everything else, as the
[module's rules](README.md) say of every question about a code point.

1. Returns the primary composite of the code points `a` and `b`, a `char32_t`: what canonical composition puts the
   two together into, or 0 when they do not compose. The Hangul syllables are included, which compose by
   arithmetic: a leading jamo and a vowel make a syllable, and a syllable without a final consonant takes one. A
   pair the composition excludes composes to nothing.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the first code point, a starter |
| `b` | the code point that follows it |

## Return value

The code point the two compose to, `char32_t`; 0 when they do not.

## Complexity

Constant: arithmetic for Hangul, otherwise a binary search over the table of primary composites.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::compose(U'e', U'\u0301'));
    println("{}", txt::compose(U'\u1100', U'\u1161'));  // a leading jamo and a vowel
    println("{}", uint32_t(txt::compose(U'a', U'b')));
}
```

Output:

```text
é
가
0
```

## See also

- [decompose](decompose.md): one code point taken apart
- [combining_class_of](combining_class_of.md): the order of the marks
- [normalize](normalize.md): composition over a whole text
- [txt](README.md)
