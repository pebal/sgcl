[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const;
```

Returns a copy of the equality the set compares its elements by: the one it was constructed with, kept by every
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
    immutable::set<int> s = {1};
    auto equal = s.key_eq();
    println("{} {}", equal(1, 1), equal(1, 2));
}
```

Output:

```text
true false
```

## See also

- [hash_function](hash_function.md): the hash of the elements
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
