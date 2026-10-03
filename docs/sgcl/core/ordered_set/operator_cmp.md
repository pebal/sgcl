[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::operator==, operator!= (sgcl::ordered_set)

```cpp
friend bool operator==(const ordered_set& lhs, const ordered_set& rhs);
```

Compares the contents of two sets: equal sizes and every element of `lhs` found in `rhs`, found by its hash and
the equality and then compared with `==`. The order is ignored, as `LinkedHashSet.equals` does: two sets with the
same elements are equal whatever the order they came in, and whatever their bucket counts.

`operator==` is a hidden friend, found by the arguments' type. `a != b` is the compiler's rewrite of
`!(a == b)`. There is no ordering: no `<` and no `<=>`.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the sets to compare |

## Return value

`true` when the sets hold the same elements, `false` otherwise.

## Complexity

Linear in `size()` on average: a lookup in `rhs` per element of `lhs`; quadratic in the worst case.

## Exceptions

What the `==` of `Key` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> a = {1, 2};
    ordered_set<int> b(1000);
    b.insert({2, 1});
    println("{} {} {}", a, b, a == b);

    b.insert(3);
    println("{}", a != b);
}
```

Output:

```text
{1, 2} {2, 1} true
true
```

## See also

- [contains](contains.md): checks whether an element is there
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
