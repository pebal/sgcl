[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::sorted_multimap

```cpp
/*(1)*/ sorted_multimap() noexcept(std::is_nothrow_default_constructible_v<key_compare>);
/*(2)*/ explicit sorted_multimap(const key_compare& comp)
            noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
/*(3)*/ template<std::input_iterator InputIt>
        sorted_multimap(InputIt first, InputIt last, const key_compare& comp = key_compare());
/*(4)*/ sorted_multimap(std::initializer_list<value_type> ilist,
                        const key_compare& comp = key_compare());
/*(5)*/ sorted_multimap(const sorted_multimap& other);
/*(6)*/ sorted_multimap(sorted_multimap&& other)
            noexcept(std::is_nothrow_move_constructible_v<key_compare>);
```

Constructs a multimap from one of the sources below.

1. An empty multimap. Nothing is allocated: the header node is made on the first insertion.
2. An empty multimap whose keys are ordered by `comp`.
3. The elements of the range `[first, last)`, inserted in order with `end()` as the hint, so that sorted input
   costs one comparison per element; every element is kept, equivalent keys in the order of the range. A range of
   another type (pairs of a `std::string_view` and an `int` for `string` keys) is converted once per element, into
   its node, before the node's key is compared: the source need not be comparable with the keys at all.
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
| `other` | the multimap the elements are copied or taken from |

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
no multimap is constructed.

## Notes

The deduction guides of the class ([sorted_multimap](../sorted_multimap.md#deduction-guides)) give the key and
the mapped type from a range of pairs or from an initializer list whose pairs are spelled out
(`std::pair{1, 2.0}`): a braced pair alone names no type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_multimap<string, int> scores = {{"bob", 5}, {"ann", 3}, {"ann", 7}};
    sorted_multimap<int, char, std::greater<int>> descending({{1, 'a'}, {2, 'b'}, {1, 'c'}});
    println("{} {}", scores, descending);

    vector<pair<int, string>> pairs = {{2, "b"}, {1, "a"}, {2, "z"}};
    sorted_multimap<int, string> from_range(pairs.begin(), pairs.end());
    println("{}", from_range);  // both 2s kept, in the order of the range

    sorted_multimap copy = scores;
    sorted_multimap taken = std::move(scores);
    println("{} {} {}", copy.size(), taken.size(), scores.empty());
}
```

Output:

```text
{"ann": 3, "ann": 7, "bob": 5} {2: 'b', 1: 'a', 1: 'c'}
{1: "a", 2: "b", 2: "z"}
3 3 true
```

The deduction guides:

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int main() {
    sorted_multimap m = {std::pair{1, 2.0}, std::pair{1, 3.0}};
    sorted_multimap from_range(m.begin(), m.end());

    println("{}", std::is_same_v<decltype(m), sorted_multimap<int, double>>);
    println("{}", std::is_same_v<decltype(from_range), sorted_multimap<int, double>>);
}
```

Output:

```text
true
true
```

## See also

- [operator=](operator_assign.md): replaces the contents of a multimap
- [insert](insert.md): inserts elements into a multimap
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
