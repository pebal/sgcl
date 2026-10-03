[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](../enumerable.md)

# sgcl::mixin::enumerable\<Derived\>::min

```cpp
constexpr decltype(auto) min() const noexcept(/* see below */);               // (1)
template<class Compare>
constexpr decltype(auto) min(Compare cmp) const noexcept(/* see below */);    // (2)
```

Finds the smallest element, walking the whole range; of several equal smallest, the first.

1. Compares the elements with `<`. Takes part only when the elements are `req::comparable`.
2. Compares them with `cmp`, `cmp(a, b)` true when `a` goes before `b`. Takes part only when `cmp` is a strict
   weak order of the elements; it asks nothing of the element, which needs no `<`.

- (1–2) The range must not be empty: on an empty range the behaviour is undefined, as with `front()`, and nothing
  is checked.

## Parameters

| Parameter | Description |
|---|---|
| `cmp` | the order, called with two const elements and returning what converts to `bool` |

## Return value

A const reference to the smallest element, or the value itself where the iterator gives values rather than
elements (`range(n)` gives an `int`).

## Complexity

Linear in the size of the range: exactly n − 1 comparisons for n elements.

## Exceptions

- (1) None when the `<` of the elements is noexcept; otherwise what it throws.
- (2) None when the copy of `cmp` and its call on two const elements are noexcept; otherwise what they throw.

## Notes

The sorted containers (`sorted_set`, `sorted_multiset`, `sorted_map`, `sorted_multimap`) hide both with a `min()`
of their own, the first element of their order, in constant time; they have no `min(cmp)`, since they are ordered
by their own comparator.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x, y;  // no ==, no <
};

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.min());

    // a comparator asks nothing of point
    vector<point> pts = {{1, 2}, {3, 0}};
    println("{}", pts.min([](point a, point b) { return a.y < b.y; }).x);

    println("{}", range(5, 10).min());
}
```

Output:

```text
3
3
5
```

## See also

- [max](max.md): the largest element
- [sort](../ordered/sort.md): sorts the elements
- [sgcl::mixin::enumerable\<Derived\>](../enumerable.md)
