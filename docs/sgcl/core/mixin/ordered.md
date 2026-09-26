# sgcl::mixin::ordered

```cpp
#include "sgcl/core/mixin/ordered.h"   // or "sgcl/core/mixin/mixin.h", "sgcl/sgcl.h"

namespace sgcl::mixin {
    template<class Derived>
    class ordered;
}
```

`mixin::ordered<Derived>` gives a class the order of the whole range as members — whether it is sorted, the searches that assume it is, and the sorting that makes it so — and declares that the range has one: `req::ordered<R>` is "R carries `mixin::ordered` and its elements are comparable" ([the mixins](README.md)). The sequences, `slice`, `range` and the immutable `immutable::vector` and `immutable::list` carry it; the sets and maps do not (their order is the container's, `lower_bound` their own).

## Rules

- Every method exists only for elements that are ordered (`req::comparable`: `<=>` or `<`), or takes a comparator or a key and asks nothing of the element.
- The sorts exist only where the elements can be written (`req::sequence`) and reached by position (`req::random_access`): an immutable vector is ordered — `is_sorted`, `binary_search` — but not sorted in place. `list` and `forward_list` have a `sort` of their own, on the nodes, which hides these.
- The searches assume a sorted range, by `<` or by the comparator given, and take O(log n) comparisons on a random-access range, O(n) steps on a list. A sorted `vector` with them is the flat map of this library: the lookups of a `sorted_map` with the memory of a `vector`.
- One mixin holds every overload of a name: `sort()`, `sort(cmp)`, `sort_by(proj)` are all here, not split between this and `mixin::sequence`, because a name in two bases is ambiguous.

## Members

```cpp
bool is_sorted() const;  template<class Compare> bool is_sorted(Compare cmp) const;
bool binary_search(const auto& value) const;         // whether the value is there; and with a comparator
size_t sorted_index_of(const auto& value) const;     // its position, npos when not; and with a comparator
auto lower_bound(const auto& value);                 // the first position not less than the value; and const, and with a comparator
auto upper_bound(const auto& value);                 // the first greater
void sort();  template<class Compare> void sort(Compare cmp);   // in place, by <, by the comparator
template<class Proj> void sort_by(Proj proj);        // by a key taken from the element: v.sort_by(&item::name)
void stable_sort();  template<class Compare> void stable_sort(Compare cmp);
```

```cpp
vector v = {5, 3, 9, 3};
v.sort();                                        // 3 3 5 9
assert(v.is_sorted() && v.binary_search(5) && v.sorted_index_of(9) == 3 && v.sorted_index_of(4) == npos);
assert(*v.lower_bound(4) == 5 && v.upper_bound(9) == v.end());
v.insert(v.lower_bound(4), 4);                   // where a new value goes to keep the order
struct item { string name; int price; };
vector<item> items = {{"tea", 3}, {"bread", 2}};
items.sort_by(&item::price);                     // no < on item needed
items.stable_sort([](const item& a, const item& b) { return a.name < b.name; });
immutable::vector<int> iv = immutable::vector<int>().push_back(1).push_back(2);
assert(iv.is_sorted() && iv.binary_search(2));   // ordered; no sort(): nothing is written in place
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

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
    std::cout << (keys.is_sorted() ? "sorted" : "not sorted") << ", 30 at " << keys.sorted_index_of(30) << "\n";
    return 0;
}
```

The output:

```
sorted, 30 at 2
```

## See also

- [the mixins and the requirements](README.md); [mixin::enumerable](enumerable.md) (`min`, `max`, `contains`), [mixin::sequence](sequence.md) (`fill`, `reverse`)
