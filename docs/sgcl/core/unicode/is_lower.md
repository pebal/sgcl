[sgcl](../../README.md) › [core](../README.md) › [unicode](../unicode.md)

# sgcl::unicode::is_lower

```cpp
static constexpr /* unspecified */ is_lower {};

template<class... T> requires (std::same_as<T, char32_t> && ...)
constexpr auto operator()(T... c) const noexcept;                      // (1)
template<class... T> requires (!(std::same_as<T, char32_t> && ...))
constexpr auto operator()(T...) const noexcept = delete;               // (2)
```

An object called as a function, `unicode::is_lower(c)`, and passed as a predicate:
`s.runes().count_of(unicode::is_lower)`.

1. Checks whether the code point `c` has an upper case other than itself: `to_upper(c) != c`. A letter without case,
   a digit or a sign is neither lower nor upper, and so is `ß`, whose upper case is not one code point. The result is
   a `bool`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` has an upper case other than itself.

## Complexity

Constant: two reads of a table in the Basic Multilingual Plane, a binary search over 11 ranges above it.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", unicode::is_lower(U'ł'), unicode::is_lower(U'Ł'), unicode::is_lower(U'ß'));
    string s = "Grüße aus Łódź";
    println("{} lower", s.runes().count_of(unicode::is_lower));
}
```

Output:

```text
true false false
9 lower
```

## See also

- [is_upper](is_upper.md): whether a code point has a lower case other than itself
- [to_upper](to_upper.md): the upper case
- [sgcl::unicode](../unicode.md)
