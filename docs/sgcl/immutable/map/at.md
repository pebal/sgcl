[sgcl](../../README.md) › [immutable](../README.md) › [map](../map.md)

# sgcl::immutable::map\<Key, T, Hash, KeyEqual\>::at

```cpp
const T& at(const Key& key) const;                    // (1)
template<class K> const T& at(const K& key) const;    // (2)
```

Returns a reference to the value under `key`, with bounds checking: a key that is absent throws.

1. Looks up `key`.
2. Looks up a key of another type without building a `Key`. Takes part only when `Hash::is_transparent` and
   `KeyEqual::is_transparent` are both types.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the element |

## Return value

A `const` reference to the value.

## Complexity

Logarithmic in `size()`, base 32, and one comparison of keys; one per element of a chain when hashes collide to
the last bit.

## Exceptions

`out_of_range` when the key is absent.

## Notes

The reference is valid while some version holds the node of the element: this map, or any map that shares the
node. `value_or` and `try_get` of [mixin::lookup](../../core/mixin/lookup.md) read without an exception.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{}", ports.at("https"));

    try {
        println("{}", ports.at("ftp"));
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
443
out of range: sgcl::immutable::map::at
```

## See also

- [find](find.md): the element under a key, as an iterator
- [mixin::lookup](../../core/mixin/lookup.md): `get`, `try_get`, `value_or`
- [sgcl::immutable::map\<Key, T, Hash, KeyEqual\>](../map.md)
