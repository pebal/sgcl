[sgcl](../../README.md) › [core](../README.md) › [mixin](README.md)

# sgcl::mixin::ordered\<Derived\>

```cpp
#include "sgcl/core/mixin/ordered.h"   // or "sgcl/core.h"

namespace sgcl::mixin {
    template<class Derived>
    class ordered;
}
```

`mixin::ordered<Derived>` gives a class the order of the whole range as members — whether it is sorted, the
searches that assume it is, and the sorting that makes it so — and declares that the range has one:
`req::ordered<R>` is "R carries `mixin::ordered` and its elements are comparable" ([the mixins](README.md)). The
sequences, `slice`, `range` and the immutable `immutable::vector` and `immutable::list` carry it; the sets and
maps do not (their order is the container's, `lower_bound` their own).

What `std` gives as free algorithms (`std::ranges::sort`, `binary_search`, `lower_bound`) and Go as functions of
its `slices` package is here a member of every ordered range. A search that finds nothing by position returns
`npos` ([sorted_index_of](ordered/sorted_index_of.md)).

## Rules

- Every method exists only for elements that are ordered (`req::comparable`: `<=>` or `<`), or takes a comparator
  or a key and asks nothing of the element.
- The sorts exist only where the elements can be written (`req::sequence`) and reached by position
  (`req::random_access`): an immutable vector is ordered — `is_sorted`, `binary_search` — but not sorted in place.
  `list` and `forward_list` have a `sort` of their own, on the nodes, which hides these.
- The searches assume a sorted range, by `<` or by the comparator given, and take O(log n) comparisons on a
  random-access range, O(n) steps on a list. A sorted `vector` with them is the flat map of this library: the
  lookups of a `sorted_map` with the memory of a `vector`.
- `stable_sort` of elements that may hold tracked pointers (a `tracked_ptr`, a `string`, a pair or a struct with
  one) never moves them into memory the collector does not scan, as the standard's `stable_sort` would
  ([The rules](../README.md#the-rules), 1): their positions are sorted, plain numbers in a plain buffer of 8 bytes
  an element, and the elements moved once each into place, faster than the standard's sort of the elements
  themselves ([stable_sort](ordered/stable_sort.md)). Elements with no tracked pointer go through
  `std::stable_sort` as they are.
- One mixin holds every overload of a name: `sort()`, `sort(cmp)`, `sort_by(proj)` are all here, not split
  between this and `mixin::sequence`, because a name in two bases is ambiguous.
- Each method is noexcept as far as what it calls is: the element's `<`, the comparator or key given and its copy,
  and for the sorts the element's moves and swaps.

## Template parameters

| Parameter | Description |
|---|---|
| `Derived` | The class that carries the mixin and names itself as the argument (`class vector : public mixin::ordered<vector<T>>`). It gives `begin()` and `end()`, const and not, over its elements. |

## Member functions

| Function | Description |
|---|---|
| `(constructor)`, `(destructor)` | protected: the mixin exists only as a base |

#### Sorting

| Function | Description |
|---|---|
| [is_sorted](ordered/is_sorted.md) | checks whether the elements are sorted |
| [sort](ordered/sort.md) | sorts the elements |
| [sort_by](ordered/sort_by.md) | sorts the elements by a key taken from each |
| [stable_sort](ordered/stable_sort.md) | sorts the elements, keeping the order of equivalent ones |

#### Binary search

| Function | Description |
|---|---|
| [binary_search](ordered/binary_search.md) | checks whether a sorted range holds a value |
| [sorted_index_of](ordered/sorted_index_of.md) | the position of a value in a sorted range |
| [lower_bound](ordered/lower_bound.md) | the first element not less than a value |
| [upper_bound](ordered/upper_bound.md) | the first element greater than a value |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A flat map: a vector kept sorted, searched in O(log n). The function
// asks for what it uses — an ordered, writable range
void add_sorted(req::ordered auto& r, int x) requires req::sequence<decltype(r)> {
    r.insert(r.lower_bound(x), x);
}

int main() {
    vector<int> keys;
    for (int k : {40, 10, 30, 20}) {
        add_sorted(keys, k);
    }
    println("{}, 30 at {}", (keys.is_sorted() ? "sorted" : "not sorted"), keys.sorted_index_of(30));
}
```

Output:

```text
sorted, 30 at 2
```

## See also

- [req::ordered](../req/ordered.md): a range of comparable elements: what a function asks for to call these members
- [the mixins and the requirements](README.md); [mixin::enumerable](enumerable.md) (`min`, `max`, `contains`),
  [mixin::sequence](sequence.md) (`fill`, `reverse`)
- `tests/core/mixin.cpp`, `tests/containers/mixins.cpp`
