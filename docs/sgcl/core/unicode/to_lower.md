[sgcl](../../README.md) › [core](../README.md) › [unicode](../unicode.md)

# sgcl::unicode::to_lower

```cpp
static constexpr /* unspecified */ to_lower {};

template<class... T> requires (std::same_as<T, char32_t> && ...)
constexpr auto operator()(T... c) const noexcept;                      // (1)
template<class... T> requires (!(std::same_as<T, char32_t> && ...))
constexpr auto operator()(T...) const noexcept = delete;               // (2)
```

An object called as a function, `unicode::to_lower(c)`.

1. Returns the lower case of the code point `c` by Unicode's simple mapping, one code point to one: `U'Ł'` to
   `U'ł'`, `U'İ'` to `U'i'`; `c` itself when it has none. The result is a `char32_t`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`. A `char` is a byte of
   UTF-8, not a code point, and `'ż'` is an `int`: a program passes `s.decode(i).first`, a rune of `s.runes()`, or
   `U'ż'`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The lower case of `c`, a `char32_t`, or `c` when it has none.

## Complexity

Constant: two reads of a table in the Basic Multilingual Plane, a binary search over 11 ranges above it.

## Exceptions

None.

## Notes

A string's [to_lower](../string/to_lower.md) maps each of its code points by this. The full mapping, which may change
the number of code points, and the rules of a language are [txt::to_lower_full](../../txt/to_lower_full.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", unicode::to_lower(U'Ł') == U'ł');
    println("{}", unicode::to_lower(U'İ') == U'i');
    println("{}", unicode::to_lower(U'ł') == U'ł');

    string s = "Żółw";
    char out[utf8::max_width];
    size_t n = utf8::encode(unicode::to_lower(s.decode(0).first), out);
    println("{}", string(out, n));
}
```

Output:

```text
true
true
true
ż
```

## See also

- [to_upper](to_upper.md): the upper case
- [is_upper](is_upper.md): whether a code point has a lower case other than itself
- [sgcl::unicode](../unicode.md)
