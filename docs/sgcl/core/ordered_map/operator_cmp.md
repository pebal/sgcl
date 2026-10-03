[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::operator==, operator!= (sgcl::ordered_map)

```cpp
friend bool operator==(const ordered_map& lhs, const ordered_map& rhs);
```

Compares the contents of two maps: equal sizes and, for every element of `lhs`, an element of `rhs` with an
equal key and an equal value (`value_type == value_type`). The order is ignored, as `LinkedHashMap.equals` does:
two maps with the same pairs are equal whatever the order they came in, and whatever their bucket counts.

`operator==` is a hidden friend, found by the arguments' type. `a != b` is the compiler's rewrite of
`!(a == b)`. There is no ordering: no `<` and no `<=>`.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the maps to compare |

## Return value

`true` when the maps hold the same pairs, `false` otherwise.

## Complexity

Linear in `size()` on average: a lookup in `rhs` per element of `lhs`; quadratic in the worst case.

## Exceptions

What the `==` of `Key` or of `T` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, int> a = {{1, 1}, {2, 2}};
    ordered_map<int, int> b(1000);
    b.insert({{2, 2}, {1, 1}});
    println("{} {} {}", a, b, a == b);

    b[2] = 3;
    println("{}", a != b);
}
```

Output:

```text
{1: 1, 2: 2} {2: 2, 1: 1} true
true
```

## See also

- [find](find.md): an iterator to the element under a key
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
