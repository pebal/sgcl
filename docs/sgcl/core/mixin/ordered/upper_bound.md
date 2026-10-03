[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::upper_bound

```cpp
/*(1)*/ constexpr auto upper_bound(const auto& value) noexcept(/* see below */);
/*(2)*/ constexpr auto upper_bound(const auto& value) const noexcept(/* see below */);
/*(3)*/ template<class Compare>
        constexpr auto upper_bound(const auto& value, Compare cmp) noexcept(/* see below */);
/*(4)*/ template<class Compare>
        constexpr auto upper_bound(const auto& value, Compare cmp) const noexcept(/* see below */);
```

Finds, in a sorted range, the first element that goes after `value`: where `value` would be inserted to keep the
order, after the elements equivalent to it. The range must be sorted by the order the search uses (it needs only
to be split by it: every element that does not go after `value` ahead of every one that does).

- (1–2) By `<`. Take part only when the elements are `req::comparable`; `value` may be of any type the elements
  compare with by `<`, both ways.
- (3–4) By `cmp`; `cmp` is called with `value` and an element. Take part only when `cmp` is a strict weak order of
  the elements.
- (2), (4) On a const range.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to compare the elements with |
| `cmp` | the order, called with `value` and an element, returning what converts to `bool` |

## Return value

- (1), (3) An iterator of the range to the first element after `value`; `end()` when no element goes after it.
- (2), (4) The same as a const iterator.

## Complexity

Logarithmic in the size of the range in comparisons (at most log2(n) + 1); in steps of the iterator,
logarithmic on a random-access range, linear otherwise (a `list`).

## Exceptions

- (1–2) None when the `<` of an element with `value` and of `value` with an element are noexcept; otherwise what
  they throw.
- (3–4) None when the copy of `cmp` and its calls on an element and `value`, either way round, are noexcept;
  otherwise what they throw.

## Notes

The elements equivalent to `value` are those from [lower_bound](lower_bound.md) up to `upper_bound`. The sorted
containers (`sorted_set`, `sorted_map` and their multi forms) do not carry this mixin: their `upper_bound` is
their own, by the key, down the tree.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {3, 3, 5, 9};
    println("{} {}", *v.upper_bound(3), v.upper_bound(9) == v.end());
    println("{}", v.upper_bound(3) - v.lower_bound(3));

    const list<int> l = {1, 2, 2, 4};
    println("{}", *l.upper_bound(2));
}
```

Output:

```text
5 true
2
4
```

## See also

- [lower_bound](lower_bound.md): the first element not less than a value
- [binary_search](binary_search.md): checks whether a sorted range holds a value
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
