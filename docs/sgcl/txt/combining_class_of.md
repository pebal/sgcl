[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::combining_class_of

```cpp
#include "sgcl/txt/normalize.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ combining_class_of {};   // called as combining_class_of(c)

    /*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
            constexpr auto operator()(T... c) const noexcept;
    /*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
            constexpr auto operator()(T...) const noexcept = delete;
}
```

An object called as a function, `txt::combining_class_of(c)`, and passed as a predicate or a projection. It takes a
`char32_t` and refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the canonical combining class of the code point `c`, a `uint8_t`: 0 for a starter, and for a mark the
   number that decides the order the marks are put in by the canonical ordering of the normalization (220 below the
   letter, 230 above it, 9 for a virama).
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The canonical combining class of `c`, `uint8_t`.

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
    println("{} {} {} {}", txt::combining_class_of(U'a'), txt::combining_class_of(U'\u0301'),
            txt::combining_class_of(U'\u0323'), txt::combining_class_of(U'\u094D'));
    string s = "e\u0301\u0323";  // e, an acute above and a dot below
    println("{}", s.runes().count_of([](char32_t c) { return txt::combining_class_of(c) != 0; }));
}
```

Output:

```text
0 230 220 9
2
```

## See also

- [compose](compose.md): what two code points compose to
- [decompose](decompose.md): one code point taken apart
- [txt](README.md)
