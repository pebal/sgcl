[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::swap

```cpp
void swap(sorted_multimap& other) noexcept(std::is_nothrow_swappable_v<key_compare>);      // (1)
template<class Key, class T, class Compare>
void swap(sorted_multimap<Key, T, Compare>& lhs, sorted_multimap<Key, T, Compare>& rhs)    // (2)
    noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of two multimaps: their trees, counts and comparators. No element is touched, and every
iterator keeps pointing at its element, now in the other multimap; `end()` goes with its header.

1. Exchanges the contents of this multimap and `other`.
2. The non-member, declared in `sgcl`: `lhs.swap(rhs)`. `swap(a, b)` written without a namespace finds it by the
   arguments' type.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the multimap to exchange the contents with |
| `lhs`, `rhs` | the multimaps whose contents to exchange |

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
    sorted_multimap<int, int> a = {{1, 1}, {1, 2}}, b = {{2, 2}};
    auto it = a.begin();

    swap(a, b);
    println("{} {}", a, b);
    println("{}", it == b.begin());  // it follows its element into b
}
```

Output:

```text
{2: 2} {1: 1, 1: 2}
true
```

## See also

- [merge](merge.md): relinks the nodes of another map into this one
- [operator=](operator_assign.md): assigns values to the multimap
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
