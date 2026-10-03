[sgcl](../../README.md) › [core](../README.md) › [multimap](README.md)

# sgcl::operator==, operator!= (sgcl::multimap)

```cpp
friend bool operator==(const multimap& lhs, const multimap& rhs);
```

Compares two multimaps by their elements: `true` when they have the same size and, for every run of equal keys in
`lhs`, a run of the same length in `rhs` that is a permutation of it (the elements compared with
`value_type == value_type`), whatever their bucket counts and their orders of iteration. `lhs != rhs` is
`!(lhs == rhs)`, the compiler's rewrite of `==`. A multimap has no `<=>`: the order of its iteration is the
hashes', not a value.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the multimaps to compare |

## Return value

`true` when the multimaps are equal, `false` otherwise.

## Complexity

Constant when the sizes differ; otherwise a lookup in `rhs` per run of `lhs`, and per run a comparison of every
element with every other of the two runs, quadratic in the length of the run.

## Exceptions

What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, int> a = {{1, 1}, {1, 2}, {2, 5}};
    multimap<int, int> b = {{2, 5}, {1, 2}, {1, 1}};
    println("{}", a == b);

    b.emplace(1, 1);
    a.emplace(1, 2);
    println("{}", a != b);  // the same sizes and keys, other values
}
```

Output:

```text
true
true
```

## See also

- [equal_range](equal_range.md): the run of the elements under a key
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](README.md)
