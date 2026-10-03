[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<Hash>);
```

Returns a copy of the hash function of the keys, the one the map underneath holds: the `hash` given to the
constructor, or a default-constructed `Hash`.

## Parameters

None.

## Return value

A copy of the hash function.

## Complexity

Constant.

## Exceptions

What the copy of `Hash` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

struct SeededHash {
    size_t seed = 0;

    size_t operator()(int key) const noexcept {
        return std::hash<int>()(key) ^ seed;
    }
};

int main() {
    using Cache = concurrent::cache<int, string, SeededHash>;
    Cache names(10, {}, Cache::DefaultSample, SeededHash{42});
    println("{}", names.hash_function().seed);
}
```

Output:

```text
42
```

## See also

- [key_eq](key_eq.md): a copy of the equality of the keys
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
