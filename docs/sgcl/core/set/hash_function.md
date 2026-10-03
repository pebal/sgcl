[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>);
```

Returns a copy of the hash function of the set: the one it was constructed with, or took over by a move or a
swap.

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
    set<int, Seeded> s(16, Seeded{12345});
    s.insert(1);

    Seeded hash = s.hash_function();
    println("{} {}", hash.seed, hash(1) == Seeded{12345}(1));
}
```

Output:

```text
12345 true
```

## See also

- [key_eq](key_eq.md): the equality of the keys
- [bucket](bucket.md): the bucket a hash falls into
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
