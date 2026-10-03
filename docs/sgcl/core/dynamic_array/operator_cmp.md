[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::operator==, operator\<=\> (sgcl::dynamic_array)

```cpp
/*(1)*/ friend constexpr bool operator==(const dynamic_array& a, const dynamic_array& b);
/*(2)*/ friend constexpr auto operator<=>(const dynamic_array& a, const dynamic_array& b);
```

Compares two arrays element by element. The operators come with [mixin::equatable](../mixin/equatable.md) and
[mixin::comparable](../mixin/comparable.md), hidden friends found by the arguments' type; `!=`, `<`, `<=`, `>`
and `>=` follow from them.

1. `true` when the arrays have the same size and every element of `a` is equal to the element of `b` at the
   same position, by `==`. Takes part only when `T` is [req::equatable](../req/equatable.md).
2. The lexicographical comparison of the elements, with the synthesized three-way comparison: `<=>` of `T` when
   it has one, else a `std::weak_ordering` built from `<`; an array that is a prefix of the other is less. The
   result is that comparison's type. Takes part only when `T` is [req::comparable](../req/comparable.md).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the arrays to compare |

## Return value

- (1) `true` when the arrays hold equal elements, `false` otherwise.
- (2) The ordering of the first pair of elements that are not equivalent, or, when there is none, of the sizes.

## Complexity

- (1) Constant when the sizes differ; otherwise linear in the size, up to the first pair of elements that differ.
- (2) Linear in the size of the smaller array, up to the first pair of elements that differ.

## Exceptions

What the comparison of the elements throws. The operators are not `noexcept` even for elements whose comparison
is, as the comparisons of `std`'s containers: a type whose defaulted `==` compares an array of itself would need
its own exception specification to compute this one's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> a = {1, 2, 3};
    dynamic_array<int> b = {1, 2};
    dynamic_array<int> c = {1, 3};
    println("{} {} {}", a == b, b < a, a < c);
    println("{}", (a <=> dynamic_array<int>{1, 2, 3}) == 0);
}
```

Output:

```text
false true true
true
```

## See also

- [mixin::equatable](../mixin/equatable.md), [mixin::comparable](../mixin/comparable.md)
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
