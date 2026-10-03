[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::contains

```cpp
/*(1)*/ bool contains(const Key& key) const noexcept;
/*(2)*/ template<class K> bool contains(const K& key) const noexcept(/* see below */);
```

Checks whether the map has an element under `key`. It hides the `contains` of
[mixin::enumerable](../../core/mixin/enumerable/contains.md), which would compare every element with a value:
the map asks by the key.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

`true` when the map has an element under the key, `false` otherwise.

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
    immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
    string url = "ftp://host";
    println("{} {}", ports.contains("http"), ports.contains(url.as_slice(0, 3)));  // no string made
}
```

Output:

```text
true false
```

## See also

- [find](find.md): the element under a key, as an iterator
- [count](count.md): the number of elements under a key
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
