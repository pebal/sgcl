[sgcl](../../README.md) › [core](../README.md) › [unicode](../unicode.md)

# sgcl::unicode::is_upper

```cpp
static constexpr /* unspecified */ is_upper {};

template<class... T> requires (std::same_as<T, char32_t> && ...)
constexpr auto operator()(T... c) const noexcept;                      // (1)
template<class... T> requires (!(std::same_as<T, char32_t> && ...))
constexpr auto operator()(T...) const noexcept = delete;               // (2)
```

An object called as a function, `unicode::is_upper(c)`, and passed as a predicate:
`s.runes().count_of(unicode::is_upper)`.

1. Checks whether the code point `c` has a lower case other than itself: `to_lower(c) != c`. A letter without case,
   a digit or a sign is neither upper nor lower. The result is a `bool`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` has a lower case other than itself.

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
    string s = "ŁÓDŹ nad Wisłą 2026";
    println("{} upper", s.runes().count_of(unicode::is_upper));
    println("{} {}", unicode::is_upper(U'Ż'), unicode::is_upper(U'2'));
}
```

Output:

```text
5 upper
true false
```

## See also

- [is_lower](is_lower.md): whether a code point has an upper case other than itself
- [to_lower](to_lower.md): the lower case
- [sgcl::unicode](../unicode.md)
