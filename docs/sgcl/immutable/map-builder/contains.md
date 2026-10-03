[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::contains

```cpp
bool contains(const Key& key) const noexcept;                                     // (1)
template<class K> bool contains(const K& key) const noexcept(/* see below */);    // (2)
```

Checks whether the builder holds an element under `key` now.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

`true` when the builder has an element under the key, `false` otherwise.

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
    immutable::map<string, int> m = {{"a", 1}};
    auto b = m.thaw();
    b.erase("a");
    println("{} {}", b.contains("a"), m.contains("a"));
}
```

Output:

```text
false true
```

## See also

- [try_get](try_get.md): a pointer to the value under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
