[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::operator==, operator!= (sgcl::immutable::map)

```cpp
friend bool operator==(const map& a, const map& b);    // (1)
friend bool operator!=(const map& a, const map& b);    // (2)
```

Compares two maps by their elements.

1. `true` when `a` and `b` have the same keys with equal values, whatever the two share and whatever the order of
   their iteration. Two maps that hold the same root (a version and its copy, two snapshots of one value) are
   equal at once, without a look at an element; otherwise every key of `a` is looked up in `b`.
2. `!(a == b)`.

Both take part only when `T` is [req::equatable](../../core/req/equatable.md); the keys are compared by
`KeyEqual`. A map has no `<=>`: the order of its iteration is the hashes', not a value.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the maps to compare |

## Return value

- (1) `true` when the maps are equal, `false` otherwise.
- (2) `true` when they are not equal, `false` otherwise.

## Complexity

Constant when the two hold the same root or their sizes differ; otherwise a lookup in `b` per element of `a`,
*n* log32(*n*).

## Exceptions

What the comparison of the values throws.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> a = {{"x", 1}, {"y", 2}};
    immutable::map<string, int> b = immutable::map<string, int>().set("y", 2).set("x", 1);
    println("{} {} {}", a == b, a != a.set("x", 3), a == a.erase("z"));
}
```

Output:

```text
true true true
```

## See also

- [find](find.md): the element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
