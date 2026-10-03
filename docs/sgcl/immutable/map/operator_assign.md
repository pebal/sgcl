[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::operator=

```cpp
/*(1)*/ map& operator=(const map& other) noexcept;
/*(2)*/ map& operator=(map&& other) noexcept;
```

Makes this variable hold the version `other` holds.

1. Copies the two words of `other` and its function objects; every node is shared.
2. The same as (1): `other` keeps its version.

The version the variable held before is not changed; its nodes are left to the collector once no other version
reaches them. The assignment is how a program moves on to the next version, `m = m.set(k, v)`: the only member
that is not `const`, and the one thing that needs synchronization when other threads read the variable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map whose version is taken |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator taken from this map before the assignment is not valid after it.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> stock = {{"apples", 3}};
    immutable::map<string, int> before = stock;
    stock = stock.set("apples", 2);  // the variable moves on, the old version stays
    println("{} {}", stock.at("apples"), before.at("apples"));
}
```

Output:

```text
2 3
```

## See also

- [(constructor)](map.md): constructs the map
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
