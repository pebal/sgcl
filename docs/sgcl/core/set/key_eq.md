[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::key_eq

```cpp
key_equal key_eq() const noexcept(std::is_nothrow_copy_constructible_v<key_equal>);
```

Returns a copy of the equality of the keys of the set: the one it was constructed with, or took over by a move
or a swap.

## Parameters

None.

## Return value

A copy of the equality of the keys.

## Complexity

Constant.

## Exceptions

What the copy of `KeyEqual` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Modulo {
    int m = 1;

    size_t operator()(int key) const noexcept { return size_t(key % m); }
    bool operator()(int a, int b) const noexcept { return a % m == b % m; }
};

int main() {
    set<int, Modulo, Modulo> s(8, Modulo{10}, Modulo{10});
    s.insert({3, 13, 4});

    Modulo equal = s.key_eq();
    println("{} {} {}", s.size(), equal.m, equal(3, 23));
}
```

Output:

```text
2 10 true
```

## See also

- [hash_function](hash_function.md): the hash function
- [find](find.md): a lookup by the hash and the equality
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
