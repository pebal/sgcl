[sgcl](../../README.md) › [concurrent](../README.md) › [set](README.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::hash_function

```cpp
hasher hash_function() const noexcept(std::is_nothrow_copy_constructible_v<hasher>);
```

Returns a copy of the hash function the set was constructed with.

## Parameters

None.

## Return value

A copy of the hash function.

## Complexity

Constant.

## Exceptions

What the copy of `Hash` throws; none when it is noexcept.

## Notes

The set never changes its hash function, so the call may run concurrently with anything. The bucket of a key is
the low bits of its hash, the hash taken modulo [bucket_count](bucket_count.md): a hash whose low bits do not vary
crowds the keys into a few buckets ([Rules](README.md#rules)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Mixed {
    size_t operator()(long page) const noexcept {
        return size_t(page) * 0x9E3779B97F4A7C15;  // the low bits of an aligned address mixed in
    }
};

int main() {
    concurrent::set<long, Mixed> pages;
    for (int i : range(64)) {
        pages.insert(long(i) * 4096);
    }

    Mixed hash = pages.hash_function();
    println("{} {}", pages.size(), hash(4096) == Mixed{}(4096));
}
```

Output:

```text
64 true
```

## See also

- [key_eq](key_eq.md): the equality of the keys
- [(constructor)](set.md): a set with a hash function of its own
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](README.md)
