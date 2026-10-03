[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::sorted_set

```cpp
sorted_set() = default;                                                              // (1)
explicit sorted_set(const key_compare& comp)                                         // (2)
    noexcept(std::is_nothrow_copy_constructible_v<key_compare>);
template<std::input_iterator InputIt>
sorted_set(InputIt first, InputIt last, const key_compare& comp = key_compare());    // (3)
sorted_set(std::initializer_list<value_type> ilist,                                  // (4)
           const key_compare& comp = key_compare());
sorted_set(const sorted_set& other) = default;                                       // (5)
sorted_set(sorted_set&& other) = default;                                            // (6)
```

Constructs a set from one of the sources below.

1. An empty set with `Compare()`. It allocates nothing: the header node of the tree is made on the first
   insertion.
2. An empty set with the comparison `comp`; nothing allocated either.
3. The elements of the range `[first, last)`, inserted one by one with `end()` as the hint, so a sorted range
   costs one comparison per element. Of several equivalent keys the first is kept. A range of another type than
   `Key` (`string_view`s for `string` keys) has each element converted once, into its node, before the node's key
   is compared: the source need not be comparable with the keys at all.
4. The elements of `ilist`, as (3).
5. A copy of `other`, with its comparison. The tree is copied shape for shape: a node per element with its colour
   and its links, no comparison and no rebalancing.
6. Takes the tree of `other` over, with its comparison; `other` is left empty. No element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `comp` | the comparison of the keys |
| `first`, `last` | the range the elements are copied from |
| `ilist` | the list the elements are copied from |
| `other` | the set to copy or to move from |

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

If an exception is thrown while (3–5) copy the elements, the elements built so far are destroyed and no set is
constructed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>
#include <string_view>

using namespace sgcl;

int main() {
    sorted_set primes = {5, 3, 2, 7, 3};  // sorted_set<int>, the second 3 dropped
    println("{}", primes);

    sorted_set<int, std::greater<int>> descending(std::greater<int>{});
    descending.insert({1, 3, 2});
    println("{}", descending);

    vector<std::string_view> words = {"pear", "apple", "fig"};
    sorted_set<string> from_range(words.begin(), words.end());  // a string made per element
    println("{}", from_range);

    sorted_set copy = primes;
    sorted_set<int> taken = std::move(primes);
    println("{} {} {}", copy.size(), taken.size(), primes.empty());
}
```

Output:

```text
{2, 3, 5, 7}
{3, 2, 1}
{"apple", "fig", "pear"}
4 4 true
```

## See also

- [operator=](operator_assign.md): assigns another set or a list
- [insert](insert.md): inserts elements into a set
- [sgcl::sorted_set\<Key, Compare\>](README.md)
