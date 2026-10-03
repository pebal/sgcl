[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::operator==, operator\<=\> (sgcl::list)

```cpp
friend constexpr bool operator==(const list& a, const list& b);     // (1)
friend constexpr auto operator<=>(const list& a, const list& b);    // (2)
```

Compare two lists by their elements, as `std::list`'s operators do. Both are hidden friends of
[mixin::equatable](../mixin/equatable.md) and [mixin::comparable](../mixin/comparable.md), found through the
list's type.

1. `true` when `a` and `b` have equal elements in the same order. The two lists are walked together, up to the
   first difference or the end of either. Takes part only when `T` is [req::equatable](../req/equatable.md).
2. Compares the elements lexicographically, by the synthesized three-way comparison: `<=>` of `T` when it has one,
   else a `std::weak_ordering` built from `<`. Takes part only when `T` is [req::comparable](../req/comparable.md).

`!=` follows from (1), and `<`, `<=`, `>` and `>=` from (2).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the lists to compare |

## Return value

- (1) `true` when the lists are equal, `false` otherwise.
- (2) The order of the first pair of elements that differ, or of the sizes when one list is a prefix of the
  other: `std::strong_ordering` for a `list<int>`, `std::partial_ordering` for a `list<double>`.

## Complexity

Linear in the size of the shorter list, at most one comparison of elements per position; (1) compares none
when the sizes differ.

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
    list a = {1, 2};
    list b = {1, 3};
    list c = {1, 2, 0};
    println("{} {} {} {}", a == b, a < b, a < c, a != c);
    println("{}", std::is_same_v<decltype(a <=> b), std::strong_ordering>);
    println("{} {}", req::equatable<list<int>>, req::equatable<list<Point>>);
}
```

Output:

```text
false true true true
true
true false
```

## See also

- [mixin::equatable](../mixin/equatable.md), [mixin::comparable](../mixin/comparable.md)
- [sgcl::list\<T\>](../list.md)
