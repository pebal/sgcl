[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::sorted_map

```cpp
sorted_map() noexcept(std::is_nothrow_default_constructible_v<key_compare>);                   // (1)
explicit sorted_map(const key_compare& comp)                                                   // (2)
    noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
template<std::input_iterator InputIt>
sorted_map(InputIt first, InputIt last, const key_compare& comp = key_compare());              // (3)
sorted_map(std::initializer_list<value_type> ilist,                                            // (4)
           const key_compare& comp = key_compare());
sorted_map(const sorted_map& other);                                                           // (5)
sorted_map(sorted_map&& other) noexcept(std::is_nothrow_move_constructible_v<key_compare>);    // (6)
```

Constructs a map from one of the sources below.

1. An empty map. Nothing is allocated: the header node is made on the first insertion.
2. An empty map whose keys are ordered by `comp`.
3. The elements of the range `[first, last)`, inserted in order with `end()` as the hint, so that sorted input
   costs one comparison per element; of two equal keys the first is kept. A range of another type (pairs of a
   `std::string_view` and an `int` for `string` keys) is converted once per element, into its node, before the
   node's key is compared: the source need not be comparable with the keys at all.
4. The elements of `ilist`, inserted as (3) inserts them.
5. A copy of `other`, with nodes of its own in the same order and `other`'s comparator: the tree copied shape for
   shape, a node per element with its colour and its links, no comparison and no rebalancing.
6. Takes the tree of `other` over, and its comparator moved; `other` is empty after.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparison that orders the keys |
| `first`, `last` | the range the elements are made from |
| `ilist` | the list the elements are made from |
| `other` | the map the elements are copied or taken from |

## Complexity

- (1–2) Constant.
- (3–4) *n* log *n* comparisons for *n* elements; linear when they come sorted.
- (5) Linear in `other.size()`, with no comparison.
- (6) Constant.

## Exceptions

- (1–2) What the default constructor or the copy constructor of `Compare` throws; none when it is noexcept.
- (3–4) What the construction of `value_type` from an element throws, and the copy of `Compare`.
- (5) What the copy constructor of `value_type` (of `Key` and `T`) throws, and the copy of `Compare`.
- (6) What the move constructor of `Compare` throws; none when it is noexcept.

When an element's constructor throws, the elements built before it are destroyed and the exception propagates:
no map is constructed.

## Notes

The deduction guides of the class ([sorted_map](README.md#deduction-guides)) give the key and the mapped
type from a range of pairs or from an initializer list whose pairs are spelled out (`std::pair{1, 2.0}`): a
braced pair alone names no type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <string_view>

using namespace sgcl;

int main() {
    sorted_map<int, string> empty;
    sorted_map<string, int> ages = {{"bob", 27}, {"ann", 31}};
    sorted_map<int, int, std::greater<int>> descending({{1, 1}, {3, 3}, {2, 2}});
    println("{} {} {}", empty.size(), ages, descending);

    vector<pair<int, string>> pairs = {{2, "b"}, {1, "a"}, {2, "z"}};
    sorted_map<int, string> from_range(pairs.begin(), pairs.end());
    println("{}", from_range);  // the first of the two 2s kept

    // a range of another type: each pair converted once, into its node
    vector<pair<std::string_view, int>> views = {{"y", 2}, {"x", 1}};
    sorted_map<string, int> converted(views.begin(), views.end());
    println("{}", converted);

    sorted_map copy = ages;
    sorted_map taken = std::move(ages);
    println("{} {} {}", copy, taken, ages.empty());
}
```

Output:

```text
0 {"ann": 31, "bob": 27} {3: 3, 2: 2, 1: 1}
{1: "a", 2: "b"}
{"x": 1, "y": 2}
{"ann": 31, "bob": 27} {"ann": 31, "bob": 27} true
```

The deduction guides:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <type_traits>

using namespace sgcl;

int main() {
    sorted_map m = {std::pair{1, 2.0}, std::pair{2, 3.0}};
    sorted_map from_range(m.begin(), m.end());
    sorted_map greater({std::pair{1, 2}}, std::greater<int>());

    println("{}", std::is_same_v<decltype(m), sorted_map<int, double>>);
    println("{}", std::is_same_v<decltype(from_range), sorted_map<int, double>>);
    println("{}", std::is_same_v<decltype(greater), sorted_map<int, int, std::greater<int>>>);
}
```

Output:

```text
true
true
true
```

## See also

- [operator=](operator_assign.md): replaces the contents of a map
- [insert](insert.md): inserts elements into a map
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
