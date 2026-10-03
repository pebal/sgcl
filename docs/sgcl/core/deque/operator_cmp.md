[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::operator==, operator\<=\> (sgcl::deque)

```cpp
friend constexpr bool operator==(const deque& a, const deque& b);     // (1)
friend constexpr auto operator<=>(const deque& a, const deque& b);    // (2)
```

Compares two deques by their elements, as `std::deque` compares them.

1. `true` when `a` and `b` have the same size and equal elements in the same order. The sizes are compared first.
   Takes part only when `T` is [req::equatable](../req/equatable.md).
2. Compares the elements lexicographically with the synthesized three-way comparison: `<=>` of `T` when it has
   one, else a `std::weak_ordering` built from `<`. Takes part only when `T` is
   [req::comparable](../req/comparable.md).

`!=`, `<`, `<=`, `>` and `>=` follow from them. The operators come from [mixin::equatable](../mixin/equatable/README.md)
and [mixin::comparable](../mixin/comparable/README.md), found through the deque's type.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the deques to compare |

## Return value

- (1) `true` when the deques are equal, `false` otherwise.
- (2) The ordering of the first pair of elements that differ, or of the sizes when one deque is a prefix of the
  other.

## Complexity

- (1) Constant when the sizes differ; otherwise linear in the size.
- (2) Linear in the size of the shorter deque.

## Exceptions

What the comparison of the elements throws.

## Notes

The operators are not `noexcept` even for elements whose comparison is, as with the containers of `std`: a type
whose defaulted `==` compares a deque of itself would otherwise need its own exception specification to compute
this one's.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;  // no ==
};

int main() {
    deque a = {1, 2};
    deque b = {1, 3};
    deque c = {1, 2, 0};
    println("{} {} {} {}", a == b, a != b, a < b, a < c);
    println("{}", (a <=> b) < 0);
    println("{} {}", req::equatable<deque<int>>, req::equatable<deque<Point>>);
}
```

Output:

```text
false true true true
true
true false
```

## See also

- [mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md): the operators of every
  container of the library
- [sgcl::deque\<T\>](README.md)
