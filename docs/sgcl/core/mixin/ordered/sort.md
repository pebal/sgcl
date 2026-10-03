[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::sort

```cpp
constexpr void sort() noexcept(/* see below */);                                       // (1)
template<class Compare> constexpr void sort(Compare cmp) noexcept(/* see below */);    // (2)
```

Sorts the elements in place, in ascending order. The sort is not stable: of equivalent elements, the order after
it is unspecified ([stable_sort](stable_sort.md) keeps it).

1. By `<`. Takes part only when the elements are `req::comparable`.
2. By `cmp`, `cmp(a, b)` true when `a` goes before `b`. Takes part only when `cmp` is a strict weak order of the
   elements; it asks nothing of the element, which needs no `<`.

- (1–2) Take part only where the elements can be written (the range is `req::sequence`) and reached by position
  (`req::random_access`): a `vector`, an `array`, a `deque`, a `slice<T>`; not an immutable container, not a
  `slice<const T>`.

## Parameters

| Parameter | Description |
|---|---|
| `cmp` | the order, called with two elements and returning what converts to `bool` |

## Return value

None.

## Complexity

O(n log n) comparisons for n elements.

## Exceptions

- (1) None when the move constructor, the move assignment and the swap of the elements and their `<` are
  noexcept; otherwise what they throw.
- (2) None when the move constructor, the move assignment and the swap of the elements, the copy of `cmp` and its
  call on two elements are noexcept; otherwise what they throw.

If an exception is thrown, the elements are valid, but their order, and after a move that threw their values, are
unspecified, as with `std::sort`.

## Notes

`list` and `forward_list` hide both with a `sort` of their own, on the nodes: the elements are not moved, and
the sort is stable.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {5, 3, 9, 3};
    v.sort();
    println("{}", v);
    v.sort([](int a, int b) { return a > b; });
    println("{}", v);

    array<string, 3> words = {"pear", "fig", "plum"};
    words.sort();
    println("{}", words);
}
```

Output:

```text
[3, 3, 5, 9]
[9, 5, 3, 3]
["fig", "pear", "plum"]
```

## See also

- [stable_sort](stable_sort.md): sorts the elements, keeping the order of equivalent ones
- [sort_by](sort_by.md): sorts the elements by a key taken from each
- [is_sorted](is_sorted.md): checks whether the elements are sorted
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
