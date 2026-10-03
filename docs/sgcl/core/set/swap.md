[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::swap

```cpp
void swap(set& other) noexcept(std::is_nothrow_swappable_v<hasher> &&      // (1)
                               std::is_nothrow_swappable_v<key_equal>);
friend void swap(set& lhs, set& rhs) noexcept(noexcept(lhs.swap(rhs)));    // (2)
```

Exchanges the contents of two sets: the tables, the counts, the load factors, the hashers and the equalities.
No element is touched, and every iterator keeps pointing at its element, now in the other set.

1. Exchanges the contents of this set and `other`.
2. Exchanges the contents of `lhs` and `rhs`, as `lhs.swap(rhs)`: a hidden friend, found by the arguments'
   type, so that `swap(a, b)` and `std::ranges::swap(a, b)` call it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set to exchange the contents with |
| `lhs`, `rhs` | the sets whose contents to exchange |

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
    set<int> a = {1};
    set<int> b = {2, 3};
    auto it = a.begin();

    swap(a, b);  // it still points at 1, which is in b now
    println("{} {} {}", a.size(), b.size(), it == b.find(1));

    a.swap(b);
    println("{} {}", a.contains(1), b.contains(2));
}
```

Output:

```text
2 1 true
true true
```

## See also

- [merge](merge.md): relinks the nodes of another set
- [operator=](operator_assign.md): replaces the elements of a set
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
