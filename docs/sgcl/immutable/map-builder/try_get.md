[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md) › [builder](../map-builder.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder::try_get

```cpp
const T* try_get(const Key& key) const noexcept;                                     // (1)
template<class K> const T* try_get(const K& key) const noexcept(/* see below */);    // (2)
```

Returns a pointer to the value under `key`, or null when the key is absent: the read of `try_get` of a map, on
what the builder holds now. A builder has no iterators and no `find`, since its nodes change under them.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

A pointer to the value, null when there is none.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

- (1) None.
- (2) What `Hash` and `KeyEqual` called with a `K` throw; none when their calls are noexcept, as those of the
  function objects of `std` are taken to be.

## Notes

The pointer is valid until the builder's next change, which may move the element within its node; a value to keep
is copied out first.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> stock = {{"apples", 3}};
    auto b = stock.thaw();
    if (const int* n = b.try_get("apples")) {
        int more = *n + 2;  // copied out before the change
        b.set("apples", more);
    }
    println("{} {}", *b.try_get("apples"), b.try_get("pears") == nullptr);
}
```

Output:

```text
5 true
```

## See also

- [contains](contains.md): checks whether the builder has an element under a key
- [set](set.md): puts a value under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::builder](../map-builder.md)
