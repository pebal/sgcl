[sgcl](../../README.md) › [core](../README.md) › [sorted_map](README.md)

# sgcl::sorted_map\<Key, T, Compare\>::swap

```cpp
void swap(sorted_map& other) noexcept(std::is_nothrow_swappable_v<key_compare>);    // (1)
template<class Key, class T, class Compare>
void swap(sorted_map<Key, T, Compare>& lhs, sorted_map<Key, T, Compare>& rhs)       // (2)
    noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of two maps: their trees, counts and comparators. No element is touched, and every
iterator keeps pointing at its element, now in the other map; `end()` goes with its header.

1. Exchanges the contents of this map and `other`.
2. The non-member, declared in `sgcl`: `lhs.swap(rhs)`. `swap(a, b)` written without a namespace finds it by the
   arguments' type.

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

What the swap of `Compare` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<int, string> a = {{1, "one"}}, b = {{2, "two"}, {3, "three"}};
    auto it = a.begin();

    swap(a, b);
    println("{} {}", a, b);
    println("{} {}", it->second, it == b.begin());  // it follows its element into b

    a.swap(b);
    println("{} {}", a.size(), b.size());
}
```

Output:

```text
{2: "two", 3: "three"} {1: "one"}
one true
1 2
```

## See also

- [merge](merge.md): relinks the nodes of another map into this one
- [operator=](operator_assign.md): assigns values to the map
- [sgcl::sorted_map\<Key, T, Compare\>](README.md)
