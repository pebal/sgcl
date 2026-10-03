[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::swap

```cpp
void swap(ordered_map& other)                                                                   // (1)
    noexcept(std::is_nothrow_swappable_v<hasher> && std::is_nothrow_swappable_v<key_equal>);
friend void swap(ordered_map& lhs, ordered_map& rhs) noexcept(noexcept(lhs.swap(rhs)));         // (2)
```

1. Exchanges the contents of this map and `other`: the tables, the orders, the counts, the load factors, the
   hashes and the equalities.
2. `lhs.swap(rhs)`: the non-member `swap`, a hidden friend found by the arguments' type, by `swap(a, b)`
   written without a namespace or after `using std::swap;`.

No element is touched. Every iterator keeps pointing at its element, now in the other map; an `end()` goes with
its sentinel, and is the other map's `end()` after.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map to exchange the contents with |
| `lhs`, `rhs` | the maps to exchange the contents of |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the swap of `Hash` or `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, int> a = {{1, 1}, {2, 2}};
    ordered_map<int, int> b = {{3, 3}};
    auto it = a.begin();
    auto stop = a.end();

    swap(a, b);
    println("{} {}", a, b);
    println("{} {}", it == b.find(1), stop == b.end());

    a.swap(b);
    println("{} {}", a, b);
}
```

Output:

```text
{3: 3} {1: 1, 2: 2}
true true
{1: 1, 2: 2} {3: 3}
```

## See also

- [operator=](operator_assign.md): replaces the contents of a map
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
