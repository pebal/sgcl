[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](README.md)

# sgcl::mixin::ordered\<Derived\>::sort_by

```cpp
template<class Proj> constexpr void sort_by(Proj proj) noexcept(/* see below */);
```

Sorts the elements in place by a key taken from each: `proj` called with an element gives its key, and the keys
are compared with `<`, in ascending order. `proj` may be a pointer to a member (`&item::name`) or any function of
an element. The element needs no `<` of its own; the key does. The sort is not stable: of elements with
equivalent keys, the order after it is unspecified.

Takes part only when `proj` is callable with a const element and gives a key that has `<` (`req::comparable`),
and where the elements can be written (the range is `req::sequence`) and reached by position
(`req::random_access`).

## Parameters

| Parameter | Description |
|---|---|
| `proj` | the projection: called with an element, it gives the key to sort by |

## Return value

None.

## Complexity

O(n log n) comparisons of keys and calls of `proj` for n elements.

## Exceptions

None when the move constructor, the move assignment and the swap of the elements, the copy of `proj`, its call on
an element and the `<` of the keys are noexcept; otherwise what they throw.

If an exception is thrown, the elements are valid, but their order, and after a move that threw their values, are
unspecified, as with `std::sort`.

## Notes

`sort_by(proj)` is `std::ranges::sort(r, {}, proj)`, as a member: the comparison and the key in one argument.

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
    vector<item> items = {{"tea", 3}, {"bread", 2}, {"apple", 4}};
    items.sort_by(&item::price);  // no < on item needed
    println("{} {} {}", items[0].name, items[1].name, items[2].name);

    vector<string> words = {"plum", "fig", "banana"};
    words.sort_by([](const string& w) { return w.size(); });
    println("{}", words);
}
```

Output:

```text
bread tea apple
["fig", "plum", "banana"]
```

## See also

- [sort](sort.md): sorts the elements by `<` or by a comparator
- [stable_sort](stable_sort.md): sorts the elements, keeping the order of equivalent ones
- [sgcl::mixin::ordered\<Derived\>](README.md)
