[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](../lookup.md)

# sgcl::mixin::lookup\<Derived\>::contains_key

```cpp
template<class K>
bool contains_key(const K& key) const noexcept(/* see below */);
```

Checks whether the map has a value under `key`: one search, by the map's `find` or by the pointer read of a map
that gives one. `key` may be of any type the map's `find` takes.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

`true` when the map has a value under `key`, `false` otherwise.

## Complexity

One search of the map, the complexity of its `find`: constant on average for `map`, `multimap` and `ordered_map`,
logarithmic for `sorted_map` and `sorted_multimap`, logarithmic base 32 for `immutable::map`.

## Exceptions

What the map's search with a `K` throws: a transparent hash, equality or comparison called with another type than
the key's; none with the key type, whose calls the map requires noexcept.

## Notes

The maps have a `contains(key)` of their own, which asks the same: `contains_key` is the name a function over any
`req::lookup` map can call, as `contains` of [mixin::enumerable](../enumerable.md) asks about an element.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_map<string, int> ports = {{"http", 80}, {"https", 443}};
    println("{} {}", ports.contains_key("https"), ports.contains_key("ftp"));

    immutable::map<int, int> squares = immutable::map<int, int>().insert(3, 9);
    println("{}", squares.contains_key(3));
}
```

Output:

```text
true false
true
```

## See also

- [try_get](try_get.md): a pointer to the value, null when absent
- [sgcl::mixin::lookup\<Derived\>](../lookup.md)
