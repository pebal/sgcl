[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::concurrent::sorted_map\<Key, T, Compare\>::sorted_map

```cpp
sorted_map()                                                                             // (1)
    noexcept(std::is_nothrow_default_constructible_v<Compare> &&
             std::is_nothrow_copy_constructible_v<Compare>);
explicit sorted_map(const Compare& comp)                                                 // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Compare>);
template<std::input_iterator InputIt>
sorted_map(InputIt first, InputIt last, const Compare& comp = Compare());                // (3)
sorted_map(std::initializer_list<value_type> ilist, const Compare& comp = Compare());    // (4)
sorted_map(const sorted_map&) = delete;                                                  // (5)
```

Constructs a map from one of the sources below.

1. An empty map: the head node, a sentinel of the maximum height and no element, on the managed heap.
2. An empty map whose keys are ordered by `comp`.
3. A map of the elements of the range `[first, last)`, built at once: the elements are taken into a buffer, sorted
   by key with the first of two equal keys kept, as `insert` would keep it, and the nodes are linked in that order
   behind the last one at each of their levels, a plain store each and no search.
4. A map of the elements of `ilist`, built as (3).
5. The map is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparison that orders the keys |
| `first`, `last` | the range the elements are made from |
| `ilist` | the list the elements are made from |

## Complexity

- (1–2) Constant: one allocation.
- (3–4) The sort of the elements, *n* log *n* comparisons, then linear in their number: a node and a store per
  link each, and no search. 110 to 200 ns per element for 200,000 random keys, against 380 to 600 for an empty map
  and an insert per element.

## Exceptions

- (1–2) What the construction of `Compare` (its default constructor, the copy of `comp`) throws; none when
  it is noexcept.
- (3–4) What the construction of `value_type` from an element of the range, or its move, throws.

If an exception is thrown, no map is constructed; the nodes made before it are left to the collector.

## Notes

The constructors from a range (3–4) do without the search and the compare-exchanges of the inserts because no
other thread can see the map before it is constructed. A map that the threads fill as they go is the empty map
(1) and [insert](insert.md) or [try_emplace](try_emplace.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <type_traits>

using namespace sgcl;

struct Registry {
    concurrent::sorted_map<int, string> names;  // a member of a managed object
};

int main() {
    concurrent::sorted_map<int, string> empty;  // on the stack
    tracked_ptr registry = make_tracked<Registry>();
    println("{} {}", empty.size(), registry->names.empty());

    concurrent::sorted_map<string, int, std::greater<string>> descending({{"a", 1}, {"c", 3}});
    println("{}", descending);

    vector<pair<int, string>> pairs = {{3, "c"}, {1, "a"}, {3, "z"}, {2, "b"}};
    concurrent::sorted_map<int, string> from_range(pairs.begin(), pairs.end());
    println("{}", from_range);  // the first of the two 3s kept

    println("{}", std::is_copy_constructible_v<concurrent::sorted_map<int, string>>);
}
```

Output:

```text
0 true
{"c": 3, "a": 1}
{1: "a", 2: "b", 3: "c"}
false
```

## See also

- [insert](insert.md), [try_emplace](try_emplace.md): insert into a map the threads share
- [sgcl::concurrent::sorted_map\<Key, T, Compare\>](../sorted_map.md)
