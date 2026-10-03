[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::count

```cpp
/*(1)*/ size_type count(const Key& key) const noexcept;
/*(2)*/ template<class K> size_type count(const K& key) const noexcept(/* see below */);
```

Returns the number of elements under `key`: 1 when the key is there, 0 when it is absent, since a map holds one
element per key.

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
    immutable::map<int, string> names = {{1, "one"}, {2, "two"}};
    println("{} {}", names.count(1), names.count(3));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): checks whether the map has an element under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
