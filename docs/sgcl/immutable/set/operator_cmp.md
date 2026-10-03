[sgcl](../../README.md) › [immutable](../README.md) › [set](README.md)

# sgcl::immutable::operator==, operator!= (sgcl::immutable::set)

```cpp
friend bool operator==(const set& a, const set& b) noexcept;    // (1)
friend bool operator!=(const set& a, const set& b) noexcept;    // (2)
```

Compares two sets by their elements.

1. `true` when `a` and `b` hold the same elements, whatever the two share and whatever the order of their
   iteration. Two sets that hold the same root are equal at once, without a look at an element; otherwise every
   element of `a` is looked up in `b`, by `KeyEqual`.
2. `!(a == b)`.

A set has no `<=>`: the order of its iteration is the hashes', not a value.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the sets to compare |

## Return value

- (1) `true` when the sets are equal, `false` otherwise.
- (2) `true` when they are not equal, `false` otherwise.

## Complexity

Constant when the two hold the same root or their sizes differ; otherwise a lookup in `b` per element of `a`,
*n* log32(*n*).

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int> a = {1, 2, 3};
    immutable::set<int> b = immutable::set<int>().insert(3).insert(2).insert(1);
    println("{} {} {}", a == b, a != a.erase(2), a == a.insert(1));
}
```

Output:

```text
true true true
```

## See also

- [find](find.md): the element equal to a key
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](README.md)
