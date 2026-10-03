[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::sorted_set

```cpp
sorted_set()                                                                             // (1)
    noexcept(std::is_nothrow_default_constructible_v<Compare> &&
             std::is_nothrow_copy_constructible_v<Compare>);
explicit sorted_set(const Compare& comp)                                                 // (2)
    noexcept(std::is_nothrow_copy_constructible_v<Compare>);
template<std::input_iterator InputIt>
sorted_set(InputIt first, InputIt last, const Compare& comp = Compare());                // (3)
sorted_set(std::initializer_list<value_type> ilist, const Compare& comp = Compare());    // (4)
sorted_set(const sorted_set&) = delete;                                                  // (5)
```

Constructs a set from one of the sources below.

1. An empty set: the head node, a sentinel of the maximum height and no key, on the managed heap.
2. An empty set whose keys are ordered by `comp`.
3. A set of the keys of the range `[first, last)`, built at once: the keys are taken into a buffer, sorted with
   the first of two equivalent keys kept, as `insert` would keep it, and the nodes are linked in that order behind
   the last one at each of their levels, a plain store each and no search.
4. A set of the keys of `ilist`, built as (3).
5. The set is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparison that orders the keys |
| `first`, `last` | the range the keys are made from |
| `ilist` | the list the keys are made from |

## Complexity

- (1–2) Constant: one allocation.
- (3–4) The sort of the keys, *n* log *n* comparisons, then linear in their number: a node and a store per link
  each, and no search, where an empty set and an insert per key pay a search per key.

## Exceptions

- (1–2) What the construction of `Compare` (its default constructor, the copy of `comp`) throws; none when
  it is noexcept.
- (3–4) What the construction of `Key` from an element of the range, or its move, throws.

If an exception is thrown, no set is constructed; the nodes made before it are left to the collector.

## Notes

The constructors from a range (3–4) do without the search and the compare-exchanges of the inserts because no
other thread can see the set before it is constructed. A set that the threads fill as they go is the empty set
(1) and [insert](insert.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <type_traits>

using namespace sgcl;

struct Index {
    concurrent::sorted_set<string> words;  // a member of a managed object
};

int main() {
    concurrent::sorted_set<int> empty;  // on the stack
    tracked_ptr index = make_tracked<Index>();
    println("{} {}", empty.size(), index->words.empty());

    concurrent::sorted_set<int, std::greater<int>> descending = {1, 3, 2};
    println("{}", descending);

    vector<string> words = {"pear", "apple", "pear", "fig"};
    concurrent::sorted_set<string> from_range(words.begin(), words.end());
    println("{}", from_range);

    println("{}", std::is_copy_constructible_v<concurrent::sorted_set<int>>);
}
```

Output:

```text
0 true
{3, 2, 1}
{"apple", "fig", "pear"}
false
```

## See also

- [insert](insert.md): inserts into a set the threads share
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
