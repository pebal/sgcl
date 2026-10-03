[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::operator=

```cpp
set& operator=(const set& other) noexcept;    // (1)
set& operator=(set&& other) noexcept;         // (2)
```

Makes this variable hold the version `other` holds.

1. Copies the two words of `other` and its function objects; every node is shared.
2. The same as (1): `other` keeps its version.

The version the variable held before is not changed; its nodes are left to the collector once no other version
reaches them. The assignment is how a program moves on to the next version, `s = s.insert(x)`: the only member
that is not `const`, and the one thing that needs synchronization when other threads read the variable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set whose version is taken |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int> seen = {1, 2};
    immutable::set<int> before = seen;
    seen = seen.insert(3);  // the variable moves on, the old version stays
    println("{} {}", seen.size(), before.size());
}
```

Output:

```text
3 2
```

## See also

- [(constructor)](set.md): constructs the set
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
