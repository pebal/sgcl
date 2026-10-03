[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [enumerable](README.md)

# sgcl::mixin::enumerable\<Derived\>::max

```cpp
constexpr decltype(auto) max() const noexcept(/* see below */);               // (1)
template<class Compare>
constexpr decltype(auto) max(Compare cmp) const noexcept(/* see below */);    // (2)
```

Finds the largest element, walking the whole range; of several equal largest, the first.

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

A const reference to the largest element, or the value itself where the iterator gives values rather than
elements (`range(n)` gives an `int`).

## Complexity

Linear in the size of the range: exactly n − 1 comparisons for n elements.

## Exceptions

- (1) None when the `<` of the elements is noexcept; otherwise what it throws.
- (2) None when the copy of `cmp` and its call on two const elements are noexcept; otherwise what they throw.

## Notes

The sorted containers (`sorted_set`, `sorted_multiset`, `sorted_map`, `sorted_multimap`) hide both with a `max()`
of their own, the last element of their order, in constant time; they have no `max(cmp)`, since they are ordered
by their own comparator.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct item {
    string name;
    int price;
};

int main() {
    vector v = {5, 3, 9, 3};
    println("{}", v.max());

    vector<item> items = {{"tea", 3}, {"bread", 2}, {"apple", 4}};
    const item& dearest = items.max([](const item& a, const item& b) { return a.price < b.price; });
    println("{}", dearest.name);

    sorted_set<string> words = {"pear", "fig", "plum"};
    println("{}", words.max());
}
```

Output:

```text
9
apple
plum
```

## See also

- [min](min.md): the smallest element
- [sort](../ordered/sort.md): sorts the elements
- [sgcl::mixin::enumerable\<Derived\>](README.md)
