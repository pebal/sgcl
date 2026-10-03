[sgcl](../../README.md) › [core](../README.md) › [unicode](README.md)

# sgcl::unicode::to_upper

```cpp
static constexpr /* unspecified */ to_upper {};

template<class... T> requires (std::same_as<T, char32_t> && ...)
constexpr auto operator()(T... c) const noexcept;                      // (1)
template<class... T> requires (!(std::same_as<T, char32_t> && ...))
constexpr auto operator()(T...) const noexcept = delete;               // (2)
```

An object called as a function, `unicode::to_upper(c)`.

1. Returns the upper case of the code point `c` by Unicode's simple mapping, one code point to one: `U'ż'` to `U'Ż'`;
   `c` itself when it has none. `U'ß'` has no upper case of one code point and stays `U'ß'`. The result is a
   `char32_t`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`. A `char` is a byte of
   UTF-8, not a code point, and `'ż'` is an `int`: a program passes `s.decode(i).first`, a rune of `s.runes()`, or
   `U'ż'`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

The upper case of `c`, a `char32_t`, or `c` when it has none.

## Complexity

Constant: two reads of a table in the Basic Multilingual Plane, a binary search over 11 ranges above it.

## Exceptions

None.

## Notes

A string's [to_upper](../string/to_upper.md) maps each of its code points by this. The full mapping, where `ß`
becomes `SS`, and the rules of a language are [txt::to_upper_full](../../txt/to_upper_full.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", unicode::to_upper(U'ż') == U'Ż');
    println("{}", unicode::to_upper(U'ß') == U'ß');
    println("{}", unicode::to_upper(U'7') == U'7');
}
```

Output:

```text
true
true
true
```

## See also

- [to_lower](to_lower.md): the lower case
- [is_lower](is_lower.md): whether a code point has an upper case other than itself
- [sgcl::unicode](README.md)
