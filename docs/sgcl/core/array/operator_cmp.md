[sgcl](../../README.md) › [core](../README.md) › [array](../array.md)

# sgcl::operator==, operator\<=\> (sgcl::array)

```cpp
/*(1)*/ friend constexpr bool operator==(const array& a, const array& b);
/*(2)*/ friend constexpr auto operator<=>(const array& a, const array& b);
```

Compares two arrays of the same type element by element, as for `std::array`. The operators come with
[mixin::equatable](../mixin/equatable.md) and [mixin::comparable](../mixin/comparable.md), hidden friends found by
the arguments' type; `!=`, `<`, `<=`, `>` and `>=` follow from them.

1. `true` when every element of `a` is equal to the element of `b` at the same position, by `==`. Takes part
   only when `T` is [req::equatable](../req/equatable.md). Two `array<T, 0>` are equal.
2. The lexicographical comparison of the elements, with the synthesized three-way comparison: `<=>` of `T` when
   it has one, else a `std::weak_ordering` built from `<`. The result is that comparison's type, so an array of
   `double` gives a `std::partial_ordering`. Takes part only when `T` is
   [req::comparable](../req/comparable.md).

Both are `constexpr`: two arrays built at compile time are compared at compile time.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the arrays to compare |

## Return value

- (1) `true` when the arrays hold equal elements, `false` otherwise.
- (2) The ordering of the first pair of elements that are not equivalent, or equivalence when there is none.

## Complexity

Linear in `N`, up to the first pair of elements that differ.

## Exceptions

What the comparison of the elements throws. The operators are not `noexcept` even for elements whose comparison
is, as the comparisons of `std`'s containers: a type whose defaulted `==` compares an array of itself would need
its own exception specification to compute this one's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <compare>

using namespace sgcl;

int main() {
    array<int, 3> a = {1, 2, 3};
    array<int, 3> b = {1, 2, 4};
    println("{} {} {} {}", a == b, a != b, a < b, a >= b);

    array<double, 2> e = {1.0, 2.0};
    array<double, 2> f = {1.0, 3.0};
    println("{}", (e <=> f) == std::partial_ordering::less);

    constexpr array<char, 2> x = {'o', 'k'};
    constexpr bool same = x == array<char, 2>{'o', 'k'};
    println("{} {}", same, array<int, 0>() == array<int, 0>());
}
```

Output:

```text
false true true false
true
true true
```

## See also

- [mixin::equatable](../mixin/equatable.md), [mixin::comparable](../mixin/comparable.md)
- [sgcl::array\<T, N\>](../array.md)
