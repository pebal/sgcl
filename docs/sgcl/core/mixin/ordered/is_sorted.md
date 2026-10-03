[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::is_sorted

```cpp
/*(1)*/ constexpr bool is_sorted() const noexcept(/* see below */);
/*(2)*/ template<class Compare>
        constexpr bool is_sorted(Compare cmp) const noexcept(/* see below */);
```

Checks whether the elements are in order: no element goes before the one ahead of it. Equal neighbours are in
order.

1. By `<`. Takes part only when the elements are `req::comparable`.
2. By `cmp`, `cmp(a, b)` true when `a` goes before `b`. Takes part only when `cmp` is a strict weak order of the
   elements; it asks nothing of the element, which needs no `<`.

## Parameters

| Parameter | Description |
|---|---|
| `cmp` | the order, called with two const elements and returning what converts to `bool` |

## Return value

`true` when the elements are sorted, and on a range of fewer than two; `false` otherwise.

## Complexity

Linear in the size of the range: at most n − 1 comparisons for n elements, stopping at the first pair out of
order.

## Exceptions

- (1) None when the `<` of the elements is noexcept; otherwise what it throws.
- (2) None when the copy of `cmp` and its call on two const elements are noexcept; otherwise what they throw.

## Notes

An immutable container is ordered without being sorted in place: an `immutable::vector` has `is_sorted` and the
searches, and no `sort`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.is_sorted());
    v.sort();
    println("{}", v.is_sorted());
    println("{}", v.is_sorted([](int a, int b) { return a > b; }));

    // ordered, with no sort(): nothing is written in place
    immutable::vector<int> iv = immutable::vector<int>().push_back(1).push_back(2);
    println("{} {}", iv.is_sorted(), iv.binary_search(2));
}
```

Output:

```text
false
true
false
true true
```

## See also

- [sort](sort.md), [stable_sort](stable_sort.md): sort the elements
- [binary_search](binary_search.md): checks whether a sorted range holds a value
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
