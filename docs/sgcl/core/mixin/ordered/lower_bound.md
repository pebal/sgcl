[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](../ordered.md)

# sgcl::mixin::ordered\<Derived\>::lower_bound

```cpp
constexpr auto lower_bound(const auto& value) noexcept(/* see below */);                       // (1)
constexpr auto lower_bound(const auto& value) const noexcept(/* see below */);                 // (2)
template<class Compare>
constexpr auto lower_bound(const auto& value, Compare cmp) noexcept(/* see below */);          // (3)
template<class Compare>
constexpr auto lower_bound(const auto& value, Compare cmp) const noexcept(/* see below */);    // (4)
```

Finds, in a sorted range, the first element that does not go before `value`: where `value` would be inserted to
keep the order, before the elements equivalent to it. The range must be sorted by the order the search uses (it
needs only to be split by it: every element that goes before `value` ahead of every one that does not).

- (1–2) By `<`. Take part only when the elements are `req::comparable`; `value` may be of any type the elements
  compare with by `<`, both ways.
- (3–4) By `cmp`; `cmp` is called with an element and `value`. Take part only when `cmp` is a strict weak order of
  the elements.
- (2), (4) On a const range.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value to compare the elements with |
| `cmp` | the order, called with an element and `value`, returning what converts to `bool` |

## Return value

- (1), (3) An iterator of the range to the first element not before `value`; `end()` when every element goes
  before it.
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

The sorted containers (`sorted_set`, `sorted_map` and their multi forms) do not carry this mixin: their
`lower_bound` is their own, by the key, down the tree.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {3, 3, 5, 9};
    println("{} {}", *v.lower_bound(4), v.lower_bound(3) - v.begin());

    v.insert(v.lower_bound(4), 4);  // where a new value goes to keep the order
    println("{}", v);

    vector descending = {7, 5, 3, 1};
    println("{}", *descending.lower_bound(4, [](int a, int b) { return a > b; }));
}
```

Output:

```text
5 0
[3, 3, 4, 5, 9]
3
```

## See also

- [upper_bound](upper_bound.md): the first element greater than a value
- [sorted_index_of](sorted_index_of.md): the position of a value in a sorted range
- [sgcl::mixin::ordered\<Derived\>](../ordered.md)
