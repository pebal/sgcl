[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::swap

```cpp
void swap(sorted_set& other) noexcept(std::is_nothrow_swappable_v<key_compare>);    // (1)
template<class Key, class Compare>
void swap(sorted_set<Key, Compare>& lhs, sorted_set<Key, Compare>& rhs)             // (2)
    noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of two sets: their trees, their counts and their comparisons. No element is copied, moved
or destroyed, and every iterator keeps pointing at its element, which is in the other set now.

1. Exchanges the contents of this set and `other`.
2. A function of the namespace `sgcl`, not a member: `lhs.swap(rhs)`. An unqualified `swap(a, b)` finds it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set to exchange the contents with |
| `lhs`, `rhs` | the sets to exchange the contents of |

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
    sorted_set<int> a = {1, 2};
    sorted_set<int> b = {9};
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
{9} {1, 2}
true
{1, 2} {9}
```

## See also

- [operator=](operator_assign.md): replaces the contents
- [merge](merge.md): moves the nodes of another set into this one
- [sgcl::sorted_set\<Key, Compare\>](README.md)
