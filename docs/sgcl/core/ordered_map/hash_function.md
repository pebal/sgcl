[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>);
```

Returns a copy of the hash function of the map.

## Parameters

None.

## Return value

A copy of the hash.

## Complexity

Constant.

## Exceptions

What the copy constructor of `Hash` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Mod10 {
    size_t operator()(int key) const noexcept {
        return size_t(key % 10);
    }
};

int main() {
    ordered_map<int, int, Mod10> m;
    auto hash = m.hash_function();
    println("{} {}", hash(42), hash(7));
}
```

Output:

```text
2 7
```

## See also

- [key_eq](key_eq.md): a copy of the equality of the keys
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
