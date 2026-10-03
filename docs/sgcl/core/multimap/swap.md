[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::swap

```cpp
void swap(multimap& other)                                                                      // (1)
    noexcept(std::is_nothrow_swappable_v<hasher> && std::is_nothrow_swappable_v<key_equal>);
friend void swap(multimap& lhs, multimap& rhs) noexcept(noexcept(lhs.swap(rhs)));               // (2)
```

Exchanges the contents of two multimaps: their tables, counts, load factors, hashers and equalities. No element is
touched, and every iterator keeps pointing at its element, now in the other multimap.

1. Exchanges the contents of this multimap and `other`.
2. `lhs.swap(rhs)`, as a non-member found by argument-dependent lookup: `using std::swap; swap(a, b);` calls it.

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

What the swap of `Hash` or of `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> a = {{1, "one"}};
    multimap<int, string> b = {{2, "two"}, {3, "three"}};
    auto it = a.begin();

    swap(a, b);  // it still points at {1, "one"}, in b now
    println("{} {} {}", a.size(), b.size(), it == b.find(1));

    a.swap(b);
    println("{} {}", a.count(1), b.count(3));
}
```

Output:

```text
2 1 true
1 1
```

## See also

- [operator=](operator_assign.md): replaces the elements of a multimap
- [merge](merge.md): relinks the nodes of another multimap
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
