[sgcl](../../README.md) › [core](../README.md) › [unicode](README.md)

# sgcl::unicode::equal_fold

```cpp
static constexpr /* unspecified */ equal_fold {};

template<class... T> requires (std::same_as<T, char32_t> && ...)
constexpr auto operator()(T... c) const noexcept;                      // (1)
template<class... T> requires (!(std::same_as<T, char32_t> && ...))
constexpr auto operator()(T...) const noexcept = delete;               // (2)
```

An object called as a function of two code points, `unicode::equal_fold(a, b)`.

1. Checks whether the two code points are the same letter in either case: equal, or equal by `to_lower`, or equal by
   `to_upper`. So `U'ς'`, the final sigma, and `U'Σ'` are one letter, as are `U'σ'` and `U'Σ'`. The result is a
   `bool`.
2. Every other argument is refused: a `char`, an `int`, a `char8_t`, a `char16_t`, a `wchar_t`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the code points, the arguments of (1) |

## Return value

`true` when the two are the same letter in either case.

## Complexity

Constant: at most four case mappings.

## Exceptions

None.

## Notes

A string's `equal_fold` ([mixin::text](../mixin/text/README.md)) compares two texts code point by code point by this, what
Go's `strings.EqualFold` does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", unicode::equal_fold(U'ς', U'Σ'), unicode::equal_fold(U'ż', U'Ż'));
    println("{}", unicode::equal_fold(U'ż', U'z'));
    println("{}", string("Łódź").equal_fold("ŁÓDŹ"));
}
```

Output:

```text
true true
false
true
```

## See also

- [to_lower](to_lower.md), [to_upper](to_upper.md): the mappings it compares by
- [sgcl::unicode](README.md)
