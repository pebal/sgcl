[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::operator==, operator\<=\> (sgcl::forward_list)

```cpp
friend constexpr bool operator==(const forward_list& a, const forward_list& b);     // (1)
friend constexpr auto operator<=>(const forward_list& a, const forward_list& b);    // (2)
```

Compare two lists by their elements, as `std::forward_list`'s operators do, in one walk of both lists. Both are
hidden friends of [mixin::equatable](../mixin/equatable/README.md) and [mixin::comparable](../mixin/comparable/README.md), found
through the list's type.

1. `true` when `a` and `b` have equal elements in the same order. The walk stops at the first difference or the end
   of either list. Takes part only when `T` is [req::equatable](../req/equatable.md).
2. Compares the elements lexicographically, by the synthesized three-way comparison: `<=>` of `T` when it has one,
   else a `std::weak_ordering` built from `<`. Takes part only when `T` is [req::comparable](../req/comparable.md).

`!=` follows from (1), and `<`, `<=`, `>` and `>=` from (2).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the lists to compare |

## Return value

- (1) `true` when the lists are equal, `false` otherwise.
- (2) The order of the first pair of elements that differ, or of the lengths when one list is a prefix of the
  other: `std::strong_ordering` for a `forward_list<int>`, `std::partial_ordering` for a
  `forward_list<double>`.

## Complexity

Linear in the length of the shorter list, at most one comparison of elements per position.

## Exceptions

What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;  // no ==, no <
};

int main() {
    forward_list a = {1, 2};
    forward_list b = {1, 3};
    forward_list c = {1, 2, 0};
    println("{} {} {} {}", a == b, a < b, a < c, a != c);
    println("{}", std::is_same_v<decltype(a <=> b), std::strong_ordering>);
    println("{} {}", req::equatable<forward_list<int>>, req::equatable<forward_list<Point>>);
}
```

Output:

```text
false true true true
true
true false
```

## See also

- [mixin::equatable](../mixin/equatable/README.md), [mixin::comparable](../mixin/comparable/README.md)
- [sgcl::forward_list\<T\>](README.md)
