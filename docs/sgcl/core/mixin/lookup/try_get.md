[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [lookup](README.md)

# sgcl::mixin::lookup\<Derived\>::try_get

```cpp
template<class K>
auto try_get(const K& key) noexcept(/* see below */);          // (1)
template<class K>
auto try_get(const K& key) const noexcept(/* see below */);    // (2)
```

Returns a pointer to the value under `key`, in the map, or a null pointer when the map has none: one search, by
the map's `find` or by the pointer read of a map that gives one. The value may be read and, through (1) on a map
whose values are written in place, changed without a second search. `key` may be of any type the map's `find`
takes. On a map with several values per key, the value is the first, the one `find` finds.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key to look up |

## Return value

1. A `mapped_type*` to the value under `key`, or null; a `const mapped_type*` on `immutable::map`, whose values
   are never written.
2. A `const mapped_type*` to the value under `key`, or null.

## Complexity

One search of the map, the complexity of its `find`: constant on average for `map`, `multimap` and `ordered_map`,
logarithmic for `sorted_map` and `sorted_multimap`, logarithmic base 32 for `immutable::map`.

## Exceptions

What the map's search with a `K` throws: a transparent hash, equality or comparison called with another type than
the key's; none with the key type, whose calls the map requires noexcept.

## Notes

The pointer is valid as long as the element: until it is erased or the map is destroyed, as a reference from
`find` is; on `immutable::map`, as long as the version it was read from. It is a plain pointer and does not keep
the element alive.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, int> ports = {{"http", 80}};
    if (int* port = ports.try_get("http")) {
        *port = 8080;  // in place
    }
    println("{} {}", ports.at("http"), ports.try_get("ftp") == nullptr);

    immutable::map<string, int> fixed = immutable::map<string, int>().insert("ssh", 22);
    const int* ssh = fixed.try_get("ssh");
    println("{}", *ssh);
}
```

Output:

```text
8080 true
22
```

## See also

- [get](get.md): a copy of the value in an optional
- [contains_key](contains_key.md): checks whether the map has a value under a key
- [sgcl::mixin::lookup\<Derived\>](README.md)
