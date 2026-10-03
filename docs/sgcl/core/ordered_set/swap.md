[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::swap

```cpp
void swap(ordered_set& other)                                                                   // (1)
    noexcept(std::is_nothrow_swappable_v<hasher> && std::is_nothrow_swappable_v<key_equal>);
friend void swap(ordered_set& lhs, ordered_set& rhs) noexcept(noexcept(lhs.swap(rhs)));         // (2)
```

1. Exchanges the contents of this set and `other`: the tables, the orders, the counts, the load factors, the
   hashes and the equalities.
2. `lhs.swap(rhs)`: the non-member `swap`, a hidden friend found by the arguments' type, by `swap(a, b)`
   written without a namespace or after `using std::swap;`.

No element is touched. Every iterator keeps pointing at its element, now in the other set; an `end()` goes with
its sentinel, and is the other set's `end()` after.

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

What the swap of `Hash` or `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> a = {1, 2};
    ordered_set<int> b = {3};
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
{3} {1, 2}
true true
{1, 2} {3}
```

## See also

- [operator=](operator_assign.md): replaces the contents of a set
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
