[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>);
```

Returns a copy of the function object that compares the keys of the map for equality.

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
    ordered_map<int, int> m;
    auto equal = m.key_eq();
    println("{} {}", equal(1, 1), equal(1, 2));
}
```

Output:

```text
true false
```

## See also

- [hash_function](hash_function.md): a copy of the hash function
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
