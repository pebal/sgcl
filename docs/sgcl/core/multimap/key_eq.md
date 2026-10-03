[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>);
```

Returns a copy of the equality of the keys the multimap was constructed with.

## Parameters

None.

## Return value

A copy of the equality.

## Complexity

Constant.

## Exceptions

What the copy constructor of `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<string, int> m;
    auto equal = m.key_eq();
    println("{} {}", equal("a", "a"), equal("a", "b"));
}
```

Output:

```text
true false
```

## See also

- [hash_function](hash_function.md): a copy of the hash function
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
