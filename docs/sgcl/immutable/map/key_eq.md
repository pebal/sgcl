[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const;
```

Returns a copy of the equality the map compares its keys by: the one it was constructed with, kept by every
version made from it.

## Parameters

None.

## Return value

A copy of the equality.

## Complexity

Constant.

## Exceptions

What the copy of `KeyEqual` throws.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> m = {{"a", 1}};
    auto equal = m.key_eq();
    println("{} {}", equal("a", "a"), equal("a", "b"));
}
```

Output:

```text
true false
```

## See also

- [hash_function](hash_function.md): the hash of the keys
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
