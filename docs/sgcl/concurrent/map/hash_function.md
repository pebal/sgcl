[sgcl](../../README.md) › [concurrent](../README.md) › [map](../map.md)

# sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>::hash_function

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

What the copy of `Hash` throws; none when it is noexcept.

## Notes

The map never changes its hash function, so the call may run concurrently with anything. The bucket of a key is
the low bits of its hash, the hash taken modulo [bucket_count](bucket_count.md): a hash whose low bits do not vary
crowds the keys into a few buckets ([Rules](../map.md#rules)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Seeded {
    size_t seed = 0;

    size_t operator()(int key) const noexcept {
        return (size_t(key) ^ seed) * 0x9E3779B97F4A7C15;
    }
};

int main() {
    concurrent::map<int, string, Seeded> names(16, Seeded{12345});
    names.try_emplace(1, "Ada");

    Seeded hash = names.hash_function();
    println("{} {}", hash.seed, hash(1) == Seeded{12345}(1));
}
```

Output:

```text
12345 true
```

## See also

- [key_eq](key_eq.md): the equality of the keys
- [(constructor)](map.md): a map with a hash function of its own
- [sgcl::concurrent::map\<Key, T, Hash, KeyEqual\>](../map.md)
