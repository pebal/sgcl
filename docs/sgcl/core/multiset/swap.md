[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::swap

```cpp
void swap(multiset& other) noexcept(std::is_nothrow_swappable_v<hasher> &&           // (1)
                                    std::is_nothrow_swappable_v<key_equal>);
friend void swap(multiset& lhs, multiset& rhs) noexcept(noexcept(lhs.swap(rhs)));    // (2)
```

Exchanges the contents of two multisets: the tables, the counts, the load factors, the hashers and the
equalities. No element is touched, and every iterator keeps pointing at its element, now in the other multiset.

1. Exchanges the contents of this multiset and `other`.
2. Exchanges the contents of `lhs` and `rhs`, as `lhs.swap(rhs)`: a hidden friend, found by the arguments'
   type, so that `swap(a, b)` and `std::ranges::swap(a, b)` call it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multiset to exchange the contents with |
| `lhs`, `rhs` | the multisets whose contents to exchange |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the swap of `Hash` or of `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset<int> a = {1, 1};
    multiset<int> b = {2};
    auto it = a.begin();

    swap(a, b);  // it still points at the first 1, which is in b now
    println("{} {} {}", a.size(), b.size(), it == b.find(1));

    a.swap(b);
    println("{} {}", a.count(1), b.contains(2));
}
```

Output:

```text
1 2 true
2 true
```

## See also

- [merge](merge.md): relinks the nodes of another multiset
- [operator=](operator_assign.md): replaces the elements of a multiset
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
