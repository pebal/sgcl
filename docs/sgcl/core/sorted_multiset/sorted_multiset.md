[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::sorted_multiset

```cpp
/*(1)*/ sorted_multiset() = default;
/*(2)*/ explicit sorted_multiset(const key_compare& comp)
            noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
/*(3)*/ template<std::input_iterator InputIt>
        sorted_multiset(InputIt first, InputIt last, const key_compare& comp = key_compare());
/*(4)*/ sorted_multiset(std::initializer_list<value_type> ilist,
                        const key_compare& comp = key_compare());
/*(5)*/ sorted_multiset(const sorted_multiset& other) = default;
/*(6)*/ sorted_multiset(sorted_multiset&& other) = default;
```

Constructs a multiset from one of the sources below.

1. An empty multiset with `Compare()`. It allocates nothing: the header node of the tree is made on the first
   insertion.
2. An empty multiset with the comparison `comp`; nothing allocated either.
3. The elements of the range `[first, last)`, every one of them, inserted one by one with `end()` as the hint, so a
   sorted range costs one comparison per element; equivalent keys keep the order of the range. A range of another
   type than `Key` (`string_view`s for `string` keys) has each element converted once, into its node, before the
   node's key is compared: the source need not be comparable with the keys at all.
4. The elements of `ilist`, as (3).
5. A copy of `other`, with its comparison, its elements in the same order. The tree is copied shape for shape: a
   node per element with its colour and its links, no comparison and no rebalancing.
6. Takes the tree of `other` over, with its comparison; `other` is left empty. No element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparison of the keys |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |
| `other` | the multiset to copy or to move from |

## Complexity

- (1–2) Constant.
- (3–4) *N* log *N* in the number of elements *N*; linear when the elements come sorted.
- (5) Linear in the size of `other`.
- (6) Constant.

## Exceptions

- (1) What the default constructor of `Compare` throws; none when it is noexcept.
- (2) What the copy of `Compare` throws; none when it is noexcept.
- (3–4) What the construction of an element from `*first` (from an element of `ilist`) or the copy of `comp`
  throws.
- (5) What the copy of an element or of `Compare` throws.
- (6) What the move of `Compare` throws; none when it is noexcept.

If an exception is thrown while (3–5) copy the elements, the elements built so far are destroyed and no multiset
is constructed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <string_view>

using namespace sgcl;

int main() {
    sorted_multiset rolls = {4, 2, 4, 6, 2};  // sorted_multiset<int>, every element kept
    println("{}", rolls);

    sorted_multiset<int, std::greater<int>> descending(std::greater<int>{});
    descending.insert({1, 3, 1});
    println("{}", descending);

    vector<std::string_view> words = {"b", "a", "b"};
    sorted_multiset<string> from_range(words.begin(), words.end());  // a string made per element
    println("{}", from_range);

    sorted_multiset copy = rolls;
    sorted_multiset<int> taken = std::move(rolls);
    println("{} {} {}", copy.size(), taken.size(), rolls.empty());
}
```

Output:

```text
{2, 2, 4, 4, 6}
{3, 1, 1}
{"a", "b", "b"}
5 5 true
```

## See also

- [operator=](operator_assign.md): assigns another multiset or a list
- [insert](insert.md): inserts elements into a multiset
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
