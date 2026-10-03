[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::swap

```cpp
void swap(sorted_multiset& other) noexcept(std::is_nothrow_swappable_v<key_compare>);    // (1)
template<class Key, class Compare>
void swap(sorted_multiset<Key, Compare>& lhs, sorted_multiset<Key, Compare>& rhs)        // (2)
    noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of two multisets: their trees, their counts and their comparisons. No element is copied,
moved or destroyed, and every iterator keeps pointing at its element, which is in the other multiset now.

1. Exchanges the contents of this multiset and `other`.
2. A function of the namespace `sgcl`, not a member: `lhs.swap(rhs)`. An unqualified `swap(a, b)` finds it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multiset to exchange the contents with |
| `lhs`, `rhs` | the multisets to exchange the contents of |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the swap of `Compare` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multiset<int> a = {1, 1};
    sorted_multiset<int> b = {9};
    auto one = a.begin();

    swap(a, b);
    println("{} {}", a, b);
    println("{}", one == b.begin());  // the iterator followed its element

    a.swap(b);
    println("{} {}", a, b);
}
```

Output:

```text
{9} {1, 1}
true
{1, 1} {9}
```

## See also

- [operator=](operator_assign.md): replaces the contents
- [merge](merge.md): moves the nodes of another multiset into this one
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
