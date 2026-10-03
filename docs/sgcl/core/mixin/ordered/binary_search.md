[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::binary_search

```cpp
constexpr bool binary_search(const auto& value) const noexcept(/* see below */);    // (1)
template<class Compare>
constexpr bool binary_search(const auto& value,                                     // (2)
                             Compare cmp) const noexcept(/* see below */);
```

Checks whether a sorted range holds an element equivalent to `value`: one that neither goes before `value` nor
after it. The range must be sorted by the order the search uses; on one that is not, the answer is unspecified.

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

`true` when an element is equivalent to `value`, `false` otherwise.

## Complexity

Logarithmic in the size of the range in comparisons (at most log2(n) + O(1)); in steps of the iterator,
logarithmic on a random-access range, linear otherwise (a `list`).

## Exceptions

- (1) None when the `<` of an element with `value` and of `value` with an element are noexcept; otherwise what
  they throw.
- (2) None when the copy of `cmp` and its calls on a const element and `value`, either way round, are noexcept;
  otherwise what they throw.

## Notes

A sorted `vector` with the searches of this mixin is the flat map of this library: the lookups of a
`sorted_map`, with the memory of a `vector`. [sorted_index_of](sorted_index_of.md) gives the position of the
element found.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {3, 3, 5, 9};
    println("{} {}", v.binary_search(5), v.binary_search(4));

    vector descending = {7, 5, 3, 1};
    println("{}", descending.binary_search(3, [](int a, int b) { return a > b; }));

    list<string> names = {"Ada", "Grace", "Linus"};
    println("{}", names.binary_search("Grace"));
}
```

Output:

```text
true false
true
true
```

## See also

- [sorted_index_of](sorted_index_of.md): the position of a value in a sorted range
- [lower_bound](lower_bound.md), [upper_bound](upper_bound.md): the first element not less than, greater than a
  value
- [contains](../enumerable/contains.md): the same question by `==`, on any range
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
