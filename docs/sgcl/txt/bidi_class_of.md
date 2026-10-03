[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::bidi_class_of

```cpp
#include "sgcl/txt/bidi.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr /* unspecified */ bidi_class_of {};   // called as bidi_class_of(c)

    template<class... T> requires (std::same_as<T, char32_t> && ...)
    constexpr auto operator()(T... c) const noexcept;                      // (1)
    template<class... T> requires (!(std::same_as<T, char32_t> && ...))
    constexpr auto operator()(T...) const noexcept = delete;               // (2)
}
```

An object called as a function, `txt::bidi_class_of(c)`, and passed as a projection. It takes a `char32_t` and
refuses everything else, as the [module's rules](README.md) say of every question about a code point.

1. Returns the bidirectional class of the code point `c`, a [txt::bidi](bidi.md): what the algorithm of
   [UAX #9](https://www.unicode.org/reports/tr9/) knows about it before it looks at anything around it. An
   unassigned code point has the class `DerivedBidiClass.txt` gives its range.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The class of `c`, [txt::bidi](bidi.md).

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
    println("{} {} {}", txt::bidi_class_of(U'a') == txt::bidi::l,
            txt::bidi_class_of(U'א') == txt::bidi::r, txt::bidi_class_of(U'ب') == txt::bidi::al);
    println("{} {}", txt::bidi_class_of(U'٣') == txt::bidi::an,
            txt::bidi_class_of(U'\u202E') == txt::bidi::rlo);
}
```

Output:

```text
true true true
true true
```

## See also

- [bidi](bidi.md): the classes
- [paragraph_direction](paragraph_direction.md): what the classes decide for a paragraph
- [txt](README.md)
