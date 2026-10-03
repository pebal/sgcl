[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<KeyEqual>);
```

Returns a copy of the equality of the keys, the one the map underneath holds: the `equal` given to the
constructor, or a default-constructed `KeyEqual`.

## Parameters

None.

## Return value

A copy of the equality.

## Complexity

Constant.

## Exceptions

What the copy of `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<string, int> ages(10);
    auto equal = ages.key_eq();
    println("{} {}", equal("Ada", "Ada"), equal("Ada", "Grace"));
}
```

Output:

```text
true false
```

## See also

- [hash_function](hash_function.md): a copy of the hash function
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
