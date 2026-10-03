[sgcl](../../README.md) › [core](../README.md) › [unicode](../unicode.md)

# sgcl::unicode::is_space

```cpp
static constexpr /* unspecified */ is_space {};

/*(1)*/ template<class... T> requires (std::same_as<T, char32_t> && ...)
        constexpr auto operator()(T... c) const noexcept;
/*(2)*/ template<class... T> requires (!(std::same_as<T, char32_t> && ...))
        constexpr auto operator()(T...) const noexcept = delete;
```

An object called as a function, `unicode::is_space(c)`, and passed as a predicate.

1. Checks whether the code point `c` has the White_Space property: the six of the C locale (space, `\t`, `\n`, `\v`,
   `\f`, `\r`), U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F and U+3000. U+200B, the
   zero-width space, is a format character and not white space. The result is a `bool`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`. A `char` is a byte of
   UTF-8: `is_space(s[0])` on a no-break space would ask about the byte `0xC2` and answer no.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` is white space.

## Complexity

Constant: a test for ASCII, otherwise a few comparisons.

## Exceptions

None.

## Notes

What a string's [trim](../string/trim.md), [trim_left](../string/trim_left.md),
[trim_right](../string/trim_right.md) and [fields](../string/fields.md) skip is this.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", unicode::is_space(U' '), unicode::is_space(U'\u00A0'),
            unicode::is_space(U'\u3000'));
    println("{}", unicode::is_space(U'\u200B'));

    string s = "\u00A0two\u3000words ";
    println("{} spaces", s.runes().count_of(unicode::is_space));
}
```

Output:

```text
true true true
false
3 spaces
```

## See also

- [trim](../string/trim.md), [fields](../string/fields.md): what a string does with it
- [sgcl::unicode](../unicode.md)
