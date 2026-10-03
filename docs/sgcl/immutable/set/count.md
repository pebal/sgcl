[sgcl](../../README.md) › [immutable](../README.md) › [set](../set.md)

# sgcl::immutable::set\<Key, Hash, KeyEqual\>::count

```cpp
size_type count(const Key& key) const noexcept;                                     // (1)
template<class K> size_type count(const K& key) const noexcept(/* see below */);    // (2)
```

Returns the number of elements equal to `key`: 1 or 0, since a set holds each element once.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

1 or 0.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

- (1) None.
- (2) What `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as those of the
  function objects of `std` are taken to be.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::set<int> s = {1, 2};
    println("{} {}", s.count(1), s.count(3));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether the set has an element equal to a key
- [sgcl::immutable::set\<Key, Hash, KeyEqual\>](../set.md)
