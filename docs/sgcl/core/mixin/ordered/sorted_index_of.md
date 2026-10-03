[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::sorted_index_of

```cpp
constexpr size_t sorted_index_of(const auto& value) const noexcept(/* see below */);    // (1)
template<class Compare>
constexpr size_t sorted_index_of(const auto& value,                                     // (2)
                                 Compare cmp) const noexcept(/* see below */);
```

Finds the position of an element equivalent to `value` in a sorted range, by a binary search: the first of the
equivalent ones, where [lower_bound](lower_bound.md) stands. The range must be sorted by the order the search
uses; on one that is not, the answer is unspecified.

1. By `<`, the range sorted by it. Takes part only when the elements are `req::comparable`; `value` may be of any
   type the elements compare with by `<`, both ways.
2. By `cmp`, the range sorted by it; `cmp` is called with an element and `value`, both ways. Takes part only when
   `cmp` is a strict weak order of the elements.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to look for |
| `cmp` | the order, called with a const element and `value` either way round, returning what converts to `bool` |

## Return value

The position of the first element equivalent to `value`, `npos` when there is none.

## Complexity

Logarithmic in the size of the range in comparisons (at most log2(n) + O(1)); in steps of the iterator,
logarithmic on a random-access range, linear otherwise (a `list`).

## Exceptions

- (1) None when the `<` of an element with `value` and of `value` with an element are noexcept; otherwise what
  they throw.
- (2) None when the copy of `cmp` and its calls on a const element and `value`, either way round, are noexcept;
  otherwise what they throw.

## Notes

`sorted_index_of` is the [index_of](../enumerable/index_of.md) of a sorted range: the same answer, in
logarithmic time instead of linear.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {3, 3, 5, 9};
    println("{} {}", v.sorted_index_of(9), v.sorted_index_of(3));
    println("{}", v.sorted_index_of(4) == npos);

    vector descending = {7, 5, 3, 1};
    println("{}", descending.sorted_index_of(5, [](int a, int b) { return a > b; }));
}
```

Output:

```text
3 0
true
1
```

## See also

- [binary_search](binary_search.md): checks whether a sorted range holds a value
- [lower_bound](lower_bound.md): the first element not less than a value
- [index_of](../enumerable/index_of.md): the position of the first element equal to a value, on any range
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
