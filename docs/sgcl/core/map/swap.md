[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::swap

```cpp
void swap(map& other)                                                                           // (1)
    noexcept(std::is_nothrow_swappable_v<hasher> && std::is_nothrow_swappable_v<key_equal>);
friend void swap(map& lhs, map& rhs) noexcept(noexcept(lhs.swap(rhs)));                         // (2)
```

Exchanges the contents of two maps: their tables, counts, load factors, hashers and equalities. No element is
touched, and every iterator keeps pointing at its element, now in the other map.

1. Exchanges the contents of this map and `other`.
2. `lhs.swap(rhs)`, as a non-member found by argument-dependent lookup: `using std::swap; swap(a, b);` calls it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map to exchange the contents with |
| `lhs`, `rhs` | the maps whose contents to exchange |

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
    map<int, string> a = {{1, "one"}};
    map<int, string> b = {{2, "two"}, {3, "three"}};
    auto it = a.begin();

    swap(a, b);  // it still points at {1, "one"}, in b now
    println("{} {} {}", a.size(), b.size(), it == b.find(1));

    a.swap(b);
    println("{} {}", a.at(1), b.at(3));
}
```

Output:

```text
2 1 true
one three
```

## See also

- [operator=](operator_assign.md): replaces the elements of a map
- [merge](merge.md): relinks the nodes of another map
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
