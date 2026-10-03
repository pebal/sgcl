[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::operator==, operator\<=\> (sgcl::vector)

```cpp
friend constexpr bool operator==(const vector& a, const vector& b);     // (1)
friend constexpr auto operator<=>(const vector& a, const vector& b);    // (2)
```

Compares two vectors by their elements.

1. `true` when `a` and `b` have the same size and equal elements in the same order. `!=` follows from it.
2. Compares the elements lexicographically, as `std::lexicographical_compare_three_way`: the first pair of
   elements that differ decides, and a vector that is a prefix of the other is less. The elements are compared
   by their `<=>`, or, without one, by their `<` (a weak ordering, as `std` builds it). `<`, `<=`, `>` and `>=`
   follow from it.

The operators are friends of [mixin::equatable](../mixin/equatable/README.md) (1) and
[mixin::comparable](../mixin/comparable/README.md) (2), found through the vector's type. (1) takes part only when `T`
is [req::equatable](../req/equatable.md), (2) only when `T` is [req::comparable](../req/comparable.md): a vector
of elements without `==` has no `==` either.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the vectors to compare |

## Return value

1. `true` when the vectors are equal, `false` otherwise.
2. The order of the first elements that differ, or of the sizes when one vector is a prefix of the other: the
   comparison category of `T`'s `<=>`, `std::weak_ordering` when `T` has only `<`.

## Complexity

Linear in the size of the shorter vector, at most one comparison of elements per position; (1) is constant when
the sizes differ.

## Exceptions

What the comparison of the elements throws.

## Notes

Neither operator is `noexcept`, even for elements whose `==` and `<=>` are, as the comparisons of `std`'s
containers: a type whose defaulted `==` compares a vector of itself (a tree of nodes) would need its own
exception specification to compute the vector's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;  // no ==
};

int main() {
    vector a = {1, 2, 3};
    vector b = {1, 2, 4};
    vector c = {1, 2};
    println("{} {} {} {}", a == a, a != b, a < b, c < a);
    println("{}", req::equatable<vector<int>>);
    println("{}", req::equatable<vector<Point>>);
}
```

Output:

```text
true true true true
true
false
```

## See also

- [mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md): the operators of every
  sequence of the library
- [sgcl::vector\<T\>](README.md)
