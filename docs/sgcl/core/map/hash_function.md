[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>);
```

Returns a copy of the hash function the map was constructed with.

## Parameters

None.

## Return value

A copy of the hash function.

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
    size_t operator()(int x) const noexcept { return size_t(x % 10); }
};

int main() {
    map<int, string, Mod10> m;
    auto hash = m.hash_function();
    println("{} {}", hash(42), hash(42) == hash(2));
}
```

Output:

```text
2 true
```

## See also

- [key_eq](key_eq.md): a copy of the equality of the keys
- [bucket](bucket.md): the bucket of a key
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
