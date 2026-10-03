[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [ordered](README.md)

# sgcl::mixin::ordered\<Derived\>::stable_sort

```cpp
void stable_sort() noexcept(/* see below */);                                       // (1)
template<class Compare> void stable_sort(Compare cmp) noexcept(/* see below */);    // (2)
```

Sorts the elements in place, in ascending order, keeping the order of equivalent elements: of two that neither
goes before the other, the one that was first stays first.

1. By `<`. Takes part only when the elements are `req::comparable`.
2. By `cmp`, `cmp(a, b)` true when `a` goes before `b`. Takes part only when `cmp` is a strict weak order of the
   elements; it asks nothing of the element, which needs no `<`.

- (1–2) Take part only where the elements can be written (the range is `req::sequence`) and reached by position
  (`req::random_access`).

Elements that may hold tracked pointers (a `tracked_ptr`, a `string`, a pair or a struct with one) are never moved
into memory the collector does not scan, as the standard's `stable_sort` would move them (its buffer is
`operator new`'s; [The rules](../../README.md#the-rules), 1): their positions are sorted instead, plain numbers in
a plain buffer, by `std::stable_sort`, and the elements are then moved once each into place. Elements with no
tracked pointer (an `int`, a struct of numbers) go through `std::stable_sort` as they are.

## Parameters

| Parameter | Description |
|---|---|
| `cmp` | the order, called with two elements and returning what converts to `bool` |

## Return value

None.

## Complexity

O(n log n) comparisons for n elements when the standard's `stable_sort` gets the memory for its buffer,
O(n log² n) otherwise. Elements that may hold tracked pointers are moved about once each: one move per element out
of place, and one more per cycle of the permutation.

## Exceptions

- (1) None when the move constructor, the move assignment and the swap of the elements and their `<` are
  noexcept; otherwise what they throw.
- (2) None when the move constructor, the move assignment and the swap of the elements, the copy of `cmp` and its
  call on two elements are noexcept; otherwise what they throw.

If an exception is thrown, the elements are valid, but their order, and after a move that threw their values, are
unspecified, as with `std::stable_sort`.

## Notes

Sorting the positions is faster than the standard's `stable_sort` on the elements themselves, whose moves cost a
write barrier each: `pair<int, tracked_ptr<T>>` on M-series, 10 000 in 0.54 ms (the standard's 1.1 ms), 100 000
in 8.8 ms (11.2 ms). The positions take 8 bytes an element of plain memory for the time of the sort, and the sort
runs no collection.

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
    v.stable_sort();
    println("{}", v);

    vector<item> basket = {{"tea", 3}, {"jam", 2}, {"milk", 3}, {"egg", 2}};
    basket.stable_sort([](const item& a, const item& b) { return a.price < b.price; });
    println("{} {} {} {}", basket[0].name, basket[1].name, basket[2].name, basket[3].name);

    basket.stable_sort([](const item& a, const item& b) { return a.name < b.name; });
    println("{} {}", basket[0].name, basket[3].name);
}
```

Output:

```text
[3, 3, 5, 9]
jam egg tea milk
egg tea
```

## See also

- [sort](sort.md): sorts the elements, not keeping the order of equivalent ones
- [sort_by](sort_by.md): sorts the elements by a key taken from each
- [sgcl::mixin::ordered\<Derived\>](README.md)
