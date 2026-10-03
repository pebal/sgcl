[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::operator==, operator!= (sgcl::map)

```cpp
friend bool operator==(const map& lhs, const map& rhs);
```

Compares two maps by their elements: `true` when they have the same size and, for every element of `lhs`, an
element of `rhs` with an equal key and an equal value (`value_type == value_type`), whatever their bucket counts
and their orders of iteration. `lhs != rhs` is `!(lhs == rhs)`, the compiler's rewrite of `==`. A map has no
`<=>`: the order of its iteration is the hashes', not a value.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the maps to compare |

## Return value

`true` when the maps are equal, `false` otherwise.

## Complexity

Constant when the sizes differ; otherwise a lookup in `rhs` per element of `lhs`, linear in the size on average.

## Exceptions

What the comparison of the elements throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<int, string> a = {{1, "one"}, {2, "two"}};
    map<int, string> b(1000);
    b.insert({{2, "two"}, {1, "one"}});
    println("{} {}", a == b, a.bucket_count() == b.bucket_count());

    b[2] = "deux";
    println("{}", a != b);
}
```

Output:

```text
true false
true
```

## See also

- [find](find.md): an iterator to the element under a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
